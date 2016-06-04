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
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/deviceblocks.h"
#include "admind/src/instance.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "lum/client/include/lum_client.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/exa_nodeset.h"
#include "log/include/log.h"
#include "os/include/os_file.h"
#include "os/include/strlcpy.h"
#include "vrt/virtualiseur/include/vrt_client.h"
#include "lum/client/include/lum_client.h"

__export(EXA_ADM_VLSTART) struct vlstart_params
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  char host_list[EXA_MAXSIZE_HOSTSLIST + 1];
#ifdef WITH_FS
  __optional bool no_fs_check __default(false);
#endif
  bool readonly;
  __optional char export_method[EXA_MAXSIZE_EXPORT_METHOD] __default("");
  __optional char export_option[EXA_MAXSIZE_EXPORT_OPTION] __default("");
};

/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct vlstart_info
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  exa_nodeset_t nodelist;
  uint32_t readonly;
  uint32_t print_warning;
};


/** \brief Implements the vlstart command
 */
static void
cluster_vlstart(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct vlstart_params *params = data;
  struct adm_group *group;
  struct adm_volume *volume;

  exa_nodeset_t nodes_down;
  exa_nodeset_t nodeset;
  exa_nodeid_t node_id;
  int error_val;
  bool allnodes;

  /* An empty hostlist means all nodes */
  allnodes = params->host_list[0] == '\0';

  exalog_info("received vlstart '%s:%s' on hosts '%s'%s%s from %s",
	      params->group_name, params->volume_name,
	      params->host_list[0] == '\0' ? "all" : params->host_list,
#ifdef WITH_FS
	      params->no_fs_check ? " --nofscheck" : "",
#else
	      "",
#endif
	      params->readonly ? " --readonly" : "",
	      adm_cli_ip());

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

  if (lum_exports_get_type_by_uuid(&volume->uuid) == EXPORT_ISCSI
      && params->readonly)
  {
      set_error(err_desc, -EXA_ERR_INVALID_VALUE,
	        "The read-only option is not available with an iSCSI volume");
      return;
  }

  if (lum_exports_get_type_by_uuid(&volume->uuid) == EXPORT_ISCSI
      && !allnodes)
  {
      set_error(err_desc, -EXA_ERR_INVALID_VALUE,
	        "iSCSI volumes must be started on all nodes.");
      return;
  }

#ifdef WITH_FS
  /* Check if the volume is not part of a file system */
  if (!params->no_fs_check && adm_volume_is_in_fs(volume))
    {
      set_error(err_desc, -ADMIND_ERR_VOLUME_IN_FS,
		"volume '%s:%s' is managed by the file system layer", volume->group->name,
		volume->name);
      return;
  }
#endif

  if (allnodes)
    adm_nodeset_set_all(&nodeset);
  else
    {
      int r = adm_nodeset_from_names(&nodeset, params->host_list);
      if (r != EXA_SUCCESS)
	{
	  set_error(err_desc, r, NULL);
	  return;
	}
    }

  error_val = vrt_master_volume_start(ctx, volume, &nodeset,
                                      params->readonly,
                                      true /* print_warning */);
  set_error(err_desc, error_val, NULL);

  /* Print a warning for nodes that are down. */

  inst_get_nodes_down(&adm_service_vrt, &nodes_down);
  exa_nodeset_intersect(&nodes_down, &nodeset);

  exa_nodeset_foreach(&nodes_down, node_id)
  {
      adm_write_inprogress(adm_nodeid_to_name(node_id),
                           "Checking nodes down.",
                           -ADMIND_WARNING_NODE_IS_DOWN,
                           "Some nodes are down. The volume will be started"
                           " on them at next boot.");
  }

  exalog_debug("vlstart clustered command is complete %d", error_val);
}


int vrt_master_volume_start(admwrk_ctx_t *ctx, struct adm_volume *volume,
			    const exa_nodeset_t * nodelist, uint32_t readonly,
                            bool print_warning)
{
  int error_val;
  exa_nodeset_t nodes_volstarted;
  exa_nodeset_t nodes_readonly;
  struct vlstart_info info;
  int status;

  EXA_ASSERT(volume != NULL);

  /* Do not continue if the group is not started */
  if (volume->group->goal == ADM_GROUP_GOAL_STOPPED)
  {
    error_val = -VRT_ERR_GROUP_NOT_STARTED;
    goto end_error;
  }

  /* Compute the set of nodes on which the volume will be started */
  exa_nodeset_copy(&nodes_volstarted, &volume->goal_started);
  exa_nodeset_sum(&nodes_volstarted, nodelist);

  /* Compute the set of nodes on which the volume will be readonly */
  exa_nodeset_copy(&nodes_readonly, &volume->goal_readonly);
  if (readonly)
    exa_nodeset_sum(&nodes_readonly, nodelist);

  /* test about private volume: RW on one node or RO on several nodes */
  if (!volume->shared)
    {
      int nb_started = exa_nodeset_count(&nodes_volstarted);
      int nb_rw = nb_started - exa_nodeset_count(&nodes_readonly);

      if (nb_rw > 0 && nb_started > 1)
        {
          error_val = -VRT_ERR_VOLUME_IS_PRIVATE;
          goto end_error;
        }
    }

  memset(&info, 0, sizeof(info));

  /* Every thing is ok, we can start the volume on nodelist */
  strlcpy(info.group_name,  volume->group->name, EXA_MAXSIZE_GROUPNAME + 1);
  strlcpy(info.volume_name, volume->name, EXA_MAXSIZE_VOLUMENAME + 1);
  exa_nodeset_copy(&info.nodelist, nodelist);
  info.readonly = readonly;
  info.print_warning = print_warning;
  error_val = admwrk_exec_command(ctx, &adm_service_admin, RPC_ADM_VLSTART, &info, sizeof(info));

  if(error_val)
    goto end_error;

  status = vrt_client_get_volume_status(adm_wt_get_localmb(),
					&volume->group->uuid, &volume->uuid);

  if(status < EXA_SUCCESS)
  {
    error_val = status;
    exalog_error("vlstart failed (%s)", exa_error_msg(error_val));
    goto end_error;
  }
  else
  {
    error_val = EXA_SUCCESS;
  }

end_error:
  return error_val;
}

int
vrt_master_volume_start_all(admwrk_ctx_t *ctx, struct adm_group *group)
{
  int ret = EXA_SUCCESS;
  int overall_ret = EXA_SUCCESS;
  struct adm_volume *volume;
  exa_nodeset_t goal_rw;

  adm_group_for_each_volume(group, volume)
  {
    if (!adm_volume_is_in_fs(volume))
    {
      if (!exa_nodeset_is_empty(&volume->goal_started))
      {
        /* In the adm_volume structure, the 'goal_readonly' set is
         * included in the 'goal_started' set.
         * As a result:
         * o we need to start the volume in RW mode on nodes that
         *   have set 'goal_started' and *not* 'goal_readonly'.
         * o we need to start the volume in RO mode on nodes that
         *   have set 'goal_readonly'.
         */
        exa_nodeset_copy(&goal_rw, &volume->goal_started);
        exa_nodeset_substract(&goal_rw, &volume->goal_readonly);
        if (!exa_nodeset_is_empty(&goal_rw))
          ret = vrt_master_volume_start(ctx, volume, &goal_rw,
                                        false /* readonly */,
                                        false /* print_warning */);
        if (!exa_nodeset_is_empty(&volume->goal_readonly))
          ret = vrt_master_volume_start(ctx, volume, &volume->goal_readonly,
                                        true /* readonly */,
                                        false /* print_warning */);
        if (ret != EXA_SUCCESS)
          overall_ret = ret;
      }
    }
  }

  return overall_ret;
}

static void
local_exa_vlstart (admwrk_ctx_t *ctx, void *msg)
{
  struct adm_group *group;
  struct adm_volume *volume = NULL;
  int ret;                   /* used for local function calls */
  int barrier_ret;           /* used for barriers return values */
  struct vlstart_info *info = msg;
  int need_to_start = false;

  ret = EXA_SUCCESS;

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

  /* Do we need to start the volume on this node? */
  need_to_start = adm_nodeset_contains_me(&info->nodelist);

get_barrier:
  /*** Barrier: "cmd_params_get", getting parameters ***/
  barrier_ret = admwrk_barrier(ctx, ret, "Getting parameters");
  if (barrier_ret != EXA_SUCCESS)
    goto local_exa_vlstart_end; /* Nothing to undo */

  /* If the volume is marked as in-progress, then a previous exa_vlcreate or
   * exa_vldelete command was interrupted by a node failure and we should not
   * try to start this volume.
   */
  if (!volume->committed)
    ret = -ADMIND_ERR_RESOURCE_IS_INVALID;

  /*** Barrier: "check_xml", check that the volume exists in the XML tree ***/
  barrier_ret = admwrk_barrier(ctx, ret, "Checking XML configuration");
  if (barrier_ret != EXA_SUCCESS)
    goto local_exa_vlstart_end; /* Nothing to undo */

  if (need_to_start && volume->exported)
  {
      if (info->readonly ^ volume->readonly)
          ret = -ADMIND_ERR_VOLUME_ACCESS_MODE;
  }
  barrier_ret = admwrk_barrier(ctx, ret, "Checking volume's access mode");
  if (barrier_ret != EXA_SUCCESS)
      goto local_exa_vlstart_end;

  adm_volume_set_goal(volume, &info->nodelist, EXA_VOLUME_STARTED, info->readonly);
  ret = conf_save_synchronous();
  EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "%s", exa_error_msg(ret));

  if (need_to_start)
    ret = vrt_client_volume_start(adm_wt_get_localmb(),
				  &volume->group->uuid, &volume->uuid);
  else
    ret = -ADMIND_ERR_NOTHINGTODO;

  if (ret == EXA_SUCCESS)
  {
    volume->started = true;
    volume->readonly = info->readonly;
  }

  /* This is an INFO, not an ERROR. */
  if (ret == -VRT_ERR_VOLUME_ALREADY_STARTED)
  {
    if (info->print_warning)
      ret = -VRT_INFO_VOLUME_ALREADY_STARTED;
    else
      ret = EXA_SUCCESS;
  }

  barrier_ret = admwrk_barrier(ctx, ret, "Starting the logical volume");
  if (barrier_ret != EXA_SUCCESS &&
      barrier_ret != -VRT_INFO_VOLUME_ALREADY_STARTED)
    goto local_exa_vlstart_end;

  if (need_to_start && !volume->exported)
  {
      bool exported;

      ret = lum_service_publish_export(&volume->uuid);
      if (ret == EXA_SUCCESS)
          exported = true;
      else if (ret == -VRT_ERR_VOLUME_ALREADY_EXPORTED)
      {
          if (info->print_warning)
              ret = -VRT_INFO_VOLUME_ALREADY_EXPORTED;
          else
              ret = EXA_SUCCESS;
          exported = true;
      }
      else
          exported = false;

      volume->exported = exported;

      if (lum_exports_get_type_by_uuid(&volume->uuid) == EXPORT_BDEV
          && volume->exported)
      {
          /* FIXME Should pass export->uuid, *not* volume->uuid */
          ret = lum_client_set_readahead(adm_wt_get_localmb(), &volume->uuid,
                                         volume->readahead);
          if (ret != EXA_SUCCESS)
              exalog_warning("Failed setting readahead on %s:%s",
                             group->name, volume->name);
      }
  }
  else
      ret = -ADMIND_ERR_NOTHINGTODO;

  barrier_ret = admwrk_barrier(ctx, ret, "Exporting the logical volume");

 local_exa_vlstart_end:
  exalog_debug("local_exa_vlstart() = %s", exa_error_msg(barrier_ret));
  admwrk_ack(ctx, barrier_ret);
}

/**
 * Definition of the vlstart message.
 */
const AdmCommand exa_vlstart = {
  .code            = EXA_ADM_VLSTART,
  .msg             = "vlstart",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_vlstart,
  .local_commands  = {
    { RPC_ADM_VLSTART, local_exa_vlstart },
    { RPC_COMMAND_NULL, NULL }
  }
};


