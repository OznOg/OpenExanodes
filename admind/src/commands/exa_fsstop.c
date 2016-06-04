/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "log/include/log.h"

#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/strlcpy.h"

#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_fs.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/adm_nodeset.h"

#include "admind/services/fs/service_fs.h"
#include "admind/include/service_fs_commands.h"
#include "admind/src/commands/exa_fscommon.h"

__export(EXA_ADM_FSSTOP) struct fsstop_params
  {
    char group_name[EXA_MAXSIZE_GROUPNAME+1];
    char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
    char host_list[EXA_MAXSIZE_HOSTSLIST + 1];
    __optional bool force __default(false);
  };

/**
 * \brief main entry point to stop a particular FS from a command.
 *
 * \param[in]  fs: the FS to stop.
 * \param[in]  list : list of nodes to stop
 * \param[out] list_succedeed: list of nodes that succeedingly stopped.
 * \param[in]  force: Try to force stop.
 * \param[in]  goal_change: specify wether we need to change the file system's goal
 *
 * \return error code.
 */
static int fs_stop_one_fs(admwrk_ctx_t *ctx, fs_data_t *fs,
                          const exa_nodeset_t *list_to_stop,
                          exa_nodeset_t *list_stop_succeeded,
                          int force, adm_goal_change_t goal_change)
{
  int error_val=EXA_SUCCESS;
  const fs_definition_t* fs_definition=fs_get_definition(fs->fstype);
  EXA_ASSERT_VERBOSE(fs_definition, "Unknown file system type found.");
  EXA_ASSERT(fs_definition->stop_fs);

  /* Now check that the file system transaction is COMMITTED */
  if (!fs->transaction)
    {
      error_val=-FS_ERR_INVALID_FILESYSTEM;
      goto error;
    }

  /* End of protected access */
  error_val=fs_definition->check_before_stop(ctx, list_to_stop, fs, force);
  if (error_val!=EXA_SUCCESS)
    {
      goto error;
    }
  error_val=fs_definition->stop_fs(ctx, list_to_stop,
                                   fs, force, goal_change,
                                   list_stop_succeeded);

 error:
  return error_val;
}

/**
 * \brief main entry point to stop a list of FS from a command.
 *
 * \param[in] group: the group where ALL filesystems using it must be stopped;
 * \param[in] nodelist : a list of nodes to stop. If null, all nodes are stopped.
 * \param[in] force: Try to force stop.
 * \param[in] goal_change: specify whether we need to change the file system's goal
 *
 * \return error code.
 */
int
fs_stop_all_fs(admwrk_ctx_t *ctx, struct adm_group *group, const exa_nodeset_t *nodelist,
               int force, adm_goal_change_t goal_change)
{
  int error_val_last = EXA_SUCCESS;
  exa_nodeset_t list_stop_succeeded;
  exa_nodeset_t nodelist_all;
  fs_iterator_t iter;
  fs_data_t* struct_fs;

  if (nodelist == NULL)
    {
      adm_nodeset_set_all(&nodelist_all);
      nodelist =  &nodelist_all;
    }

  init_fs_iterator(&iter);
  while ((struct_fs = iterate_fs_instance(&iter, true, NULL, true)) != NULL)
    {
      int error_val = EXA_SUCCESS;
      if (iter.fs->volume->group != group)
	continue;
       error_val = fs_stop_one_fs(ctx, struct_fs, nodelist,
                                 &list_stop_succeeded, force, goal_change);
      if (error_val != EXA_SUCCESS)
	{
	  /* Continue on all other FS, to unmount as many FS as possible */
    	  error_val_last = error_val;
	}
    }
  return error_val_last;
}


/**
 * \brief Implements the fsstop command
 *        Check the file system transaction parameter. It should be "COMMITTED".
 *        Stop it on the specified nodes, using FS specific function.
 */
static void
cluster_fsstop(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct fsstop_params *params = data;
  exa_nodeset_t list, list_stop_succeeded, list_already_stopped;
  exa_nodeid_t node_already_stopped;
  exa_nodeset_iter_t iter_nodes_already_stopped;
  fs_data_t fs_data;

  int error_val;

  exalog_info("received fsstop '%s:%s'%s from '%s' on '%s'",
	      params->group_name, params->volume_name,
	      params->force ? " --force" : "", adm_cli_ip(),
              params->host_list);

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  /* Get the FS */
  if ((error_val = fscommands_params_get_fs(params->group_name,
	                                    params->volume_name,
					    &fs_data,
					    NULL,
					    true)) != EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }

  if (params->host_list[0] == '\0')/* An empty hostlist means all nodes */
    adm_nodeset_set_all(&list);
  else
  {
    if ((error_val = adm_nodeset_from_names(&list, params->host_list))
	!= EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }
  }

  /* Fill the list of nodes already stopped and cross with list
     of nodes to stop */
  adm_nodeset_set_all(&list_already_stopped);
  exa_nodeset_substract(&list_already_stopped, &fs_data.goal_started);
  exa_nodeset_intersect(&list_already_stopped, &list);

  /* Print a warning for all of them */
  exa_nodeset_iter_init(&list_already_stopped, &iter_nodes_already_stopped);
  while (exa_nodeset_iter(&iter_nodes_already_stopped, &node_already_stopped))
    {
      adm_write_inprogress(adm_nodeid_to_name(node_already_stopped),
                           exa_error_msg(-FS_INFO_ALREADY_STOPPED),
                           -FS_INFO_ALREADY_STOPPED,
                           exa_error_msg(-FS_INFO_ALREADY_STOPPED));
    }

  error_val = fs_stop_one_fs(ctx, &fs_data, &list,
                             &list_stop_succeeded, params->force,
                             ADM_GOAL_CHANGE_VOLUME);
  if (error_val)
    {
      set_error(err_desc, error_val, exa_error_msg(error_val));
      return;
    }

  /* If 1 node failed, the whole command failed */
  if (!params->force &&
      exa_nodeset_count(&list_stop_succeeded) != exa_nodeset_count(&list))
    {
      set_error(err_desc, -FS_ERR_UMOUNT_ERROR, NULL);
      return;
    }

  set_success(err_desc);
}


/**
 * Definition of the exa_fsstop command.
 */
const AdmCommand exa_fsstop = {
  .code            = EXA_ADM_FSSTOP,
  .msg             = "fsstop",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_fsstop,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};


