/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "admind/src/commands/exa_fscommon.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_fs.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/instance.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"

__export(EXA_ADM_FSCHECK) struct fscheck_params
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  char host_name[EXA_MAXSIZE_HOSTNAME + 1];
  char options[128];		/* FIXME */
  bool repair;
};


/** \brief Implements the fsstart command
 *
 * - Check the file system transaction. It should be "COMMITTED".
 * - Start it on the specified nodes, using FS specific function.
 * - Update the XML tree for a specific filesystem name, setting nodes goal to started.
 */
static void
cluster_fscheck(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct fscheck_params *params = data;
  exa_nodeset_t nodes_down;
  exa_nodeid_t node_id;
  fs_data_t check_fs;
  fs_definition_t *fs_definition;
  int error_val;

  /* Log this command */
  exalog_info("received fscheck '%s:%s' --parameters='%s' --node=%s%s from %s",
	      params->group_name, params->volume_name, params->options, params->host_name,
	      params->repair ? " --repair" : "",
	      adm_cli_ip());

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  /* Get FS */
  if ((error_val = fscommands_params_get_fs(params->group_name,
	                                    params->volume_name,
					    &check_fs,
					    &fs_definition,
					    true)) != EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }

  if (params->host_name[0] == '\0')
    node_id = adm_my_id;
  else
    node_id = adm_nodeid_from_name(params->host_name);

  /* Check it can be used and started. */
  EXA_ASSERT(fs_definition->check_before_start);
  error_val = fs_definition->check_before_start(ctx, &check_fs);
  if (error_val != EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }

  /* If node where to run is not part of the cluster, send an error */
  inst_get_nodes_down(&adm_service_admin, &nodes_down);
  if (exa_nodeset_contains(&nodes_down, node_id))
    {
      set_error(err_desc, -FS_ERR_CHECK_NODE_DOWN, NULL);
      return;
    }

  /* Run the real check */
  EXA_ASSERT(fs_definition->check_fs);
  error_val = fs_definition->check_fs(ctx, &check_fs, params->options,
				      node_id, params->repair);

  set_error(err_desc, error_val, NULL);
}


/**
 * Definition of the exa_fscheck command.
 */
const AdmCommand exa_fscheck = {
  .code            = EXA_ADM_FSCHECK,
  .msg             = "fscheck",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_fscheck,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};


