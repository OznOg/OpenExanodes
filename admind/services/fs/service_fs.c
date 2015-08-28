/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>
#include "admind/services/fs/service_fs.h"
#include "admind/services/fs/type_clustered.h"
#include "admind/services/fs/type_gfs.h"
#include "admind/include/service_fs_commands.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_fs.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_monitor.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "common/include/exa_config.h"
#include "common/include/exa_env.h"
#include "os/include/strlcpy.h"
#include "os/include/os_daemon_parent.h"
#include "log/include/log.h"
#include "vrt/virtualiseur/include/vrt_client.h"

#ifdef USE_YAOURT
#include "admind/src/adm_workthread.h"
#include <yaourt/yaourt.h>
#endif

static int fs_init(int thr_nb)
{
  char admind_pid_str[8 /* enougth for a pid */ ];
  char fsd_path[OS_PATH_MAX];
  char *const argv[] = {
      fsd_path,
      "-h", adm_myself()->hostname,
      "-a", admind_pid_str,
      NULL
  };
  os_daemon_t fs_daemon;

  exalog_debug("fs init");

  os_snprintf(admind_pid_str, sizeof(admind_pid_str),
              "%"PRIu32, os_daemon_current_pid());

  exa_env_make_path(fsd_path, sizeof(fsd_path), exa_env_sbindir(), "exa_fsd");

  /* Start the fs daemon */
  if (os_daemon_spawn(argv, &fs_daemon) != 0)
    return -ADMIN_ERR_FSDSTART;

  adm_monitor_register(EXA_DAEMON_FSD, fs_daemon);
  gfs_init();
  return EXA_SUCCESS;
}

/**
 * \brief Initialize an FS iterator.
 */

void init_fs_iterator(fs_iterator_t* iter)
{
  iter->fs = NULL;
  memset(&iter->fs_data, 0, sizeof(fs_data_t));
}

/**
 * \brief Helper function : return the next FS volume instance that contains a FS
 */
static void get_next_fs_volume(fs_iterator_t* iter)
{
  struct adm_volume* current_volume = NULL;
  struct adm_group* current_group = NULL;
  if (iter->fs == NULL)
    {
      current_group = adm_group_get_first();
      current_volume = NULL;
    }
  else
    {
      current_group = iter->fs->volume->group;
      current_volume = iter->fs->volume;
    }
  iter->fs = NULL;
  while (current_group != NULL)
  {
      do {
          if (current_volume)
              current_volume = current_volume->next;
          else
              current_volume = current_group->volumes;

          if (current_volume)
              if (current_volume->filesystem)
              {
                  iter->fs = current_volume->filesystem;
                  return;
              }
      } while (current_volume != NULL);

      current_group = adm_group_get_next(current_group);
  }
}

/**
 * \brief Helper function : return the next FS instance
 */
static fs_data_t* get_next_fs(fs_iterator_t* iter)
{
  /* If it is just initialized, get first FS */
  get_next_fs_volume(iter);
  if (iter->fs != NULL)
    {
      fs_fill_data_from_config(&iter->fs_data, iter->fs);
      return &iter->fs_data;
    }
  return NULL;
}

/**
 * \brief Iterate over the next FS.
 *
 * \param [in] valid     : only valid file system, ie transaction=1
 * \param [in] type      : only file systems with this type
 * \param [in] started   : only file systems with goal started (this is != mounted)
 *
 * \return a pointer to a valid fs_data_t, or NULL. This pointer is in fact inside the iterator.
 */
fs_data_t* iterate_fs_instance(fs_iterator_t* iter, bool valid,
                               const char* type, bool started)
{
    while (1)
    {
        fs_data_t* current = get_next_fs(iter);

        if (current == NULL)
            return NULL;

        if (type && (strcmp(type, current->fstype)))
            continue;

        if (valid && (!current->transaction))
            continue;

        if (started && (!exa_nodeset_count(&current->goal_started)))
            continue;

        return current;
    }
}

/**
 * \brief This is the main function of the FS service recovery.
 *
 * \return EXA_SUCCESS; in case of error, returning something else than EXA_SUCCESS
 *         will make the hierarchy recovery fail.
 */
static int fs_recover(int thr_nb)
{
  /* nodes considered brought UP by the recovery.*/
  exa_nodeset_t nodes_up_in_progress;
  /* nodes considered brought DOWN by the recovery. */
  exa_nodeset_t nodes_down_in_progress;
  /* nodes committed up (fully functionnal) */
  exa_nodeset_t nodes_up;
  int error_val ;
  fs_iterator_t iter;
  fs_data_t* struct_fs;

  exalog_debug("fs recover");

  /* Build up the lists of nodes up and down */
  inst_get_nodes_going_up(&adm_service_fs, &nodes_up_in_progress);
  inst_get_nodes_going_down(&adm_service_fs, &nodes_down_in_progress);
  inst_get_nodes_up(&adm_service_fs, &nodes_up);

  error_val  = gfs_global_recover(thr_nb,
				  &nodes_up_in_progress,
				  &nodes_down_in_progress,
				  &nodes_up);
  if (error_val)
    return error_val;

  /* For each INSTANCE of filesystem, which currently has a running instance,
     run the LOCAL recovery function */
  init_fs_iterator(&iter);
  while ((struct_fs = iterate_fs_instance(&iter, true, NULL, true)) != NULL)
  {
    const fs_definition_t* definition;
    struct adm_volume *volume;
    int ret_recovery;

    definition = fs_get_definition(struct_fs->fstype);
    volume = fs_get_volume(struct_fs);

    if (! volume->group->started)
      continue;

    exalog_debug("fs recover : restart filesystem instance '%s'",
		 fs_get_name(struct_fs));
    ret_recovery = definition->specific_fs_recovery(thr_nb, struct_fs);
    if (ret_recovery == -ADMIND_ERR_NODE_DOWN)
      return ret_recovery;
  }

  /* FS Recovery's end */
  return EXA_SUCCESS;
}

/**
 * \brief FS service shutdown.
 *
 * \return Error if daemon failed to stop.
 */
static int
fs_shutdown(int thr_nb)
{
  int error_val;

  exalog_debug("fs shutdown");

  error_val = gfs_shutdown(thr_nb);
  if (error_val)
    {
      exalog_error("Execution of gfs_stop failed with error %d.", error_val);
      return error_val;
    }

  error_val = adm_monitor_terminate(EXA_DAEMON_FSD);
  if (error_val)
    return -ADMIN_ERR_FSDSTOP;

  adm_monitor_unregister(EXA_DAEMON_FSD);

  return EXA_SUCCESS;
}

/**
 * \brief Adding a node.
 *
 * \param [in] node : Node that will be added.
 *
 * \return error code. Should always be EXA_SUCCESS, otherwise the caller may break.
 */
static int
fs_nodeadd(int thr_nb, struct adm_node *node)
{
  int ret = EXA_SUCCESS;
  /* For each FS, if one is started with the given type, call nodeadd */
  const fs_definition_t *iter_type = NULL;
  while ((iter_type = fs_iterate_over_fs_definition(iter_type)) != NULL)
    {
      if (iter_type->node_add)
	{
	  fs_data_t* struct_fs;
	  fs_iterator_t iter;
	  const char* current_name = iter_type->type_name_list[0];
	  exalog_debug("fs_nodeadd name '%s'", current_name);
	  init_fs_iterator(&iter);
	  while ((struct_fs = iterate_fs_instance(&iter, true, current_name, true)) != NULL)
	    {
	      exalog_debug("fs_nodeadd call nodeadd");
	      ret = iter_type->node_add(thr_nb, node);
	      if (ret != EXA_SUCCESS) return ret;
	      break;
	    }
	}
    }
  return ret;
}

/**
 * \brief Adding a node and commit the modification.
 *
 * \param [in] node : Node that will be added.
 */
static void
fs_nodeadd_commit(int thr_nb, struct adm_node *node)
{
  /* For each FS, if one is started with the given type, call nodeadd */
  const fs_definition_t *iter_type = NULL;
  while ((iter_type = fs_iterate_over_fs_definition(iter_type)) != NULL)
    {
      if (iter_type->node_add_commit)
	{
	  const char* current_name = iter_type->type_name_list[0];
	  fs_data_t* struct_fs;
	  fs_iterator_t iter;
	  exalog_debug("fs_nodeadd_commit name '%s'", current_name);
	  init_fs_iterator(&iter);
	  while ((struct_fs = iterate_fs_instance(&iter, true, current_name, true)) != NULL)
	    {
	      exalog_debug("fs_nodeadd call nodeadd_commit");
	      iter_type->node_add_commit(thr_nb, node);
	      break;
	    }
	}
    }
  return;
}

/**
 * \brief Stopping a node. Depends on the filesystem type
 *
 * \param [in] nodes_to_stop  Nodes that will be stopped.
 *
 * \return Stop error. Should be always success.
 */
static int
fs_nodestop(int thr_nb, const exa_nodeset_t *nodes_to_stop)
{
  if (adm_nodeset_contains_me(nodes_to_stop))
    {
      fs_iterator_t iter;
      fs_data_t* struct_fs;
      init_fs_iterator(&iter);
      while ((struct_fs = iterate_fs_instance(&iter, true, NULL, true)) != NULL)
	{
	  const fs_definition_t* definition;
	  definition = fs_get_definition(struct_fs->fstype);
	  if (definition->node_stop == NULL)
	    continue;
	  definition->node_stop(thr_nb, struct_fs);
	}
    }
  else
  {
      exa_nodeset_t nodes_up;
      inst_get_nodes_up(&adm_service_fs, &nodes_up);
      gfs_manage_node_stop(nodes_to_stop, &nodes_up);
  }
  return EXA_SUCCESS;
}

static void
local_fs_stop(int thr_nb, void *msg)
{
  const exa_nodeset_t *nodeset = msg;
  int ret = fs_nodestop(thr_nb, nodeset);

  admwrk_ack(thr_nb, ret);
}

static int fs_stop(int thr_nb, const stop_data_t *stop_data)
{
  struct adm_group *group;

  /* FIXME this would probably be better to use an iterator on fs directly
   * and not use the vrt structures. */
  adm_group_for_each_group(group)
    {
      int ret = fs_stop_all_fs(thr_nb, group, &stop_data->nodes_to_stop,
	                       stop_data->force, stop_data->goal_change);
      if (ret != EXA_SUCCESS)
	return ret;
    }

  return admwrk_exec_command(thr_nb, &adm_service_fs, RPC_SERVICE_FS_STOP,
                             &stop_data->nodes_to_stop,
			     sizeof(stop_data->nodes_to_stop));
}

/**
 * \brief Deleting a node.
 *
 * \param [in] node   : Node to delete.
 */
static void fs_nodedel(int thr_nb, struct adm_node *node)
{
  exa_nodeset_t nodes_up;
  inst_get_nodes_up(&adm_service_fs, &nodes_up);
  gfs_nodedel(thr_nb, node, &nodes_up);
}

/**
 * \brief Helper function : Update a FS tree, using a clustered/localized command.
 *                          If it already exists, it is being updated. It it fails,
 *                          it goes back on all nodes.
 *
 * \param [in] fs             : fs pointer to a FS data that needs being created/updated.
 *
 * \return the error code if it fails, EXA_SUCCESS otherwise.
 */
exa_error_code fs_update_tree(int thr_nb,fs_data_t* fs_data)
{
  exa_error_code error_val = EXA_SUCCESS;

  /* Create the message for the local command */
  /* send event */
  error_val = admwrk_exec_command(thr_nb, &adm_service_fs, RPC_SERVICE_FS_CONFIG_UPDATE,
				  fs_data, sizeof(fs_data_t));
  if ( error_val != EXA_SUCCESS)
    {
      exalog_error("Error while FS tree update : %i", error_val);
    }
  return error_val;
}

/**
 * \brief Local command to create/update/delete a filesystem in the config file.
 */
static void local_exa_fs_config (int thr_nb, void *msg)
{
  struct adm_fs *fs = NULL;
  struct adm_volume* volume = NULL;
  fs_data_t* fs_info = msg;
  int error_val;
  int save_ret;

  exalog_debug("   local cmd_fsupdate (fsname=%s)\n", fs_get_name(fs_info));

  volume = fs_get_volume(fs_info);
  fs = volume->filesystem;

  if (fs == NULL)
    {
      fs = adm_fs_alloc();
      if (!fs)
	{
	  error_val = -ENOMEM;
	  goto barrier_error;
	}
      volume->filesystem = fs;
      fs->volume = volume;
    }

  fs->size = fs_info->sizeKB;
  strlcpy(fs->type, fs_info->fstype, sizeof(fs->type));
  strlcpy(fs->mountpoint, fs_info->mountpoint, sizeof(fs->mountpoint));
  strlcpy(fs->mount_option, fs_info->mount_option, sizeof(fs->mount_option));
  fs->committed = fs_info->transaction;
  fs_get_volume(fs_info)->filesystem = fs;
  if (strcmp(fs->type, FS_NAME_GFS) == 0)
    {
      strlcpy(fs->gfs_uuid, fs_info->clustered.gfs.uuid, sizeof(fs->gfs_uuid));
      fs->gfs_nb_logs = fs_info->clustered.gfs.nb_logs;
      fs->gfs_readahead = fs_info->clustered.gfs.read_ahead;
      fs->gfs_rg_size = fs_info->clustered.gfs.rg_size;
      fs->gfs_fuzzy_statfs = fs_info->clustered.gfs.fuzzy_statfs;
      fs->gfs_demote_secs = fs_info->clustered.gfs.demote_secs;
      fs->gfs_glock_purge = fs_info->clustered.gfs.glock_purge;
    }
  else
    strlcpy(fs->gfs_uuid, "", sizeof(fs->gfs_uuid));

  /* Ok, everything ran ok on all nodes : force a config file save */
  save_ret = conf_save_synchronous();
  EXA_ASSERT_VERBOSE(save_ret == EXA_SUCCESS, "%s", exa_error_msg(save_ret));

#ifdef USE_YAOURT
  yaourt_event_wait(examsgOwner(adm_wt_get_inboxmb(thr_nb)), "fs_transaction_set");
#endif

  /* normal bail-out */
  exalog_debug("fsupdate local command is complete");
  admwrk_ack(thr_nb, EXA_SUCCESS);
  return;

barrier_error:
  admwrk_ack(thr_nb, error_val);
}

extern void gfs_create_config_local_with_ack(int thr_nb, void *msg);
extern void generic_fs_mounted_grow_local(int thr_nb, void* msg);
extern void generic_startstop_fs_local(int thr_nb, void *msg);
extern void fs_get_data_device_status_local(int thr_nb, void *msg);
extern void manage_membership_on_shm_local(int thr_nb, void *msg);
extern void manage_gfs_local(int thr_nb, void *msg);
extern void update_cman_local(int thr_nb, void *msg);
extern void gfs_add_logs_local(int thr_nb, void *msg);
extern void gfs_update_tuning(int thr_nb, void *msg);

extern int gfs_check_nodedel(int thr_nb, struct adm_node *node);

const struct adm_service adm_service_fs =
{
  .id = ADM_SERVICE_FS,
  .init = fs_init,
  .recover = fs_recover,
  .shutdown = fs_shutdown,
  .nodeadd = fs_nodeadd,
  .nodeadd_commit = fs_nodeadd_commit,
  .nodedel = fs_nodedel,
  .stop    = fs_stop,
  .check_nodedel = gfs_check_nodedel,
  .local_commands =
  {
    { RPC_SERVICE_FS_STOP,                local_fs_stop                   },
    { RPC_SERVICE_FS_GENERIC_CHECK,       generic_fs_check_local          },
    { RPC_SERVICE_FS_GFS_CREATE_CONFIG,   gfs_create_config_local_with_ack},
    { RPC_SERVICE_FS_GENERIC_GROW,        generic_fs_mounted_grow_local   },
    { RPC_SERVICE_FS_STARTSTOP,           generic_startstop_fs_local      },
    { RPC_SERVICE_FS_CONFIG_UPDATE,       local_exa_fs_config             },
    { RPC_SERVICE_FS_SHM_UPDATE,          manage_membership_on_shm_local  },
    { RPC_SERVICE_FS_DEVICE_STATUS,       fs_get_data_device_status_local },
    { RPC_SERVICE_FS_HANDLE_GFS,          manage_gfs_local                },
    { RPC_SERVICE_FS_UPDATE_CMAN,         update_cman_local               },
    { RPC_SERVICE_FS_GFS_ADD_LOGS,        gfs_add_logs_local              },
    { RPC_SERVICE_FS_GFS_UPDATE_TUNING,   gfs_update_tuning               },
    { RPC_COMMAND_NULL, NULL }
  }
};
