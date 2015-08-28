/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "admind/services/fs/type_local.h"
#include "admind/services/fs/type_clustered.h"
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
#include "admind/src/rpc.h"
#include "admind/src/adm_workthread.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_config.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"
#include "fs/include/exa_fsd.h"
#include "vrt/virtualiseur/include/vrt_client.h"

#define MY_SERVICE_ID &adm_service_fs

/**
 * \brief Fill the struct generic_fs from the config struct adm_fs
 */
static void local_fill_data_from_config(fs_data_t* generic_fs, struct adm_fs *fs)
{
}

/**
 * \brief Check that start is possible
 *
 * \return 0 on success or an error code.
 */
static exa_error_code local_check_before_start(int thr_nb, fs_data_t* fs)
{
  struct adm_group *group = fs_get_volume(fs)->group;
  if (group->started)
    return EXA_SUCCESS;
  else
    return -VRT_ERR_GROUP_NOT_STARTED;
}

/**
 * \brief Create the filesystem
 *
 * \param[in] fs          File system data
 *
 * \return 0 on success or an error code.
 */
static exa_error_code local_create_fs(int thr_nb, fs_data_t* fs)
{
  int ret_mkfs;

  /* Do not allow creation of a FS of less than 5M : mkfs.ext3 would accept,
     but wouldn't create the journal. Therefore, it would fail at mount time. */
  if ((fs->sizeKB < FS_MINIMUM_EXT3_SIZE ) && (fs->sizeKB != 0))
    {
      exalog_error("Cannot create a local FS of size less than 8M :"
		   " it would fail when mounting ext3");
      return -FS_ERR_MKFS_SIZE_ERROR;
    }

  /* print a nice message before mkfs */
  adm_write_inprogress(adm_nodeid_to_name(adm_my_id),
		       "mkfs is in progress, it may take time",
		       EXA_SUCCESS, "");

  /* Mkfs */
  ret_mkfs = fsd_fs_create_local(adm_wt_get_localmb(), fs->fstype,
				 fs->devpath);

  /* print a nice message with mkfs status */
  adm_write_inprogress(adm_nodeid_to_name(adm_my_id), "mkfs returned : ",
		       ret_mkfs, "Unexpected error, mkfs failed");

  return ret_mkfs;
}

/**
 * \brief Mount the filesystem,
 *
 * \param[in]  nodes	          Set of nodes
 * \param[in]  nodes_read_only	  Subset of nodes mounted read-only.
 * \param[out] start_succeeded    list of nodes on which the mount action succeeded.
 * \param[in]  recovery           Set to true if it is called from a recovery.
 *                                In that case, don't start the volume.
 *
 * \return 0 on success or an error code.
 */
static exa_error_code local_start_fs(int thr_nb, const exa_nodeset_t* nodes,
				     const exa_nodeset_t* nodes_read_only,
				     fs_data_t* fs, exa_nodeset_t* start_succeeded,
				     int recovery)
{
  int ret;
  startstop_info_t info;
  exa_nodeset_t final_started_list;

  exa_nodeset_copy(start_succeeded, nodes);

  /* If read-only, bypass one-node checking and go in clustered mode */
  if (!exa_nodeset_is_empty(nodes_read_only))
    {
      int ret;
      ret = clustered_start_fs(thr_nb, nodes, nodes_read_only, fs,
                               start_succeeded, recovery);
      if (ret == -VRT_ERR_VOLUME_IS_PRIVATE)
	{
	  exa_nodeset_reset(start_succeeded);
	  return -FS_ERR_LOCAL_RW_ONE_NODE;
	}
      return ret;
    }

  /* 1 and only 1 node */
  exa_nodeset_copy(&final_started_list, &fs->goal_started);
  exa_nodeset_sum(&final_started_list, nodes);
  if (exa_nodeset_count(&final_started_list) > 1
      || exa_nodeset_count(&fs->goal_started_ro) != 0)
    {
      exa_nodeset_reset(start_succeeded);
      return -FS_ERR_LOCAL_RW_ONE_NODE;
    }

  if (!recovery)
    {
      /* Start the volume on nodes */
      ret = fs_data_device_start(thr_nb, fs, nodes, false);
      if (ret)
      {
          exa_nodeset_reset(start_succeeded);
          return ret;
      }
    }

  /* Compute arguments to local function for mount. */
  memset(&info, 0, sizeof(info));
  info.action = startstop_action_mount;
  info.read_only = 0;
  info.recovery = recovery;
  memcpy(&info.fs, fs, sizeof(info.fs));
  exa_nodeset_copy(&info.nodes, nodes);
  ret = admwrk_exec_command(thr_nb, MY_SERVICE_ID, RPC_SERVICE_FS_STARTSTOP,
                            &info, sizeof(info));
  if (ret && ret != -FS_WARN_MOUNTED_READONLY)
    {
      exa_nodeset_reset(start_succeeded);
      /* Try to stop the volume on the failed node */
      if (!recovery)
	{
          fs_data_device_stop(thr_nb, fs, nodes, false /* force */,
                              ADM_GOAL_CHANGE_VOLUME);
	}
      return ret;
    }
  else
    {
      /* If mount succeeded although the resource is down and we are not in
	 recovery, inform the user that it is a RO mount. If mount fails, the
	 user will be warned by an error message */
      /* TODO */
    }
  return EXA_SUCCESS;
}

/**
 * \brief Umount the filesystem
 *
 * \param[in] nodes	      Set of nodes to unmount.
 * \param[in] force           Continue in case of error
 * \param[in] goal_change     Specify whether we need to change the file system's goal
 * \param[out] stop_succeeded list of nodes on which the unmount action succeeded.
 *
 * \return 0 on success or an error code.
 */
static exa_error_code local_stop_fs(int thr_nb, const exa_nodeset_t *nodes,
                                    fs_data_t *fs, bool force,
                                    adm_goal_change_t goal_change,
                                    exa_nodeset_t* stop_succeeded)
{
  int ret;
  startstop_info_t info;
  struct adm_group *group = fs_get_volume(fs)->group;

  /* If read-only, bypass one-node checking and go in clustered mode */
  if (!exa_nodeset_is_empty(&fs->goal_started_ro))
    {
      return clustered_stop_fs(thr_nb, nodes, fs, force, goal_change,
                               stop_succeeded);
    }

  /* Just change the goal if the group is stopped. */
  if (!group->started)
    goto vlstop;

  exa_nodeset_copy(stop_succeeded, nodes);

  /* Umount the filesystem */
  memset(&info, 0, sizeof(info));
  info.action = startstop_action_umount;
  memcpy(&info.fs, fs, sizeof(info.fs));
  exa_nodeset_copy(&info.nodes, nodes);
  ret = admwrk_exec_command(thr_nb, MY_SERVICE_ID, RPC_SERVICE_FS_STARTSTOP, &info, sizeof(info));
  /* Stopping node failed. */
  if (! force && (ret != EXA_SUCCESS))
    {
      exa_nodeset_reset(stop_succeeded);
      return ret;
    }

vlstop:
  /* Stop the volume */
  ret = fs_data_device_stop(thr_nb, fs, nodes, force, goal_change);

  if ( (! force) &&
       (ret != -ADMIND_ERR_NODE_DOWN) ) return ret;
  return EXA_SUCCESS;
}

/**
 * \brief Resize the filesystem
 *
 * \param[in] sizeKB      size in KB
 *
 * \return 0 on success or an error code.
 */
static exa_error_code local_resize_fs(int thr_nb, fs_data_t* fs, int64_t sizeKB)
{
  int ret,ret_stop, ret_finalize = EXA_SUCCESS;
  exa_nodeset_t list;
  struct adm_volume *volume = fs_get_volume(fs);
  struct vrt_volume_info volume_info;

  exalog_debug("local_resize_fs: sizeKB=%"PRId64, sizeKB);

  if (!strcmp(FS_NAME_XFS, fs->fstype))
    {
      return generic_fs_mounted_grow(thr_nb, fs, sizeKB);
    }

  /* Now, only the ext3 case */
  if (exa_nodeset_count(&fs->goal_started) >= 1)
    return -FS_ERR_RESIZE_NEED_UMOUNT;

  /* build a list containing only the local node */
  exa_nodeset_reset(&list);
  exa_nodeset_add(&list, adm_my_id);

  /* Start the data volume (this must fails if the volume is already started) */
  ret = fs_data_device_start(thr_nb, fs, &list, false);
  if (ret)
      goto stop_and_exit;

  /* prepare the resize */
  ret = fsd_prepare_resize(adm_wt_get_localmb(), fs->fstype, fs->devpath);
  if (ret) goto finalize_stop_and_exit;

  /* We must now resize the volume and the filesystem in the _RIGHT_ order */
  exalog_debug("resize the filesystem `%s': %s from %"PRId64"KB to %"PRId64"KB",
              fs_get_name(fs), (sizeKB >= fs->sizeKB)?"grow":"shrink", fs->sizeKB, sizeKB);
  if (sizeKB == 0 || sizeKB >= fs->sizeKB) /* grow */
    {
      /* resize the volume */
      ret = vrt_master_volume_resize(thr_nb, volume, sizeKB);
      if (ret) goto finalize_stop_and_exit;

      /* print a nice message and resize the filesystem */
      adm_write_inprogress(adm_nodeid_to_name(adm_my_id),
			   "Resizing in progress, it may take time",
			   EXA_SUCCESS, "");
      ret = fsd_resize(adm_wt_get_localmb(),
		       fs->fstype,
		       fs->devpath,
		       fs->mountpoint,
		       0); /* "0" means the size of the device */
    }
  else /* shrink */
    {
      /* resize the filesystem */
      ret = fsd_resize(adm_wt_get_localmb(),
		       fs->fstype,
		       fs->devpath,
		       fs->mountpoint,
		       sizeKB);
      if (ret) goto finalize_stop_and_exit;

      /* resize the volume */
      ret = vrt_master_volume_resize(thr_nb, volume, sizeKB);
      if (ret) goto finalize_stop_and_exit;
    }

  /* Save the new size in the configuration */
  ret = vrt_client_volume_info(adm_wt_get_localmb(),
                               &volume->group->uuid,
                               &volume->uuid,
                               &volume_info);
  EXA_ASSERT(ret == EXA_SUCCESS);
  fs->sizeKB = volume_info.size;

 finalize_stop_and_exit:

  ret_finalize = fsd_finalize_resize(adm_wt_get_localmb(),
				     fs->fstype,
				     fs->devpath);

 stop_and_exit:
  /* Stop the data volume */
  ret_stop = fs_data_device_stop(thr_nb, fs, &list, false /* force */,
                                 ADM_GOAL_CHANGE_VOLUME);
  if (ret) return ret;
  if (ret_finalize) return ret_finalize;
  return ret_stop;
}

/**
 * \brief Recovery of a filesystem instance of type local. Starts FS on node_up list
 *        if it is contains the one and only possible node in the nodes_started list.
 *
 * \return 0 on success or an error code.
 */
static exa_error_code local_specific_fs_recovery(int thr_nb, fs_data_t* fs)
{
  exa_nodeset_t start_succeed;

  exa_error_code error = local_start_fs(thr_nb, &fs->goal_started, &fs->goal_started_ro,
					fs, &start_succeed, true);
  if (error != EXA_SUCCESS)
    {
      /* Log it, and return it */
      exalog_error("Local recovery of filesystem called '%s' failed. Error=%d",
		   fs_get_name(fs), error);
    }
  return error;
}

/**
 * \brief A local FS is always private
 */
static bool local_is_volume_private(void)
{
  return true;
}

/**
 * \brief Set the read-ahead, through the command fstune and at
 *        the end a call to vltune. Call the generic tune if's not about RA.
 *
 * \param[in] fs                  Filesystem data chunk
 * \param[in] parameter           Option to change.
 * \param[in] value               Set to this value.
 *
 * \return 0 on success or an error code.
 */
static exa_error_code local_tune(int thr_nb, fs_data_t* fs,
				 const char* parameter, const char* value)
{
  if (!strcmp(parameter, EXA_FSTUNE_READAHEAD))
    {
      uint64_t ra_value;
      int ret;
      ret = exa_get_size_kb(value, &ra_value);
      if (ret != EXA_SUCCESS) {
	  return ret;
      }
      return vrt_master_volume_tune_readahead(thr_nb, fs_get_volume(fs),
					      ra_value);
    }
  return generic_fs_tune(thr_nb, fs, parameter, value);
}

/**
 * \brief Return one tuning parameter, only READAHEAD at the moment.
 *
 * param[in]  fs               represents the FS whose tuning must be caught.
 * param[out] tune_name_value  structure to fill with the name/value pair
 * param[out] error            error code.
 *
 * \return false if it was the last name or an error occurred, true instead
 *         and fill tune_name_value
 */
static bool local_gettune(int thr_nb, fs_data_t* fs,
				struct tune_t* tune, int* error)
{
  *error = EXA_SUCCESS;

  if (!strcmp(tune_get_name(tune), ""))
    {
      struct adm_volume *volume;

      volume = fs_get_volume(fs);
      if (! volume)
      {
          *error = -ADMIND_ERR_UNKNOWN_VOLUMENAME;
          return false;
      }

      tune_set_name(tune, EXA_FSTUNE_READAHEAD);
      tune_set_nth_value(tune, 0, "%" PRIu32 "K", volume->readahead);
      tune_set_description(tune, "Read-ahead for this FS with explicit unit (1K, 1M...)");
      return true;
    }
  else if (!strcmp(tune_get_name(tune), EXA_FSTUNE_READAHEAD))
    {
      tune_set_name(tune, "");
    }
  return generic_fs_gettune(thr_nb, fs, tune, error);
}

/**
 * List of filesystem types that this API handle
 */
static const char* local_type_name_list[]={FS_NAME_EXT3, FS_NAME_XFS, NULL};

/* === Client API ==================================================== */

const fs_definition_t local_definition=
{
  .type_name_list               = local_type_name_list,
  .check_before_start           = local_check_before_start,
  .check_before_stop            = generic_fs_check_before_stop,
  .create_fs                    = local_create_fs,
  .start_fs                     = local_start_fs,
  .stop_fs                      = local_stop_fs,
  .resize_fs                    = local_resize_fs,
  .check_fs                     = generic_fs_check,
  .tune                         = local_tune,
  .gettune                      = local_gettune,
  .fill_data_from_config        = local_fill_data_from_config,
  .specific_fs_recovery         = local_specific_fs_recovery,
  .is_volume_private            = local_is_volume_private
};


