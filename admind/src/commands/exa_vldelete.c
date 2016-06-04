/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "admind/include/service_lum.h"
#include "admind/include/service_vrt.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "vrt/virtualiseur/include/vrt_client.h"

__export(EXA_ADM_VLDELETE) struct vldelete_params
  {
    char group_name[EXA_MAXSIZE_GROUPNAME + 1];
    char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
    __optional bool no_fs_check __default(false);
    __optional bool metadata_recovery __default(false);
  };

/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct vldelete_info
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  uint32_t metadata_recovery;
  uint32_t pad;
};


/** \brief Implements the vldelete command
 */
static void
cluster_vldelete(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct vldelete_params *params = data;
  struct adm_group *group;
  struct adm_volume *volume;
  int error_val;

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  group = adm_group_get_group_by_name(params->group_name);
  if (group == NULL)
  {
    set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME, "Group '%s' not found",
	      params->group_name);
    return;
  }

  volume = adm_group_get_volume_by_name(group, params->volume_name);
  if (volume == NULL)
  {
    set_error(err_desc, -ADMIND_ERR_UNKNOWN_VOLUMENAME, "Volume '%s' not found",
	      params->volume_name);
    return;
  }

  /* Check if the volume is not part of a file system */
  if (!params->no_fs_check && adm_volume_is_in_fs(volume))
  {
    set_error(err_desc, -ADMIND_ERR_VOLUME_IN_FS,
	      "volume '%s:%s' is managed by the file system layer",
	      volume->group->name, volume->name);
    return;
  }

  exalog_info("received vldelete '%s:%s'%s%s from %s",
	      params->group_name, params->volume_name,
	      params->no_fs_check ? " --nofscheck" : "",
	      params->metadata_recovery ? " --metadata_recovery" : "",
	      adm_cli_ip());

  /* ask to delete the volume */
  error_val = vrt_master_volume_delete(ctx, volume, params->metadata_recovery);
  set_error(err_desc, error_val, NULL);

  exalog_debug("vldelete clustered command is complete %d", error_val);
}

int
vrt_master_volume_delete (admwrk_ctx_t *ctx, struct adm_volume *volume,
                          bool metadata_recovery)
{
  int ret;
  int reply_ret;
  admwrk_request_t handle;
  struct vldelete_info info;
  memset(&info, 0, sizeof(info));

  EXA_ASSERT(volume != NULL);

  /* Do not continue if the group is not started */
  if (volume->group->goal == ADM_GROUP_GOAL_STOPPED)
    return -VRT_ERR_GROUP_NOT_STARTED;

  ret = exa_nodeset_count(&volume->goal_started);
  if (ret > 0)
    {
      exalog_error("Cannot delete the volume '%s:%s' because it is not stopped on %d nodes",
		   volume->group->name, volume->name, ret);
      return -VRT_ERR_VOLUME_NOT_STOPPED;
    }

  strlcpy(info.group_name, volume->group->name, EXA_MAXSIZE_GROUPNAME + 1);
  strlcpy(info.volume_name, volume->name, EXA_MAXSIZE_VOLUMENAME + 1);
  info.metadata_recovery = metadata_recovery;

  admwrk_run_command(ctx, &adm_service_admin, &handle, RPC_ADM_VLDELETE, &info, sizeof(info));
  /* Examine replies in order to filter return values.
   * The priority of return values is the following (in descending order):
   * o ADMIND_ERR_METADATA_CORRUPTION
   * o ADMIND_ERR_NODE_DOWN
   * o other errors
   */
  ret = EXA_SUCCESS;
  while (admwrk_get_ack(&handle, NULL, &reply_ret))
  {
    if (reply_ret == -ADMIND_ERR_METADATA_CORRUPTION)
      ret = -ADMIND_ERR_METADATA_CORRUPTION;
    else if (reply_ret == -ADMIND_ERR_NODE_DOWN &&
             ret != -ADMIND_ERR_METADATA_CORRUPTION)
      ret = -ADMIND_ERR_NODE_DOWN;
    else if (reply_ret != EXA_SUCCESS &&
             reply_ret != -ADMIND_ERR_NOTHINGTODO &&
             ret != -ADMIND_ERR_METADATA_CORRUPTION &&
             ret != -ADMIND_ERR_NODE_DOWN)
      ret = reply_ret;
  }

  return ret;
}

static void
local_exa_vldelete (admwrk_ctx_t *ctx, void *msg)
{
  struct adm_group *group;
  struct adm_volume *volume = NULL;
  int ret;         /* used for local function calls */
  int barrier_ret; /* used for barriers return values */
  int undo_ret;
  struct vldelete_info *info = msg;

  group = adm_group_get_group_by_name(info->group_name);
  if (group == NULL)
  {
    ret = -ADMIND_ERR_UNKNOWN_GROUPNAME;
    goto get_barrier;
  }

  volume = adm_group_get_volume_by_name(group, info->volume_name);
  if (volume == NULL)
  {
    ret = -ADMIND_ERR_UNKNOWN_VOLUMENAME;
    goto get_barrier;
  }

get_barrier:
  /*** Barrier: getting parameters ***/
  ret = EXA_SUCCESS;
  barrier_ret = admwrk_barrier(ctx, ret, "Getting parameters");
  if (barrier_ret != EXA_SUCCESS)
    goto local_exa_vldelete_end_no_resume;

  ret = vrt_group_suspend_threads_barrier(ctx, &group->uuid);
  if (ret != EXA_SUCCESS)
      goto local_exa_vldelete_end;

  /*** Action: mark the transaction as in-progress ***/
  /* This is an in-memory operation, we assume it won't fail */
  volume->committed = false;
  ret = conf_save_synchronous();
  EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "%s", exa_error_msg(ret));

  /*** Barrier: mark the transaction as in-progress ***/
  barrier_ret = admwrk_barrier(ctx, ret, "Marking transaction as in-progress");
  if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
    goto metadata_corruption;
  else if (barrier_ret != EXA_SUCCESS)
    goto local_exa_vldelete_end;

  /* XXX should errors be handled ? */
  lum_exports_remove_export_from_uuid(&volume->uuid);
  lum_exports_increment_version();
  lum_serialize_exports();

  /*** Action: delete the volume (in memory) through the VRT API ***/
  ret = vrt_client_volume_delete(adm_wt_get_localmb(), &group->uuid, &volume->uuid);

  /*** Barrier: delete the volume through the VRT API ***/
  barrier_ret = admwrk_barrier(ctx, ret, "Deleting volume");
  if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
    goto metadata_corruption;
  else if (barrier_ret == -VRT_ERR_GROUP_NOT_ADMINISTRABLE)
    {
      /* Mark the transaction as committed, so that the volume is not
       * shown as "invalid" later.
       */
      volume->committed = true;
      undo_ret = conf_save_synchronous();
      EXA_ASSERT_VERBOSE(undo_ret == EXA_SUCCESS, "%s", exa_error_msg(undo_ret));
      goto local_exa_vldelete_end;
    }
  else if ((barrier_ret != EXA_SUCCESS) && !info->metadata_recovery)
    goto local_exa_vldelete_end;

  /*** Action: group sync SB (master) ***/
  ret = adm_vrt_group_sync_sb(ctx, group);

  /*** Barrier: group sync SB ***/
  barrier_ret = admwrk_barrier(ctx, ret,
			       "Syncing metadata on disk");
  if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
    goto metadata_corruption;
  else if (barrier_ret != EXA_SUCCESS)
    goto local_exa_vldelete_end;

  /* Delete the volume from the configuration */
  adm_group_remove_volume(volume);
  adm_volume_free(volume);
  ret = conf_save_synchronous();
  EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "%s", exa_error_msg(ret));

  barrier_ret = admwrk_barrier(ctx, ret, "Updating XML configuration");
  if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
    goto metadata_corruption;

  goto local_exa_vldelete_end;

metadata_corruption:
  ret = -ADMIND_ERR_METADATA_CORRUPTION;

local_exa_vldelete_end:
    barrier_ret = vrt_group_resume_threads_barrier(ctx, &group->uuid);
    /* What to do if that fails... I don't know. */
    if (barrier_ret != 0)
        ret = barrier_ret;

local_exa_vldelete_end_no_resume:
  exalog_debug("local_exa_vldelete() = %s", exa_error_msg(ret));
  admwrk_ack(ctx, ret);
}

/**
 * Definition of the vldelete command.
 */
const AdmCommand exa_vldelete = {
  .code            = EXA_ADM_VLDELETE,
  .msg             = "vldelete",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_vldelete,
  .local_commands  =
  {
    { RPC_ADM_VLDELETE, local_exa_vldelete },
    { RPC_COMMAND_NULL, NULL }
  }
};

