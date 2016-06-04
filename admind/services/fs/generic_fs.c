/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/services/fs/generic_fs.h"

#include <string.h>
#include <errno.h>

#include "admind/include/service_lum.h"
#include "admind/include/service_vrt.h"
#include "admind/services/fs/service_fs.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_fs.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"
#include "admind/src/adm_workthread.h"
#include "log/include/log.h"
#include "fs/include/exa_fsd.h"
#include "os/include/os_random.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "vrt/virtualiseur/include/vrt_client.h"

#ifdef WITH_MONITORING
#include "monitoring/md_client/include/md_notify.h"
#endif

#define MY_SERVICE_ID &adm_service_fs

extern fs_definition_t gfs_definition;
extern fs_definition_t local_definition;

/** List of all FS types */
static const fs_definition_t *fs_definition[] = {
  &gfs_definition,
  &local_definition,
  NULL,
};

/** Checking a FS */
typedef struct generic_check {
  fs_data_t fs;                                                /**< FS to check */
  char optional_parameters[EXA_MAXSIZE_FSCHECK_PARAMETER+1];   /**< "-F -..." */
  char output_file[EXA_MAXSIZE_MOUNTPOINT+1];                  /**< /tmp/output_fsck.5587 */
  exa_nodeid_t node;                                           /**< the node name on which the action applies */
  bool repair;
} generic_check_t;

/** Resizing a FS */
typedef struct resize_info_t {
  fs_data_t fs;        /*< File system to be resized */
  exa_nodeid_t node;   /*< Node that performs the resize */
} resize_info_t;

/**
 * \brief Give the next definition in the table list.
 *
 * \param[in] definition  Next definition to find. NULL if first.
 *
 * \return NULL if no other, or the next one.
*/
const fs_definition_t* fs_iterate_over_fs_definition(const fs_definition_t* current)
{
  if (current == NULL)
    {
      return fs_definition[0];
    }
  else
    {
      int index=0;
      while (fs_definition[index] != current) index++;
      return fs_definition[index+1];
    }
}

/**
 * \brief Helper function; returns true if the device is offline on some node.
 *
 * \return the status of the data device, as seen by the VRT
 */
int fs_get_data_device_offline(admwrk_ctx_t *ctx, fs_data_t* fs)
{
  int offline = false;
  int ret;

  admwrk_run_command(ctx, &adm_service_fs, RPC_SERVICE_FS_DEVICE_STATUS,
		     &fs->volume_uuid, sizeof(fs->volume_uuid));
  while(admwrk_get_ack(ctx, NULL, &ret))
    if (ret)
      offline = true;

  return offline;
}

/**
 * \brief Local function; What's the status of a volume ?
 *
 * \return True/False
 */
void fs_get_data_device_status_local(admwrk_ctx_t *ctx, void *msg)
{
  exa_uuid_t *uuid = msg;
  struct adm_volume *volume = adm_cluster_get_volume_by_uuid(uuid);

  if (volume->started)
    admwrk_ack(ctx, volume->group->offline);
  else
    admwrk_ack(ctx, false);
}

/** \brief Helper function; start the device.
 *
 * \param [in] fs:  FS for which the data device must be started.
 * \param [in] list : nodes to start
 *
 * \return : error code from device start
 */
exa_error_code fs_data_device_start(admwrk_ctx_t *ctx, fs_data_t* fs, const exa_nodeset_t* list, uint32_t readonly)
{
  struct adm_volume *volume = fs_get_volume(fs);
  return vrt_master_volume_start(ctx, volume, list, readonly,
                                 false /* print_warning */);
}

/**
 * \brief Helper function; stop the device.
 *
 * \param [in] fs:  FS for which the data device must be started.
 * \param [in] list : nodes to stop
 * \param [in] goal_change : need to change device goal ?
 * \param [in] force : continue in case of error ?
 *
 * \return : error code from device stop
 */
exa_error_code fs_data_device_stop(admwrk_ctx_t *ctx, fs_data_t* fs,
                                   const exa_nodeset_t *list,
                                   bool force,
                                   adm_goal_change_t goal_change)
{
  struct adm_volume *volume = fs_get_volume(fs);
  int err;

  if (!volume)
      return EXA_SUCCESS;

  err = lum_master_export_unpublish(ctx, &volume->uuid, list, force);
  if (err != EXA_SUCCESS)
      return err;

  return vrt_master_volume_stop(ctx, volume, list, force, goal_change,
                                false /* print_warning */);
}

/**
 * \brief Fill a generic filesystem structure from the adm_fs struct. Only generic
 *        fields are filled. This function calls specific functions to fill specific data
 *
 * \param[out] struct_fs   to be filled
 * \param[in]  fs          from
 *
 * \return : none.
 */
void fs_fill_data_from_config(fs_data_t* struct_fs, struct adm_fs *fs)
{
  const fs_definition_t *ops;

  EXA_ASSERT(fs);

  memset(struct_fs, 0, sizeof(fs_data_t));
  struct_fs->sizeKB = fs->size;
  strlcpy(struct_fs->fstype, fs->type, sizeof(struct_fs->fstype));
  strlcpy(struct_fs->mountpoint, fs->mountpoint, sizeof(struct_fs->mountpoint));
  strlcpy(struct_fs->mount_option, fs->mount_option, sizeof(struct_fs->mount_option));
  struct_fs->transaction = fs->committed;
  uuid_copy(&struct_fs->volume_uuid, &fs->volume->uuid);
  exa_nodeset_copy(&struct_fs->goal_started, &fs->volume->goal_started);
  exa_nodeset_copy(&struct_fs->goal_started_ro, &fs->volume->goal_readonly);
  adm_volume_path(struct_fs->devpath, sizeof(struct_fs->devpath),
		  fs->volume->group->name,
		  fs->volume->name);
  ops = fs_get_definition(struct_fs->fstype);
  EXA_ASSERT(ops);
  EXA_ASSERT(ops->fill_data_from_config);
  ops->fill_data_from_config(struct_fs, fs);
}

/**
 * \brief Helper function : retrieve the definition of a filesystem type using
 *          its type name.
 *
 * \param [in] name: type name to search
 *
 * \return fs_definition_t* : the definition pointer found or null if it does
 *                            not exist.
 */
const fs_definition_t* fs_get_definition(const char* name)
{
  const fs_definition_t **iter;

  if (name != NULL)
    for (iter = fs_definition; *iter != NULL; ++iter)
    {
      const char** current_name = (*iter)->type_name_list;
      while (*current_name)
      {
	if (strcmp(name, *current_name) == 0)
	  return *iter;

	current_name++;
      }
    }

  return NULL;
}

/**
 * Local function; Grow a FS.
 */
void generic_fs_mounted_grow_local(admwrk_ctx_t *ctx, void *msg)
{
  resize_info_t *info = msg;
  int ret = EXA_SUCCESS;

  exalog_debug("generic_resize_fs_local");

  if (info->node == adm_my_id)
    ret = fsd_resize(adm_wt_get_localmb(),
		     info->fs.fstype,
		     info->fs.devpath,
		     info->fs.mountpoint,
		     0);

  ret = admwrk_barrier(ctx, ret, "FS: Resize a mounted file system");
  admwrk_ack(ctx, ret);
}

/**
 * \brief Grow a file system that needs to be mounted RW to be resized.
 *
 * \param[in] fs_to_grow  Data of the file system to grow.
 * \param[in] new_size    How much to grow ?
 *
 * \return 0 on success or an error code.
 */
exa_error_code generic_fs_mounted_grow(admwrk_ctx_t *ctx, fs_data_t* fs_to_grow,
				       int64_t new_size)
{
  int ret;
  exa_nodeset_t list_mounted_rw, list_mounted_ro;
  resize_info_t info;
  struct adm_volume *volume = fs_get_volume(fs_to_grow);
  struct vrt_volume_info volume_info;

  /* Check that the fs is mounted RW somewhere. */
  ret = generic_fs_get_started_nodes(ctx, fs_to_grow, &list_mounted_rw, &list_mounted_ro);
  exa_nodeset_substract(&list_mounted_rw, &list_mounted_ro);
  if (exa_nodeset_is_empty(&list_mounted_rw))
    return -FS_ERR_RESIZE_NEED_MOUNT_RW;

  /* Do not allow shrink */
  if (new_size != 0 && new_size < fs_to_grow->sizeKB)
    return -FS_ERR_RESIZE_SHRINK_NOT_POSSIBLE;

  /* resize the volume */
  ret = vrt_master_volume_resize(ctx, volume, new_size);
  if (ret) return ret;

  /* print a nice message and resize the filesystem */
  adm_write_inprogress(adm_nodeid_to_name(adm_my_id),
		       "Resizing in progress, it may take time",
		       EXA_SUCCESS, "");
  memcpy(&info.fs, fs_to_grow, sizeof(info.fs));
  info.node=exa_nodeset_first(&list_mounted_rw);
  ret = admwrk_exec_command(ctx, MY_SERVICE_ID, RPC_SERVICE_FS_GENERIC_GROW, &info, sizeof(info));
  if (ret!=EXA_SUCCESS) return ret;

  /* Save the new size in the configuration */
  ret = vrt_client_volume_info(adm_wt_get_localmb(),
                               &volume->group->uuid,
                               &volume->uuid,
                               &volume_info);
  EXA_ASSERT(ret == EXA_SUCCESS);
  fs_to_grow->sizeKB = volume_info.size;

  return EXA_SUCCESS;
}

/**
 * \brief Local function; Make a fsck of a filesystem.
 */
void generic_fs_check_local(admwrk_ctx_t *ctx, void *msg)
{
  generic_check_t *check = msg;
  int error_val = EXA_SUCCESS;

  if (check->node == adm_my_id)
    {
      error_val = fsd_check(adm_wt_get_localmb(), &check->fs,
			    check->optional_parameters,
			    check->repair, check->output_file);
    }
  error_val = admwrk_barrier(ctx, error_val, "Filesystem check");
  admwrk_ack(ctx, error_val);
}

/**
 * Check a FS, cluster function.
 *
 * \param[in] optional_parameter  optional parameter to pass to fsck.
 * \param[in] node_where_to_check Node used to check the filesytem on.
 * \param[in] repair              Repair or just check ?
 *
 * \return 0 on success or an error code.
 */
exa_error_code generic_fs_check(admwrk_ctx_t *ctx, fs_data_t* fs_to_test,
				const char* optional_parameters,
				exa_nodeid_t node_where_to_check,
				bool repair)
{
  int ret;
  exa_nodeset_t nodelist_where_to_check;
  generic_check_t check;
  char message_string[EXA_MAXSIZE_LINE+1];
  uint32_t output_file_id;

  exa_nodeset_reset(&nodelist_where_to_check);
  exa_nodeset_add(&nodelist_where_to_check,node_where_to_check);

  /* Check the FS is stopped on all nodes. */
  if (!exa_nodeset_is_empty(&fs_to_test->goal_started))
    {
      return -FS_ERR_FSCK_STARTED;
    }

  /* Start the data volume */
  ret = fs_data_device_start(ctx, fs_to_test,
			     &nodelist_where_to_check, false /* readonly*/);
  if (ret)
    {
      /* Error : try to stop volume */
      fs_data_device_stop(ctx, fs_to_test,
                          &nodelist_where_to_check,
                          false /*force */,
                          ADM_GOAL_CHANGE_VOLUME);
      return ret;
    }

  /* Compute a unique output file name */
  os_get_random_bytes(&output_file_id, sizeof(output_file_id));
  os_snprintf(check.output_file, sizeof(check.output_file),
           "/tmp/output_fsck.%u", output_file_id);

  /* Compute a polite message string to give back t othe user about command error log */
  os_snprintf(message_string, sizeof(message_string),
	   "You can check out the result of the command on (%s)%s ",
	   adm_nodeid_to_name(node_where_to_check),check.output_file);

  /* (nicely) Ask exa_fsd to run fsck. */
  memcpy(&check.fs, fs_to_test, sizeof(check.fs));
  strlcpy(check.optional_parameters, optional_parameters, sizeof(check.optional_parameters));
  check.node = node_where_to_check;
  check.repair = repair;
  adm_write_inprogress(adm_nodeid_to_name(adm_my_id), message_string,
		       EXA_SUCCESS, message_string);

  ret = admwrk_exec_command(ctx, MY_SERVICE_ID, RPC_SERVICE_FS_GENERIC_CHECK,
			    &check, sizeof(check));
  if (ret)
    {
      fs_data_device_stop(ctx, fs_to_test,
                          &nodelist_where_to_check,
                          false /* force */,
                          ADM_GOAL_CHANGE_VOLUME);
      return ret;
    }

  /* Stop the volume */
  ret = fs_data_device_stop(ctx, fs_to_test,
                            &nodelist_where_to_check,
                            false /* force */,
                            ADM_GOAL_CHANGE_VOLUME);

  return ret;
}

/**
 * \brief Tune a filesystem, setting one of the possible options.
 *
 * \param[in] parameter           Option to change.
 * \param[in] value               Set to this value.
 *
 * \return 0 on success or an error code.
 */
exa_error_code generic_fs_tune(admwrk_ctx_t *ctx, fs_data_t* fs,
			       const char* parameter, const char* value)
{
  if (!strcmp(parameter, EXA_FSTUNE_MOUNT_OPTION))
    {
      int ret_update;

      /* Check no node has the FS mounted somewhere */
      if (exa_nodeset_count(&fs->goal_started) != 0)
	{
	  return -FS_ERR_MOUNT_OPTION_NEEDS_FS_STOPPED;
	}

      if ( strspn (value, EXA_FS_MOUNT_OPTION_ACCEPTED_CHARS)
	   != strlen(value))
	{
	  return -FS_ERR_FORMAT_MOUNT_OPTION;
	}
      strlcpy(fs->mount_option, value, sizeof(fs->mount_option));
      ret_update = fs_update_tree(ctx, fs);
      return ret_update;
    }
  return -FS_ERR_UNKNOWN_TUNE_OPTION;
}

/**
 * \brief Check that stop is possible
 *
 * \param[in] nodes	  Set of nodes
 * \param[in] fs          Filesystem data information
 * \param[in] force       If group is offline and force is set, accept command.
 *
 * \return 0 on success or an error code.
 */
exa_error_code generic_fs_check_before_stop(admwrk_ctx_t *ctx, const exa_nodeset_t* nodes,
					    fs_data_t* fs,bool force)
{
  EXA_ASSERT(nodes);
  /* Get the list of nodes really mounted */
  if (fs_get_data_device_offline(ctx, fs))
  {
    if (! force)
      {
	/* Get the list of nodes really mounted */
	exa_nodeset_t mounted_nodes_list;

	generic_fs_get_started_nodes(ctx, fs, &mounted_nodes_list, NULL);

	/* If the nodes to stop are mounted and the force flag is not set, this is an error */
        if (!exa_nodeset_disjoint(&mounted_nodes_list, nodes))
	    return -FS_ERR_STOP_WITH_VOLUME_DOWN;
      }
  }
  return EXA_SUCCESS;
}

/**
 * \brief Compute the flags to give to FSD at start time, based on whether it's a recovery,
 * the device and its status.
 *
 * \param[in] recovery        Is it a recovery ?
 * \param[in] volume_offline  What's the device status ?
 *
 * \return the flags, i.e "allow_mount" or "allow_remount"
 */
static uint64_t generic_fs_recovery_compute_option(bool recovery, int volume_offline)
{
  uint64_t flags = 0;
  if (!recovery)
    {
      /* Allow all types of mount/remount */
      flags = FS_ALLOW_MOUNT | FS_ALLOW_REMOUNT;
    }
  else
    {
      if (volume_offline)
	{
	  /* Device is DOWN */
	  flags = FS_ALLOW_REMOUNT;
	  exalog_warning("Volume has become DOWN : if mounted, it will be remounted RO.");
	}
      else
	{
	  /* Don't do remount on a volume that got DOWN and was mounted at some point in time;
	   instead, allow to mount a volume UP never mounted before */
	  flags = FS_ALLOW_MOUNT;
	}
    }
  exalog_debug("Computing mount options : recovery %i offline %i result %"PRIu64,
	       recovery, volume_offline, flags);
  return flags;
}

/**
 * \brief Get the real list of started nodes for a given filesystem.
 *
 * \param[in]  fs_to_test FS instance to really test.
 * \param[out] nodes_list Complete list of nodes that have mounted the main device.
 * \param[out] nodes_list_ro Complete list of nodes that have mounted the main device, in RO mode.
 *             possibly NULL.
 * \param[out] Error code, if barrier is broken, EXA_SUCCESS otherwise.
 *
 * \return 0 on success or an error code.
 */
exa_error_code generic_fs_get_started_nodes(admwrk_ctx_t *ctx,fs_data_t* fs_to_test,
					    exa_nodeset_t* nodes_list,
					    exa_nodeset_t* nodes_list_ro)
{
  exa_nodeid_t nodeid;
  exa_error_code is_broken=EXA_SUCCESS;
  int error_val;
  struct fs_info_reply fs_reply;
  fs_request_t fs_request;
  const fs_definition_t* fs_definition=fs_get_definition(fs_to_test->fstype);

  exa_nodeset_reset(nodes_list);
  if (nodes_list_ro)
    exa_nodeset_reset(nodes_list_ro);
  EXA_ASSERT_VERBOSE(fs_definition, "Unknown file system type found.");

  /* Run a clustered command to get the state on all nodes and get each local status */
  strlcpy(fs_request.mountpoint, fs_to_test->mountpoint, sizeof(fs_request.mountpoint));
  strlcpy(fs_request.devpath, fs_to_test->devpath, sizeof(fs_request.devpath));
  exa_nodeset_reset( &fs_request.nodeliststatfs );
  admwrk_run_command(ctx, MY_SERVICE_ID, RPC_ADM_CLINFO_FS, &fs_request, sizeof(fs_request));
  while (admwrk_get_reply(ctx, &nodeid, &fs_reply, sizeof(fs_reply), &error_val))
    {
      if (error_val == -ADMIND_ERR_NODE_DOWN)
	{
	  is_broken=-ADMIND_ERR_NODE_DOWN;
	  continue;
	}
      /* handle FS used */
      if (fs_reply.mounted)
	{
	  exa_nodeset_add(nodes_list, nodeid);
	  if ((fs_reply.mounted == EXA_FS_MOUNTED_RO) && (nodes_list_ro))
	    {
	      exa_nodeset_add(nodes_list_ro, nodeid);
	    }
	}
    }
  return is_broken;
}

/**
 * \brief Return the next tuning parameter.
 *
 * param[in]  fs    represents the FS whose tuning must be caught.
 * param[out] tune  structure to fill with the name/value pair
 * param[out] error            error code.
 *
 * \return false if it was the last name or an error occurred, true instead
 *         and fill tune_name_value
 */
bool generic_fs_gettune(admwrk_ctx_t *ctx, fs_data_t* fs,
			      struct tune_t* tune, int* error)
{
  *error = EXA_SUCCESS;
  if (strcmp(tune_get_name(tune), "") == 0)
    {
      tune_set_name(tune, EXA_FSTUNE_MOUNT_OPTION);
      tune_set_nth_value(tune, 0, "%s", fs->mount_option);
      tune_set_description(tune, "FS will be mounted with those options");
      return true;
    }
  return false;
}

/**
 * \brief Return the adm_volume containing the FS.
*/
struct adm_volume* fs_get_volume(fs_data_t* fs)
{
  return adm_cluster_get_volume_by_uuid(&fs->volume_uuid);
}

/**
 * \brief Return the Name of the FS.
 *
 * \param[in] fs     Name to retrieve
 *
 * \return name as a char* that mustn't be deallocated.
*/
const char* fs_get_name(fs_data_t* fs)
{
  return fs_get_volume(fs)->name;
}


/**
 * Local function; prepare/unload/mount/unmount the filesystem
 */
void generic_startstop_fs_local(admwrk_ctx_t *ctx, void *msg)
{
  startstop_info_t *info = msg;
  int error_val= EXA_SUCCESS;
  int error_barrier = EXA_SUCCESS;
  const fs_definition_t* fs_definition;

  exalog_debug("generic_startstop_fs_local: fsd_mount "
	       "is node in list: %i action %"PRId64" fs '%s'",
	       adm_nodeset_contains_me(&info->nodes),
	       info->action, fs_get_name(&info->fs));

  /* prepare */
  if (info->action == startstop_action_prepare)
    {
      error_val = fsd_prepare(adm_wt_get_localmb(), &info->fs);
      if (!error_val)
	{
	  fs_definition = fs_get_definition(info->fs.fstype);
	  EXA_ASSERT(fs_definition);
	  if (fs_definition->post_prepare)
	    {
	      fs_definition->post_prepare();
	    }
	}
      error_val = admwrk_barrier(ctx, error_val, "FS: Loading daemons and modules");
    }

  /* mount */
  if (info->action == startstop_action_mount)
    {
      if (adm_nodeset_contains_me(&info->nodes))
	{
	  int read_only, allow_mount_remount;

	  read_only = info->read_only || fs_get_volume(&info->fs)->group->offline;
	  /* compute action to perform, based on volume's status */
	  allow_mount_remount =
	    generic_fs_recovery_compute_option(info->recovery,
					       fs_get_volume(&info->fs)->group->offline);

	  error_val = fsd_mount(adm_wt_get_localmb(), &info->fs,
				read_only, allow_mount_remount,
				fs_get_volume(&info->fs)->group->name,
				fs_get_name(&info->fs));

	  /* Return a WARNING if the volume is offline */
	  if (fs_get_volume(&info->fs)->group->offline)
	  {
	      if (error_val == EXA_SUCCESS)
	      {
#ifdef WITH_MONITORING
		  /* send a trap to notify mount ro */
		  md_client_notify_filesystem_readonly(adm_wt_get_localmb(),
						       &info->fs.volume_uuid,
						       fs_get_volume(&info->fs)->group->name,
						       fs_get_name(&info->fs));
#endif
		  error_val = -FS_WARN_MOUNTED_READONLY;
	      }
	      else
	      {
#ifdef WITH_MONITORING
		  /* send a trap to notify fs offline */
		  md_client_notify_filesystem_offline(adm_wt_get_localmb(),
						      &info->fs.volume_uuid,
						      fs_get_volume(&info->fs)->group->name,
						      fs_get_name(&info->fs));
#endif
	      }
	  }
	}
      error_barrier = admwrk_barrier(ctx, error_val,
				     "FS: Mounting the filesystem");
    }

  /* umount */
  if (info->action == startstop_action_umount)
    {
    if (adm_nodeset_contains_me(&info->nodes))
	{
	  error_val = fsd_umount(adm_wt_get_localmb(), &info->fs,
				 fs_get_volume(&info->fs)->group->name,
				 fs_get_name(&info->fs));
	  if (error_val) error_val = -FS_ERR_UMOUNT_ERROR;
	}
      error_barrier = admwrk_barrier(ctx, error_val,
				     "FS: Unmounting the filesystem");
    }

  /* unload*/
  if (info->action == startstop_action_unload)
    {
      fs_definition = fs_get_definition(info->fs.fstype);
      EXA_ASSERT(fs_definition);
      if (fs_definition->pre_unload)
	{
	  fs_definition->pre_unload();
	}
      error_val = fsd_unload(adm_wt_get_localmb(), &info->fs);

      error_barrier = admwrk_barrier(ctx, error_val,
				     "FS: Unloading daemons and modules");
      if (error_barrier == -ADMIND_ERR_NODE_DOWN)
	{
	  error_val=error_barrier;
	}
    }

  admwrk_ack(ctx, error_val);
}
