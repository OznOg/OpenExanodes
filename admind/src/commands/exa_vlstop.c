/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "admind/include/service_lum.h"
#include "admind/include/service_vrt.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admind.h"
#include "admind/src/deviceblocks.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/exa_nodeset.h"
#include "log/include/log.h"
#include "os/include/os_file.h"
#include "os/include/strlcpy.h"
#include "vrt/virtualiseur/include/vrt_client.h"

__export(EXA_ADM_VLSTOP) struct vlstop_params
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  char host_list[EXA_MAXSIZE_HOSTSLIST + 1];
  __optional bool no_fs_check __default(false);
  __optional bool force __default(false);
};

/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct vlstop_info
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  exa_nodeset_t nodelist;
  adm_goal_change_t goal_change;
  bool force;
  bool print_warning;
};


/** \brief Implements the vlstop command
 */
static void
cluster_vlstop(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct vlstop_params *params = data;
  struct adm_group *group;
  struct adm_volume *volume;
  exa_nodeset_t nodelist;
  int error_val;
  bool allnodes;

  /* An empty hostlist means all nodes */
  allnodes = params->host_list[0] == '\0';

  exalog_info("received vlstop '%s:%s' hosts '%s'%s%s from %s",
	      params->group_name, params->volume_name,
	      params->host_list[0] == '\0' ? "all" : params->host_list,
              params->no_fs_check ? " --nofscheck" : "",
              params->force ? " --force" : "",
              adm_cli_ip());

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  group = adm_group_get_group_by_name(params->group_name);
  if (group == NULL)
    {
      set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME,
		"Group '%s' not found", params->group_name);
      return;
    }

  volume = adm_group_get_volume_by_name(group, params->volume_name);
  if (volume == NULL)
    {
      set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME,
		"Volume '%s' not found", params->volume_name);
      return;
    }

  /* Check if the volume is not part of a file system */
  if (!params->no_fs_check && adm_volume_is_in_fs(volume))
    {
      set_error(err_desc, -ADMIND_ERR_VOLUME_IN_FS, NULL);
      return;
    }

  if (lum_exports_get_type_by_uuid(&volume->uuid) == EXPORT_ISCSI
      && !allnodes)
  {
      set_error(err_desc, -EXA_ERR_INVALID_VALUE,
	        "iSCSI volumes must be stopped on all nodes.");
      return;
  }

   /* Do not continue if the group is not started */
  if (!params->force && !volume->group->started)
    {
      set_error(err_desc, -VRT_ERR_GROUP_NOT_STARTED, NULL);
      return;
    }

  if (allnodes)
    adm_nodeset_set_all(&nodelist);
  else
    {
      int r = adm_nodeset_from_names(&nodelist, params->host_list);
      if (r != EXA_SUCCESS)
	{
	  set_error(err_desc, r, NULL);
	  return;
	}
    }

  error_val = lum_master_export_unpublish(thr_nb, &volume->uuid, &nodelist, params->force);
  if (error_val == EXA_SUCCESS)
      error_val = vrt_master_volume_stop(thr_nb, volume, &nodelist, params->force,
                                         ADM_GOAL_CHANGE_VOLUME, true /* print_warning */);
  set_error(err_desc, error_val, NULL);

  exalog_debug("vlstop clustered command is complete %d", error_val);
}

/* FIXME Should probably be moved to service_vrt.c */
int
vrt_master_volume_stop(int thr_nb, struct adm_volume *volume,
                       const exa_nodeset_t *nodelist, bool force,
                       adm_goal_change_t goal_change, bool print_warning)
{
  struct vlstop_info info;

  EXA_ASSERT(volume != NULL);

  memset(&info, 0, sizeof(info));

  /* Create the message for the local command */
  strlcpy(info.group_name, volume->group->name, EXA_MAXSIZE_GROUPNAME + 1);
  strlcpy(info.volume_name, volume->name, EXA_MAXSIZE_VOLUMENAME + 1);
  exa_nodeset_copy(&info.nodelist, nodelist);
  info.goal_change = goal_change;
  info.force = force;
  info.print_warning = print_warning;

  return admwrk_exec_command(thr_nb, &adm_service_admin,
                             RPC_ADM_VLSTOP, &info, sizeof(info));
}

static void
local_exa_vlstop (int thr_nb, void *msg)
{
  struct adm_group *group;
  struct adm_volume *volume = NULL;
  int ret = EXA_SUCCESS;
  int barrier_ret;           /* used for barriers return values */
  struct vlstop_info *info = msg;

  group = adm_group_get_group_by_name(info->group_name);
  if (group == NULL)
  {
    ret = -ADMIND_ERR_UNKNOWN_GROUPNAME;
    goto check_barrier;
  }

  volume = adm_group_get_volume_by_name(group, info->volume_name);
  if (volume == NULL)
  {
    ret = -ADMIND_ERR_UNKNOWN_VOLUMENAME;
    goto check_barrier;
  }

  /* If the volume is marked as in-progress, then a previous
    * exa_vlcreate or exa_vldelete command was interrupted by a
    * node failure and we should not try to stop this volume.
    */
  if (!info->force && !volume->committed)
  {
    ret = -ADMIND_ERR_RESOURCE_IS_INVALID;
    goto check_barrier;
  }

check_barrier:
  /*** Barrier: "check_xml", check that the volume exists in the XML tree ***/
  barrier_ret = admwrk_barrier(thr_nb, ret, "Checking XML configuration");
  if (!info->force && barrier_ret != EXA_SUCCESS)
    goto local_exa_vlstop_end;

  /*** Action: stop the volume through the VRT API ***/
  if (!info->force && !group->started)
    ret = -VRT_ERR_VOLUME_NOT_STARTED;
  else if (adm_nodeset_contains_me(&info->nodelist))
    ret = vrt_client_volume_stop(adm_wt_get_localmb(),
                                 &volume->group->uuid, &volume->uuid);
  else
    ret = -ADMIND_ERR_NOTHINGTODO;

  if (info->force || ret == EXA_SUCCESS)
  {
    volume->started = false;
    volume->readonly = false;
  }

  /* This is an INFO, not an ERROR. */
  if (ret == -VRT_ERR_VOLUME_NOT_STARTED)
  {
    if (info->print_warning)
      ret = -VRT_INFO_VOLUME_ALREADY_STOPPED;
    else
      ret = EXA_SUCCESS;
  }

  /*** Barrier: "vrt_volume_stop", stop the volume ***/
  /* We need to examine all return values because some errors are
   * benign.
   */
  barrier_ret = admwrk_barrier(thr_nb, ret, "Stopping the logical volume");
  if (barrier_ret == -VRT_INFO_VOLUME_ALREADY_STOPPED)
    barrier_ret = EXA_SUCCESS;
  if (!info->force && barrier_ret != EXA_SUCCESS)
    goto local_exa_vlstop_end;

  if (info->goal_change & ADM_GOAL_CHANGE_VOLUME)
  {
      /*** Action: update the volume status in the config tree ***/
      /* This is an in-memory operation, we assume it won't fail */
      adm_volume_set_goal(volume, &info->nodelist, EXA_VOLUME_STOPPED, false /* readonly */);

      /* The goal changed, save the config file */
      ret = conf_save_synchronous();
      /* FIXME this is not what we call a good error handling */
      EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "%s", exa_error_msg(ret));
  }

 local_exa_vlstop_end:
  admwrk_ack(thr_nb, barrier_ret);
}

/**
 * Definition of the vlstop command.
 */
const AdmCommand exa_vlstop = {
  .code            = EXA_ADM_VLSTOP,
  .msg             = "vlstop",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_vlstop,
  .local_commands  = {
    { RPC_ADM_VLSTOP, local_exa_vlstop },
    { RPC_COMMAND_NULL, NULL }
  }
};


