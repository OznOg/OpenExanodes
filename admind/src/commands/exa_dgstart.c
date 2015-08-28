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
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/admindstate.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/instance.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "log/include/log.h"
#include "nbd/service/include/nbdservice_client.h"
#include "os/include/strlcpy.h"
#include "vrt/virtualiseur/include/vrt_client.h"

#ifdef WITH_FS
#include "admind/include/service_fs_commands.h"
#endif

__export(EXA_ADM_DGSTART) struct dgstart_params
  {
    char groupname[EXA_MAXSIZE_GROUPNAME + 1];
  };

static int vrt_master_group_start(int thr_nb, struct adm_group *group)
{
  int ret;

  /* Start the disk group. */
  ret = admwrk_exec_command(thr_nb, &adm_service_admin, RPC_ADM_DGSTART,
                            &group->uuid, sizeof(exa_uuid_t));
  if (ret == EXA_SUCCESS)
    {
      /* Start all volumes in the group whose goal is STARTED. */
      ret = vrt_master_volume_start_all(thr_nb, group);

#ifdef WITH_FS
      /* Start all file systems in the group whose goal is STARTED. */
      ret = fs_start_all_fs(thr_nb, group);
#endif
    }

  return ret;
}

static void
cluster_dgstart(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct dgstart_params *params = data;
  struct adm_group *group;
  int error_val;

  exalog_info("received dgstart '%s' from %s", params->groupname,
	      adm_cli_ip());

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  if (adm_cluster.goal != ADM_CLUSTER_GOAL_STARTED)
    {
      set_error(err_desc, -EXA_ERR_ADMIND_STOPPED, NULL);
      exalog_debug("Exanodes Not started : return static informations");
      return;
    }

  group = adm_group_get_group_by_name(params->groupname);
  if (group == NULL)
  {
    set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME,
	      "Group '%s' not found", params->groupname);
    return;
  }

  error_val = vrt_master_group_start(thr_nb, group);

  if (error_val != EXA_SUCCESS)
    {
      if (error_val == -VRT_WARN_GROUP_OFFLINE)
	set_error(err_desc, error_val, NULL);
      else
	set_error(err_desc, error_val, "%s", exa_error_msg(error_val));
    }
  else
    set_success(err_desc);
}

static void
local_exa_dgstart (int thr_nb, void *msg)
{
  int ret, barrier_ret;
  exa_uuid_t *group_uuid = msg;
  struct adm_group *group;

  /* group must exists, it has already been verified during the
     cluster command */
  group = adm_group_get_group_by_uuid(group_uuid);
  EXA_ASSERT(group);

  if (!group->committed)
    ret = -ADMIND_ERR_RESOURCE_IS_INVALID;
  else
    ret = EXA_SUCCESS;
  barrier_ret = admwrk_barrier(thr_nb, ret, "Checking if group is valid");
  if (barrier_ret != EXA_SUCCESS)
    goto local_exa_dgstart_end;

  group->goal = ADM_GROUP_GOAL_STARTED;
  ret = conf_save_synchronous();
  barrier_ret = admwrk_barrier(thr_nb, ret, "Saving configuration");
  if (barrier_ret != EXA_SUCCESS)
  {
    group->goal = ADM_GROUP_GOAL_STOPPED;
    goto local_exa_dgstart_end;
  }

  ret = local_exa_dgstart_vrt_start (thr_nb, group);
  barrier_ret = admwrk_barrier(thr_nb, ret, "Starting group");
  if (barrier_ret == -VRT_INFO_GROUP_ALREADY_STARTED)
  {
    barrier_ret = EXA_SUCCESS;
    goto local_exa_dgstart_end;
  }
  if (barrier_ret != EXA_SUCCESS)
    goto local_exa_dgstart_end;

  ret = vrt_client_group_compute_status (adm_wt_get_localmb(), group_uuid);
  if (ret == -VRT_WARN_GROUP_OFFLINE)
  {
    group->offline = true;
    ret = EXA_SUCCESS;
  }
  barrier_ret = admwrk_barrier(thr_nb, ret, "Compute status");
  if (barrier_ret != EXA_SUCCESS)
    goto local_exa_dgstart_end;

  if (!group->offline)
  {
      exa_nodeset_t all_nodes;
      adm_nodeset_set_all(&all_nodes);

      ret = vrt_client_group_resync(adm_wt_get_localmb(), &group->uuid, &all_nodes);
      barrier_ret = admwrk_barrier(thr_nb, ret, "Resyncing group");
      if (barrier_ret != EXA_SUCCESS)
          goto local_exa_dgstart_end;

      ret = vrt_client_group_post_resync(adm_wt_get_localmb(), group_uuid);
      barrier_ret = admwrk_barrier(thr_nb, ret, "Post-resyncing group");
      if (barrier_ret != EXA_SUCCESS)
          goto local_exa_dgstart_end;

      ret = adm_vrt_group_sync_sb(thr_nb, group);
      barrier_ret = admwrk_barrier(thr_nb, ret, "Syncing superblocks");
      if (barrier_ret != EXA_SUCCESS)
          goto local_exa_dgstart_end;
  }

  ret = vrt_client_group_resume(adm_wt_get_localmb(), group_uuid);
  barrier_ret = admwrk_barrier(thr_nb, ret, "Resuming group");
  if (barrier_ret != EXA_SUCCESS)
    goto local_exa_dgstart_end;

  if (group->offline)
  {
      barrier_ret = -VRT_WARN_GROUP_OFFLINE;
      group->synched = false;
  }
  else
      group->synched = true;

 local_exa_dgstart_end:
  admwrk_ack(thr_nb, barrier_ret);
}

/**
 * Definition of the dgstart command.
 */
const AdmCommand exa_dgstart = {
  .code            = EXA_ADM_DGSTART,
  .msg             = "dgstart",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_dgstart,
  .local_commands  = {
    { RPC_ADM_DGSTART, local_exa_dgstart },
    { RPC_COMMAND_NULL, NULL }
  }
};
