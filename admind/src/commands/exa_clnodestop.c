/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>

#include "admind/include/service_vrt.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr_mship.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "admind/src/admindstate.h"
#include "vrt/virtualiseur/include/vrt_client.h"
#include "log/include/log.h"

#include "os/include/strlcpy.h"

__export(EXA_ADM_CLNODESTOP) struct clnodestop_params
  {
    char node_names[EXA_MAXSIZE_HOSTSLIST + 1];
    __optional bool force __default(false);
    __optional bool ignore_offline __default(false);
    __optional bool recursive __default(false);
  };


static void
cluster_clnodestop(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct clnodestop_params *params = data;
  int ret;
  struct adm_group *group;
  exa_nodeset_t nodes_to_stop, all_nodes;
  stop_data_t stop_data;

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  adm_nodeset_set_all(&all_nodes);

  ret = adm_nodeset_from_names(&nodes_to_stop, params->node_names);
  if (ret != EXA_SUCCESS)
  {
      set_error(err_desc, ret, NULL);
      return;
  }

  exalog_info("received clnodestop --nodes '%s'%s%s%s from %s",
	      params->node_names,
	      exa_nodeset_equals(&nodes_to_stop, &all_nodes) ?
	           "(all)": "",
              params->force ? " --force" : "",
              params->recursive ? " --recursive" : "",
              adm_cli_ip());

  if (params->recursive
      && !exa_nodeset_equals(&nodes_to_stop, &all_nodes))
  {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM,
	        "The recursive option is authorized only when stopping all nodes");
      return;
  }
  /* Check that we won't lose the quorum of nodes when stopping
   * the given nodes. Do not perform this check if this node is starting.
   */
  /* FIXME: There is a potential race when invoking adm_get_state()
   * because Admind's state can change right after (ie. go STARTED).
   */
  if (!params->force && adm_get_state() != ADMIND_STARTING)
    {
      exa_nodeset_t remaining_nodes;
      exa_nodeset_copy(&remaining_nodes, evmgr_mship());
      exa_nodeset_substract(&remaining_nodes, &nodes_to_stop);

      /* If after the nodestop there would remain some nodes but not enough
       * to have a quorum, the command is refused */
      if (!exa_nodeset_is_empty(&remaining_nodes)
          && !evmgr_mship_may_have_quorum(&remaining_nodes))
        {
	  set_error(err_desc, -ADMIND_ERR_QUORUM_PRESERVE, NULL);
	  return;
        }
    }

  adm_group_for_each_group(group)
  {
      /* No need to check that the group is going offline when stopping all nodes */
      if (group->started && !exa_nodeset_equals(&nodes_to_stop, &all_nodes)
	  && !params->ignore_offline)
      {
	  ret = vrt_client_group_going_offline(adm_wt_get_localmb(),
                                               &group->uuid, &nodes_to_stop);
	  if (ret != EXA_SUCCESS)
	  {
	      set_error(err_desc, ret, NULL);
	      return;
	  }
      }
  }

  exa_nodeset_copy(&stop_data.nodes_to_stop, &nodes_to_stop);
  stop_data.force       = params->force;
  stop_data.goal_change = params->recursive;
  adm_hierarchy_run_stop(ctx, &stop_data, err_desc);

  /* The nodestop failed; the cluster may be in a inconsistant status
   * (fe some instances stopped, some other not) so we trigger a recovery
   * to go back in a correct state */
  if (err_desc->code != EXA_SUCCESS)
      evmgr_request_recovery(adm_wt_get_inboxmb());

  /* when forced, never fails */
  if (params->force)
    set_success(err_desc);
}

const AdmCommand exa_clnodestop = {
  .code            = EXA_ADM_CLNODESTOP,
  .msg             = "clnodestop",
  .accepted_status = ADMIND_STARTING | ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_clnodestop,
  .allowed_in_recovery = true,
  .local_commands = {
    { RPC_COMMAND_NULL, NULL }
  }
};
