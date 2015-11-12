/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>

#include "admind/include/service_vrt.h"
#include "admind/include/service_lum.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/admindstate.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "nbd/service/include/nbdservice_client.h"
#include "vrt/virtualiseur/include/vrt_client.h"

/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct dgdelete_info
{
  exa_uuid_t group_uuid;
  uint32_t   force;
};

__export(EXA_ADM_DGDELETE) struct dgdelete_params
  {
    char groupname[EXA_MAXSIZE_GROUPNAME + 1];
    __optional bool force __default(false);
    __optional bool recursive __default(false);
  };



/**
 * dgdelete cluster command
 */
static void cluster_dgdelete(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct dgdelete_params *params = data;
  struct adm_group *group;
  int error_val;
  struct dgdelete_info info;

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  group = adm_group_get_group_by_name(params->groupname);
  if (group == NULL)
  {
    set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME,
	      "Group '%s' not found", params->groupname);
    return;
  }

  exalog_info("received dgdelete '%s'%s%s from %s",
	      params->groupname, params->force ? " --force" : "",
	      params->recursive ? " --recursive" : "",
	      adm_cli_ip());

  if (group->goal == ADM_GROUP_GOAL_STARTED)
  {
    set_error(err_desc, -VRT_ERR_GROUP_NOT_STOPPED, NULL);
    return;
  }

  if (!params->recursive && (adm_group_nb_volumes(group) > 0))
    {
      set_error(err_desc, -ADMIND_ERR_GROUP_NOT_EMPTY, NULL);
      error_val = -ADMIND_ERR_GROUP_NOT_EMPTY;
      return;
    }

  /* We have all the params, do the actual work */
  uuid_copy(& info.group_uuid, & group->uuid);
  info.force             = params->force;

  error_val = admwrk_exec_command(thr_nb, &adm_service_admin, RPC_ADM_DGDELETE,
				  &info, sizeof(info));
  if (error_val != EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }

  set_success(err_desc);
}


static void remove_exports_in_group(const struct adm_group *group)
{
    bool changed = false;
    struct adm_volume *volume;

    adm_group_for_each_volume(group, volume)
    {
        changed = true;
        lum_exports_remove_export_from_uuid(&volume->uuid);
    }

    if (changed)
    {
        lum_exports_increment_version();
        lum_serialize_exports();
    }
}

static void
local_exa_dgdelete (int thr_nb, void *msg)
{
  int ret, barrier_ret;
  struct adm_group *group;
  struct dgdelete_info *info = msg;

  /* Group existence has been verified by the cluster command */
  group = adm_group_get_group_by_uuid(& info->group_uuid);
  EXA_ASSERT(group);

  group->committed = false;

  ret = conf_save_synchronous();
  barrier_ret = admwrk_barrier(thr_nb, ret, "Saving configuration file");
  if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
    goto metadata_corruption;
  else if (barrier_ret != EXA_SUCCESS)
    goto local_exa_dgdelete_end;

  /* FIXME We should use vrt_master_volume_delete() so that volumes are
           properly deleted using the very same code path as when deleted
           with commands. */
  remove_exports_in_group(group);

  adm_group_remove_group(group);
  adm_group_cleanup_group(group);
  sb_version_delete(group->sb_version);
  adm_group_free(group);

  ret = conf_save_synchronous();
  EXA_ASSERT(ret == EXA_SUCCESS);
  barrier_ret = admwrk_barrier(thr_nb, ret, "Saving configuration file group");
  if (barrier_ret != EXA_SUCCESS)
    goto local_exa_dgdelete_end;

  goto local_exa_dgdelete_end;

metadata_corruption:
  ret = -ADMIND_ERR_METADATA_CORRUPTION;

 local_exa_dgdelete_end:
  admwrk_ack(thr_nb, ret);
}

/**
 * Definition of the dgdelete command.
 */
const AdmCommand exa_dgdelete = {
  .code            = EXA_ADM_DGDELETE,
  .msg             = "dgdelete",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_dgdelete,
  .local_commands = {
    { RPC_ADM_DGDELETE, local_exa_dgdelete },
    { RPC_COMMAND_NULL, NULL }
  }
};
