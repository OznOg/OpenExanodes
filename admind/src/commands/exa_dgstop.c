/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>
#include <string.h>

#ifdef WITH_FS
#include "admind/include/service_fs_commands.h"
#endif

#include "admind/include/service_lum.h"
#include "admind/include/service_vrt.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admindstate.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "log/include/log.h"
#include "os/include/strlcpy.h"
#include "vrt/virtualiseur/include/vrt_client.h"

/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct dgstop_info
{
  exa_uuid_t group_uuid;
  uint32_t   goal_change;
  uint32_t   force;
  uint32_t   print_warning;
  uint32_t   pad;
};

__export(EXA_ADM_DGSTOP) struct dgstop_params
  {
    char groupname[EXA_MAXSIZE_GROUPNAME + 1];
    __optional bool recursive __default(false);
    __optional bool force __default(false);
  };

static int
vrt_master_group_stop(admwrk_ctx_t *ctx, struct adm_group *group,
                      int force, adm_goal_change_t goal_change,
                      bool print_warning)
{
  struct adm_volume *volume;
  struct dgstop_info info;
  exa_nodeset_t all_nodes;
  int ret;

  adm_nodeset_set_all(&all_nodes); /* get all nodes list */

  memset(&info, 0, sizeof(info));

#ifdef WITH_FS
  ret = fs_stop_all_fs(ctx, group, NULL, force, goal_change);
  if (ret != EXA_SUCCESS)
    return ret;
#endif

  adm_group_for_each_volume(group, volume)
  {
      ret = lum_master_export_unpublish(ctx, &volume->uuid, &all_nodes, force);
      if (ret != EXA_SUCCESS)
          return ret;

      ret = vrt_master_volume_stop(ctx, volume, &all_nodes, force,
                                   goal_change, false /* print_warning */);
      if (ret != EXA_SUCCESS)
          return ret;
  }

  uuid_copy(&info.group_uuid, &group->uuid);
  info.goal_change = goal_change;
  info.force       = force;
  info.print_warning = print_warning;
  return admwrk_exec_command(ctx, &adm_service_admin, RPC_ADM_DGSTOP,
			     &info, sizeof(info));
}

static void
cluster_dgstop(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct dgstop_params *params = data;
  struct adm_group *group;
  adm_goal_change_t goal_change;
  int error_val;

  exalog_info("received dgstop '%s'%s%s from %s",
	      params->groupname,
	      params->recursive ? " --recursive" : "",
	      params->force ? " --force" : "", adm_cli_ip());

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  if (adm_cluster.goal != ADM_CLUSTER_GOAL_STARTED)
    {
      set_error(err_desc, -EXA_ERR_ADMIND_STOPPED, NULL);
      return;
    }

  group = adm_group_get_group_by_name(params->groupname);
  if (group == NULL)
  {
    set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME,
	      "Group '%s' not found", params->groupname);
    return;
  }

  goal_change = ADM_GOAL_CHANGE_GROUP;
  if (params->recursive)
      goal_change |= ADM_GOAL_CHANGE_VOLUME;

  error_val = vrt_master_group_stop(ctx, group, params->force,
                                    goal_change,
                                    true /* print_warning */);
  set_error(err_desc, error_val, NULL);
}


static void
local_exa_dgstop(admwrk_ctx_t *ctx, void *msg)
{
  int ret, barrier_ret;
  struct adm_group *group;
  struct dgstop_info *info = msg;

  /* Group existence has already been verified by the cluster
     command */
  group = adm_group_get_group_by_uuid(&info->group_uuid);
  EXA_ASSERT(group);

  /* Check the group is valid */

  if (!info->force && !group->committed)
    ret = -ADMIND_ERR_RESOURCE_IS_INVALID;
  else
    ret = EXA_SUCCESS;
  barrier_ret = admwrk_barrier(ctx, ret, "Checking if group is valid");
  if (barrier_ret != EXA_SUCCESS)
    goto local_exa_dgstop_end;

  /* Check the group has no started volumes and send a warning if the it is
   * already stopped */

  if (group->started)
    ret = vrt_client_group_stoppable(adm_wt_get_localmb(), &group->uuid);
  else if (info->print_warning)
    ret = -VRT_INFO_GROUP_ALREADY_STOPPED;
  else
    ret = EXA_SUCCESS;
  barrier_ret = admwrk_barrier(ctx, ret, "Checking if group is stoppable");
  if (barrier_ret == -VRT_INFO_GROUP_ALREADY_STOPPED)
    barrier_ret = EXA_SUCCESS;
  if (!info->force && barrier_ret != EXA_SUCCESS)
    goto local_exa_dgstop_end;

  /* Change and sync the goal if required */

  if (info->goal_change & ADM_GOAL_CHANGE_GROUP)
    group->goal = ADM_GROUP_GOAL_STOPPED;

  ret = conf_save_synchronous();
  barrier_ret = admwrk_barrier(ctx, ret, "Updating configuration");
  if (!info->force && barrier_ret != EXA_SUCCESS)
  {
    /* Reset the goal to its former value. */
    if (info->goal_change & ADM_GOAL_CHANGE_GROUP)
      group->goal = ADM_GROUP_GOAL_STARTED;
    goto local_exa_dgstop_end;
  }

  /* Stop the group in VRT if needed */
  barrier_ret = service_vrt_group_stop(group, info->force);

local_exa_dgstop_end:

  admwrk_ack(ctx, barrier_ret);
}

/**
 * Definition of the dgstop command.
 */
const AdmCommand exa_dgstop = {
  .code            = EXA_ADM_DGSTOP,
  .msg             = "dgstop",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_dgstop,
  .local_commands  = {
    { RPC_ADM_DGSTOP, local_exa_dgstop },
    { RPC_COMMAND_NULL, NULL }
  }
};
