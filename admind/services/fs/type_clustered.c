/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "admind/services/fs/type_clustered.h"
#include "admind/services/fs/generic_fs.h"
#include "admind/services/fs/service_fs.h"

#include <errno.h>
#include <string.h>

#include "admind/include/service_vrt.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_fs.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/instance.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "fs/include/exa_fsd.h"


#define MY_SERVICE_ID &adm_service_fs

/* === Internal functions ============================================ */

/* --- is_clustered_started --------------------------------------------- */
/**
 * Check if a clustered filesystem is started on at least one node
 *
 * \param[int] fstype    A string constant for the given clustered FS type.
 *
 * \return true if one is valid and started.
 *         false otherwise.
 */
int
is_clustered_started(const char* fstype)
{
  int number_started = 0;
  fs_iterator_t iter;
  fs_data_t* struct_fs;

  init_fs_iterator(&iter);
  while ((struct_fs = iterate_fs_instance(&iter, true, fstype, true)) != NULL)
    {
      number_started++;
    }
  return number_started;
}

/* --- clustered_check_before_start ------------------------------------- */
/**
 * Check that start is possible
 *
 * \return 0 on success or an error code.
 */
exa_error_code clustered_check_before_start(admwrk_ctx_t *ctx, fs_data_t* fs)
{
  struct adm_group *group = fs_get_volume(fs)->group;

  if (group->started)
    return EXA_SUCCESS;
  else
    return -VRT_ERR_GROUP_NOT_STARTED;
}

/* --- clustered_start_fs ----------------------------------------------- */
/**
 * Mount the filesystem
 *
 * \param[in] nodes	          Set of nodes
 * \param[in] nodes_read_only	  Set of nodes mounted read-only. not a SUBSET of "nodes" !
 * \param[out] start_succeeded    List of nodes on which the mount action succeeded.
 * \param[in] recovery            Set to true if it is called from a recovery.
 *
 * \return 0 on success or an error code.
 */
exa_error_code clustered_start_fs(admwrk_ctx_t *ctx, const exa_nodeset_t* nodes,
				  const exa_nodeset_t* nodes_read_only,
				  fs_data_t* fs, exa_nodeset_t* start_succeeded,
				  int recovery)
{
  int ret, err, final_err;
  startstop_info_t info;
  exa_nodeset_t start_failed;
  exa_nodeset_t all_nodes_to_start;
  exa_nodeset_iter_t iter_node;
  exa_nodeid_t current_node;

  final_err = EXA_SUCCESS;
  exa_nodeset_reset(&start_failed);
  exa_nodeset_copy(start_succeeded, nodes);
  exa_nodeset_sum(start_succeeded, nodes_read_only);
  exa_nodeset_copy(&all_nodes_to_start, nodes);
  exa_nodeset_sum(&all_nodes_to_start, nodes_read_only);

  if (!recovery)
    {
      /* Start the data volume normal */
      if (!exa_nodeset_is_empty(nodes))
      {
        ret = fs_data_device_start(ctx, fs, nodes, false);
        if (ret)
        {
            exa_nodeset_reset(start_succeeded);
            return ret;
        }
      }

      if (!exa_nodeset_is_empty(nodes_read_only))
      {
        ret = fs_data_device_start(ctx, fs, nodes_read_only, true);
        if (ret)
        {
            exa_nodeset_reset(start_succeeded);
            return ret;
        }
      }
    }
  memcpy(&info.fs, fs, sizeof(info.fs));
  info.recovery = recovery;

  /* Yet another ugly hack for GFS. We should mount on all nodes, but there is a
     race in GFS that prevents us to do so.*/
  exa_nodeset_iter_init(&all_nodes_to_start, &iter_node);

  /* prepare */
  info.action = startstop_action_prepare;
  exa_nodeset_reset(&info.nodes);
  err = admwrk_exec_command(ctx, MY_SERVICE_ID, RPC_SERVICE_FS_STARTSTOP, &info, sizeof(info));
  if (err != EXA_SUCCESS)
    {
      exa_nodeset_copy(&start_failed, start_succeeded);
      exa_nodeset_reset(start_succeeded);
      final_err = err;
    }
  else
    {
      /* effective mount */
      info.action = startstop_action_mount;
      while (exa_nodeset_iter(&iter_node, &current_node))
	{
	  exa_nodeid_t nodeid;
	  exa_nodeset_t nodes;
	  exa_nodeset_reset(&info.nodes);
	  exa_nodeset_add(&info.nodes, current_node);
	  info.read_only = exa_nodeset_contains(nodes_read_only, current_node);

	  inst_get_current_membership_cmd(MY_SERVICE_ID, &nodes);

	  /* Call start. Sort for each node. */
	  admwrk_run_command(ctx, &nodes, RPC_SERVICE_FS_STARTSTOP, &info, sizeof(info));
	  while (!exa_nodeset_is_empty(&nodes))
	    {
              admwrk_get_ack(ctx, &nodes, &nodeid, &err);
	      exalog_debug("FS start (mount) >>> reply from node %u, err=%d",
			   nodeid, err);
	      if (err != -ADMIND_ERR_NODE_DOWN && err != EXA_SUCCESS && err != -FS_WARN_MOUNTED_READONLY)
		/* Remove from list of succeeded. */
		{
		  exa_nodeset_del(start_succeeded, nodeid);
		  exa_nodeset_add(&start_failed, nodeid);
		  final_err = err;
		}
	      if (err == -ADMIND_ERR_NODE_DOWN)
		{
		  final_err = err;
		}
	    }
	}
    }

  if (!recovery)
    {
      /* Stop volume on nodes that failed to start, only not in recovery */
      if (exa_nodeset_count(&start_failed) != 0)
	{
          ret = fs_data_device_stop(ctx, fs, &start_failed,
                                    false /* force */, ADM_GOAL_CHANGE_VOLUME);
	  if (ret)
	    {
	      if (ret!=-ADMIND_ERR_NODE_DOWN)
		{
		  return ret;
		}
	    }
	}
    }
  return final_err;
}

/* --- clustered_stop_fs ------------------------------------------------ */
/**
 * Umount the filesystem
 *
 * \param[in] nodes	         Set of nodes to stop
 * \param[in] force              Continue on case of error
 * \param[in] goal_change        Specify whether we need to change the goal
 * \param[out] stop_succeeded    List of nodes on which the unmount action succeeded.
 * \return 0 on success or an error code.
 */
exa_error_code clustered_stop_fs(admwrk_ctx_t *ctx, const exa_nodeset_t *nodes_to_stop,
                                 fs_data_t* fs, bool force,
                                 adm_goal_change_t goal_change,
                                 exa_nodeset_t* stop_succeeded)
{
  exa_nodeset_t nodes;
  exa_nodeid_t nodeid;
  int ret,err;
  startstop_info_t info;
  int unload=0;
  int ret_unload=0;
  struct adm_group *group = fs_get_volume(fs)->group;

  if(!group->started)
    goto vlstop;

  exa_nodeset_copy(stop_succeeded, nodes_to_stop);

  /* umount the filesystem */
  info.action = startstop_action_umount;
  if (is_clustered_started(fs->fstype)>1)
    {
      /* There is another FS of this type started */
      unload = 0;
    }
  else
    {
      /* Is this one the only one marked started ? */
      if (((is_clustered_started(fs->fstype) == 1) && (exa_nodeset_count(&fs->goal_started))) ||
	   (is_clustered_started(fs->fstype)==0) )
	{
	  /* Check if we're totally stopped when this one stops. */
	  exa_nodeset_t copy;
	  unload=0;
	  exa_nodeset_copy(&copy, &fs->goal_started);
	  exa_nodeset_substract(&copy, nodes_to_stop);
	  exalog_debug("FS stop nodes to stop : %d number of nodes stopped %d ",
		       exa_nodeset_count(nodes_to_stop),exa_nodeset_count(&copy));
	  if (exa_nodeset_count(&copy) == 0)
	    {
	      unload=1;
	    }
	}
    }
  memcpy(&info.fs, fs, sizeof(info.fs));
  exa_nodeset_copy(&info.nodes, nodes_to_stop);

  inst_get_current_membership_cmd(MY_SERVICE_ID, &nodes);

  /* Call stop. Sort for each node. */
  admwrk_run_command(ctx, &nodes, RPC_SERVICE_FS_STARTSTOP, &info, sizeof(info));
  while (!exa_nodeset_is_empty(&nodes))
  {
    admwrk_get_ack(ctx, &nodes, &nodeid, &err);
    exalog_debug("FS stop (unmount) >>> reply from %u, err=%d",
		 nodeid, err);
    if (!force && err != -ADMIND_ERR_NODE_DOWN && err != EXA_SUCCESS)
      /* Remove from list of succeeded. */
      {
	exa_nodeset_del(stop_succeeded, nodeid);
	unload=0;
      }
  }

  /* We have to unload the filesystem. We get in this case *only*
     when all filesystems are marked STOPPED, and are *really* unmounted. */
  if (unload)
    {
      info.action = startstop_action_unload;
      exa_nodeset_reset(&info.nodes);
      ret_unload = admwrk_exec_command(ctx, MY_SERVICE_ID, RPC_SERVICE_FS_STARTSTOP,
				       &info, sizeof(info));
    }

vlstop:
  /* Stop the data volume on stopped FS */
  ret = fs_data_device_stop(ctx, fs, stop_succeeded, force, goal_change);
  if (!force && ret_unload)
  {
    return ret_unload;
  }

  if ( (! force) &&
       (ret != -ADMIND_ERR_NODE_DOWN) ) return ret;

  return EXA_SUCCESS;
}

/* --- clustered_check_fs -------------------------------------------- */
/**
 * Check filesystem
 *
 * \param[in] optional_parameter  optional parameter to pass to fsck.
 * \param[in] node_where_to_check Node used to check the filesytem on.
 * \param[in] repair              Repair or just check ?
 * \return 0 on success or an error code.
 */
exa_error_code clustered_check_fs(admwrk_ctx_t *ctx, fs_data_t* fs, const char* optional_parameter,
				  exa_nodeid_t node_where_to_check, bool repair)
{
  return generic_fs_check(ctx, fs, optional_parameter,
			  node_where_to_check, repair);
}

/* --- clustered_specific_fs_recovery ---------------------------------------------- */
/**
 * \brief Recovery of a filesystem instance.
 *
 * \return 0 on success or an error code.
 */
exa_error_code clustered_specific_fs_recovery(admwrk_ctx_t *ctx, fs_data_t* fs)
{
    exa_nodeset_t list_to_start, list_to_start_read_only, list_started;
    const fs_definition_t *fs_definition = fs_get_definition(fs->fstype);
    exa_error_code error;

    exa_nodeset_copy(&list_to_start, &fs->goal_started);
    exa_nodeset_copy(&list_to_start_read_only, &fs->goal_started_ro);

    exa_nodeset_intersect(&list_to_start_read_only, &list_to_start);

    error = fs_definition->start_fs(ctx, &list_to_start, &list_to_start_read_only,
                                    fs, &list_started, true);
    if (error != EXA_SUCCESS)
    {
        /* Log it, and return it */
        exalog_error("Local recovery of filesystem called '%s' failed. Error=%d",
                     fs_get_name(fs), error);
        return error;
    }

    return EXA_SUCCESS;
}
