/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "admind/services/fs/type_gfs.h"
#include "admind/services/fs/service_fs.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/shm.h>

#include "admind/include/service_vrt.h"
#include "admind/services/fs/type_clustered.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_fs.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/rpc.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/service_parameter.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_config.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "common/include/uuid.h"
#include "log/include/log.h"
#include "fs/include/exa_fsd.h"

#define MY_SERVICE_ID &adm_service_fs

#define EXA_MAXSIZE_GFS_CLUSTERNAME 8

/* Define the size of a GFS log */
#define GFS_LOG_SIZE_MB             128

extern const fs_definition_t gfs_definition;

typedef struct create_config_info_t {
  uint64_t generation_number;
} create_config_info_t;

/* Structure describing the shared memory segment used to talk to Gulm */
typedef struct fs_membership_shm_info_t {
  uint64_t revision;
  uint64_t gulm_master;
  struct nodeinfo
  {
    uint64_t node_state;
    uint64_t state_changed;
    char hostname[EXA_MAXSIZE_HOSTNAME+1];
  } nodes_info[EXA_MAX_NODES_NUMBER];
} fs_membership_shm_info_t;

fs_membership_shm_info_t* membership_shm_info= NULL;

#define GULM_UPDATE_TYPE_MASTERS   0
#define GULM_UPDATE_TYPE_UP_DOWN   1

/* Message sent to update the shared memory segment, and masters info,
   called for nodestop, start and recovery. */
typedef struct gfs_update_gulm_info_t {
  uint64_t gulm_update_type;
  exa_nodeset_t gulm_masters;
  exa_nodeset_t nodes_up_in_progress;
  exa_nodeset_t nodes_up_committed;
  exa_nodeset_t nodes_down_in_progress;
  exa_nodeid_t designated_gulm_master;
} gfs_update_gulm_info_t;

typedef struct gfs_update_cman_info_t {
  exa_nodeset_t nodes_down_in_progress;
} gfs_update_cman_info_t;

typedef struct gfs_add_logs_info_t {
  exa_nodeset_t nodes_mounted;
  fs_data_t fs;
  uint64_t number_of_logs;
} gfs_add_logs_info_t;

static bool loaded = false; /**< If daemons are loaded or not, to monitor them or not
				     (avoid monitoring twice, this would assert) **/

static uint64_t generation_number = 0;

static exa_nodeset_t gulm_masters;
static exa_nodeid_t designated_gulm_master = EXA_NODEID_NONE;

/* When we must not handle GFS anymore, because it may have received
   I/O errors. */
uint64_t fs_handle_gfs = 1;

/**
 * \brief Initialize internal data.
 */
void gfs_init(void)
{
  exa_nodeset_reset(&gulm_masters);
}


/**
 * \brief Return true if the lock protocol is Gulm.
 */
bool gfs_using_gulm(void)
{
  return !strcmp(adm_cluster_get_param_text(EXA_OPTION_FS_GFS_LOCK_PROTOCOL),
		 GFS_LOCK_GULM);
}

/**
 * \brief Return true if the lock protocol is CMAN/DLM.
 */
bool gfs_using_dlm(void)
{
  return !strcmp(adm_cluster_get_param_text(EXA_OPTION_FS_GFS_LOCK_PROTOCOL),
		 GFS_LOCK_DLM);
}


/**
 * \brief Return true if the lock protocol is gulm and
 *        nodeid is designated as a gulm lockserver (master or slave).
 */
bool gfs_is_gulm_lockserver(exa_nodeid_t nodeid)
{
    if (!gfs_using_gulm()) return 0;
    return exa_nodeset_contains (&gulm_masters, nodeid);
}



/**
 * \brief Return node id of designated gulm master.
 */
exa_nodeid_t gfs_get_designated_gulm_master(void)
{
    return designated_gulm_master;
}


/**
 * \brief Stop daemons when Exanodes exits. Avoid an ugly Gulm crash,
 *        even if it works, too.
 *
 * \return An error code *should* be returned, but it is not, because the caller
 *         doesn't accept to fail. Instead, it may assert.
 */
int gfs_shutdown(admwrk_ctx_t *ctx)
{
  if (gfs_using_gulm())
    {
      if (shmdt(membership_shm_info) != -1)
        exalog_debug("shmdt() failed, errno = %d", errno);
      membership_shm_info = NULL;
      designated_gulm_master = EXA_NODEID_NONE;
      generation_number = 0;
      if (loaded)
      {
	fs_iterator_t iter;
	fs_data_t* struct_fs;
	init_fs_iterator(&iter);
	struct_fs = iterate_fs_instance(&iter, true, FS_NAME_GFS, false);
	EXA_ASSERT_VERBOSE(struct_fs, "Gulm loaded and no Seanodes FS file system.");
	fsd_unload(adm_wt_get_localmb(), struct_fs);
	loaded = false;
      }
    }
  return EXA_SUCCESS;
}

/**
 * \brief Get a unique cluster name. This is done by taking 8 characters from
 *        the cluster UUID.
 *
 * \param[out] clustername   A table filled with the cluster unique name.
 *                           It must be at least EXA_MAXSIZE_GFS_CLUSTERNAME+1 size.
 *                           All EXA_MAXSIZE_GFS_CLUSTERNAME+1 characters are written.
 *
 * \return void. Asserts on error.
 */
static void gfs_get_cluster_name(char* clustername)
{
  char cluster_uuid[UUID_STR_LEN + 1];
  uuid2str(&adm_cluster.uuid, cluster_uuid);
  memcpy(clustername, cluster_uuid, EXA_MAXSIZE_GFS_CLUSTERNAME);
  clustername[EXA_MAXSIZE_GFS_CLUSTERNAME]='\0';
}

/**
 * \brief Generate a new UUID for a GFS filesystem. Used at filesystem creation
 *
 * \param[in] generic_fs   File system that needs a new UUID.
 *
 * \return void
 */
static void gfs_new_uuid(fs_data_t* generic_fs)
{
  char gfs_cluster_name[EXA_MAXSIZE_GFS_CLUSTERNAME+1];
  int index_uuid_string;
  exa_uuid_t fs_uuid;
  exa_uuid_str_t str_uuid;

  gfs_get_cluster_name(gfs_cluster_name);
  uuid_generate(&fs_uuid);
  uuid2str(&fs_uuid, str_uuid);
  for (index_uuid_string=0; index_uuid_string!= GFS_SIZE_STRING_CLUSTER_FS_ID; index_uuid_string++)
    {
      generic_fs->clustered.gfs.uuid[index_uuid_string]=
        gfs_cluster_name[index_uuid_string];
      generic_fs->clustered.gfs.uuid[index_uuid_string+1+GFS_SIZE_STRING_CLUSTER_FS_ID]=
        str_uuid[index_uuid_string];
    }
  generic_fs->clustered.gfs.uuid[GFS_SIZE_STRING_CLUSTER_FS_ID * 2 + 1]='\0';
  generic_fs->clustered.gfs.uuid[GFS_SIZE_STRING_CLUSTER_FS_ID]=':';
}

/**
 * \brief Parse specific GFS parameters.
 *
 * \param[in] fs          : File system data to fill with info.
 * \param[in] gfs_nb_logs : File system logs.
 * \param[in] rg_size     : Resource groups size.
 *
 * \return SUCCESS if all parameters were found and valid.
 */
static int gfs_parse_fscreate_parameters(fs_data_t *fs, int gfs_nb_logs, uint64_t rg_size)
{
  strlcpy(fs->clustered.gfs.lock_protocol,
	  adm_cluster_get_param_text(EXA_OPTION_FS_GFS_LOCK_PROTOCOL),
	  sizeof(fs->clustered.gfs.lock_protocol));

  if (gfs_nb_logs == -1)
    fs->clustered.gfs.nb_logs = adm_cluster_nb_nodes();
  else
    fs->clustered.gfs.nb_logs = gfs_nb_logs;

  fs->clustered.gfs.read_ahead = adm_cluster_get_param_int("default_readahead");
  fs->clustered.gfs.fuzzy_statfs = true;
  fs->clustered.gfs.demote_secs = adm_cluster_get_param_int("default_demote_secs");
  fs->clustered.gfs.glock_purge = adm_cluster_get_param_int("default_glock_purge");

  /* Add heuristic logic if RG_SIZE is not set */
  if (rg_size == 0)
  {
      uint64_t optimal_rg_size, size_per_node;
      size_per_node = fs->sizeKB / fs->clustered.gfs.nb_logs;
      /* try to get GFS_OPTIMAL_RG_PER_NODE RG per node */
      optimal_rg_size = size_per_node / GFS_OPTIMAL_RG_PER_NODE;
      /* This number should be in MB, not KB */
      optimal_rg_size = optimal_rg_size / 1024;
      /* Assign, and validate bounds */
      rg_size = optimal_rg_size;
      if (rg_size < GFS_MIN_RG_SIZE)
      {
	  rg_size = GFS_MIN_RG_SIZE;
      }
      if (rg_size > GFS_MAX_RG_SIZE)
      {
	  rg_size = GFS_MAX_RG_SIZE;
      }
      exalog_debug("RG Size parameters: optimal=%"PRIu64 ",rg_size=%"PRIu64,
		   optimal_rg_size, rg_size);
  }
  fs->clustered.gfs.rg_size = rg_size;

  return EXA_SUCCESS;
}

/**
 * \brief Fill the struct fs_data_t from the adm_fs struct
 *
 * \param[in] generic_fs   File system data that is filled.
 * \param[in] fs           File system config structure.
 *
 * \return void
 */
void gfs_fill_data_from_config(fs_data_t* generic_fs, struct adm_fs *fs)
{
  strlcpy(generic_fs->clustered.gfs.lock_protocol,
	  adm_cluster_get_param_text(EXA_OPTION_FS_GFS_LOCK_PROTOCOL),
	  sizeof(generic_fs->clustered.gfs.lock_protocol));
  generic_fs->clustered.gfs.read_ahead = fs->gfs_readahead;
  strlcpy(generic_fs->clustered.gfs.uuid, fs->gfs_uuid, sizeof(generic_fs->clustered.gfs.uuid));
  generic_fs->clustered.gfs.nb_logs = fs->gfs_nb_logs;
  generic_fs->clustered.gfs.rg_size = fs->gfs_rg_size;
  generic_fs->clustered.gfs.fuzzy_statfs = fs->gfs_fuzzy_statfs;
  generic_fs->clustered.gfs.demote_secs = fs->gfs_demote_secs;
  generic_fs->clustered.gfs.glock_purge = fs->gfs_glock_purge;
}


/**
 * @brief Create new uuid for the fs.
 *
 * @param[in] ctx      Thread number
 * @param[in] fs          File system data
 *
 * @return 0 upon success, an error code otherwise.
 */
static exa_error_code gfs_pre_create_fs(admwrk_ctx_t *ctx, fs_data_t* fs)
{
    gfs_new_uuid(fs);
    return EXA_SUCCESS;
}


/**
 * \brief Create the filesystem
 *
 * \param[in] fs          File system data
 *
 * \return 0 on success or an error code.
 */
static exa_error_code gfs_create_fs(admwrk_ctx_t *ctx, fs_data_t* fs)
{
    int ret;

    /* print a nice message before mkfs */
    adm_write_inprogress(adm_nodeid_to_name(adm_my_id),
                "mkfs is in progress, it may take time",
                EXA_SUCCESS, "");

    /* Mkfs */
    ret = fsd_fs_create_gfs(adm_wt_get_localmb(), fs);

    /* print a nice message with mkfs status */
    adm_write_inprogress(adm_nodeid_to_name(adm_my_id), "mkfs returned : ",
			 ret, "Unexpected error, mkfs failed");

    return ret;
}

/**
 * \brief Create configuration files needed for GFS
 *
 * \param[in] new_generation_number  version number to store in the config file.
 *                                   should be incremented at each change.
 *
 * \return 0 on success or an error code.
 */
static exa_error_code gfs_create_config_file_local(uint64_t new_generation_number)
{
  /* create /etc/cluster/cluster.conf */
  FILE* file_param=NULL;
  int error_val=EXA_SUCCESS;
  struct adm_node *node;
  char gfs_cluster_name[EXA_MAXSIZE_GFS_CLUSTERNAME+1];
  exa_nodeset_iter_t iter_gulm_master;
  exa_nodeid_t current_gulm_master;
  int number_write, error_close;

  gfs_get_cluster_name(gfs_cluster_name);

  /* Don't fail here. Fail only if fopen fails */
  mkdir("/etc/cluster", 777);

  if (!(file_param = fopen ("/etc/cluster/cluster.conf", "w")))
  {
    error_val = -EXA_ERR_OPEN_FILE;
    exalog_error("cannot open param file /etc/cluster/cluster.conf");
    goto error;
  }

  number_write=fprintf(file_param,
			   "<?xml version=\"1.0\"?>\n"
			   "<!-- THIS FILE IS AUTO GENERATED BY EXANODES -->\n"
			   "<cluster name=\"%s\" config_version=\"%"PRIu64"\">\n",
			   gfs_cluster_name, new_generation_number);
  if (number_write<=0) goto error_write;

  if (gfs_using_gulm())
    {
      number_write=fprintf(file_param, "<gulm>\n");
      if (number_write<=0) goto error_write;
      exa_nodeset_iter_init(&gulm_masters, &iter_gulm_master);
      while (exa_nodeset_iter(&iter_gulm_master, &current_gulm_master))
	{
	  struct adm_node* node_gulm_master;
	  node_gulm_master=adm_cluster_get_node_by_id(current_gulm_master);
	  number_write=fprintf(file_param,
			       "<lockserver name=\"%s\"/>\n",
			       node_gulm_master->hostname);
	  if (number_write<=0) goto error_write;
	}
      number_write=fprintf(file_param, "\n</gulm>\n");
      if (number_write<=0) goto error_write;
    }
  else if (gfs_using_dlm())
    {
      number_write=fprintf(file_param, "<cman/>\n");
      if (number_write<=0) goto error_write;
    }
  else
    {
        EXA_ASSERT_VERBOSE(false, "Unknown lock manager.");
    }
  number_write=fprintf(file_param, "<clusternodes>\n");
  if (number_write<=0) goto error_write;

  /* Build up the list of nodes */
  adm_cluster_for_each_node(node)
    {
      number_write=fprintf(file_param,
			       "<clusternode name=\"%s\">\n<fence>\n"
			       "<method name=\"human\">\n"
			       "<device name=\"exanodes\" node=\"%s\"/>\n"
			       "</method>\n"
			       "</fence>\n"
			       "</clusternode>\n",node->hostname,node->name);
      if (number_write<=0) goto error_write;
    }
  number_write=fprintf(file_param,
		       "</clusternodes>\n"
		       "<fence_daemon post_join_delay=\"30\"></fence_daemon>\n"
		       "<fencedevices>\n"
		       "<fencedevice name=\"exanodes\" agent=\"/bin/false\"/>\n"
		       "</fencedevices>"
		       "</cluster>\n");
  if (number_write<=0) goto error_write;
  error_close=fclose(file_param);
  if (error_close!=0)
    {
      error_val = -EXA_ERR_CLOSE_FILE;
      exalog_error("cannot close param file /etc/cluster/cluster.conf");
      goto error;
    }
  return EXA_SUCCESS;
 error:
  if (file_param)
    {
      fclose(file_param);
    }
  return error_val;

 error_write:
  exalog_error("cannot write param file /etc/cluster/cluster.conf");
  return -EXA_ERR_WRITE_FILE;

}

/**
 * \brief Stop a GFS filesystem. Handle the case where it is no more
 *        administrable.
 *
 * \param[in] nodes : list of nodes we want to stop.
 * \param[in] force : Continue in case of error ?
 * \param[in] goal_change : Do we need to change the goal of the
 *                          underlying volume ?
 * \param[in] stop_succeeded : the list of nodes where stop succeeded.
 *
 * \return Error code (example : umount failed)
 */
static exa_error_code gfs_stop_fs(admwrk_ctx_t *ctx, const exa_nodeset_t* nodes,
                                  fs_data_t *fs, bool force,
                                  adm_goal_change_t goal_change,
                                  exa_nodeset_t* stop_succeeded)
{
  if (!force && !fs_handle_gfs)
    {
      return -FS_ERR_HANDLE_GFS;
    }
  return clustered_stop_fs(ctx, nodes, fs, force, goal_change, stop_succeeded);
}

/**
 * \brief Start a GFS filesystem. Handle on-the-fly creation of configuration
 *        files. Calls "clustered" service for standard following ops.
 *
 * \param[in] nodes	          Set of nodes to start.
 * \param[in] nodes_read_only	  Subset of nodes mounted read-only. not a SUBSET of "nodes" !
 * \param[out] start_succeeded    list of nodes on which the mount action succeeded.
 * \param[in] recovery            Set to true if it is called from a recovery.
 *
 * \return 0 on success or an error code.
 */
static exa_error_code gfs_start_fs(admwrk_ctx_t *ctx, const exa_nodeset_t* nodes,
                                   const exa_nodeset_t* nodes_read_only,
                                   fs_data_t* fs,
                                   exa_nodeset_t* start_succeeded,
                                   int recovery)
{
  /* Create config files on all nodes. */
  int ret;
  exa_nodeset_t final_mounted_list;
  create_config_info_t config;

  exa_nodeset_reset(start_succeeded);

  if (!fs_handle_gfs)
    {
      return -FS_ERR_HANDLE_GFS;
    }

  /* Check at start-time if the Gulm masters list is valid */
  if (gfs_using_gulm())
    {
      if ((designated_gulm_master == EXA_NODEID_NONE) ||
	  (exa_nodeset_count(&gulm_masters) == 0) )
	{
	   return -FS_ERR_INVALID_GULM_MASTERS_LIST;
	}
    }

  config.generation_number = generation_number;
  ret = admwrk_exec_command(ctx, MY_SERVICE_ID, RPC_SERVICE_FS_GFS_CREATE_CONFIG,
			    &config, sizeof(create_config_info_t));
  if (ret!=EXA_SUCCESS) return ret;

  if (exa_nodeset_count(nodes) + exa_nodeset_count(nodes_read_only) == 0)
  {
    exa_nodeset_copy(start_succeeded, nodes);
    exa_nodeset_sum(start_succeeded, nodes_read_only);
    return EXA_SUCCESS;
  }

  exa_nodeset_copy(&final_mounted_list, &fs->goal_started);
  exa_nodeset_sum(&final_mounted_list, nodes);
  exa_nodeset_sum(&final_mounted_list, nodes_read_only);
  if (exa_nodeset_count(&final_mounted_list) > fs->clustered.gfs.nb_logs)
  {
    return -FS_ERR_NB_LOGS_EXCEEDED;
  }

  return clustered_start_fs(ctx, nodes, nodes_read_only, fs, start_succeeded, recovery);
}

/**
 * Resize the filesystem. A little magic here : if fuzzy_statfs is true, we need to
 * disable it, grow, then enable it.
 *
 * \param[in] fs          this SFS
 * \param[in] sizeKB      size in KB
 *
 * \return 0 on success or an error code.
 */
static exa_error_code gfs_resize_fs(admwrk_ctx_t *ctx, fs_data_t* fs, int64_t sizeKB)
{
  int ret;

  if (!fs_handle_gfs)
    {
      return -FS_ERR_HANDLE_GFS;
    }
  exalog_debug("gfs_resize_fs: sizeKB=%"PRId64, sizeKB);
  ret = generic_fs_mounted_grow(ctx, fs, sizeKB);
  if (ret)
    return ret;
  if (fs->clustered.gfs.fuzzy_statfs)
    {
      ret = admwrk_exec_command(ctx, &adm_service_fs,
				RPC_SERVICE_FS_GFS_UPDATE_TUNING,
				fs, sizeof(fs_data_t));
    }
  return ret;
}

/**
 * Called after "prepare" succeeded.
 */
static void gfs_post_prepare(void)
{
    if (!loaded && gfs_using_gulm())
	loaded = true;
}

/**
 * Called before "unloading". Stop monitoring gulm daemons.
 */
static void gfs_pre_unload(void)
{
  loaded = false;
}

/**
 * "unloading" daemons and modules. Stop monitoring gulm daemons.
 *
 * \return error code. Does not expect to fail, unless there's an error in FSD.
 */
static int gfs_node_stop(admwrk_ctx_t *ctx, fs_data_t* fs)
{
  if (!fs_handle_gfs)
    {
      return -FS_ERR_HANDLE_GFS;
    }
  exalog_debug("Stop all Seanodes FS modules and daemons");
  gfs_pre_unload();
  return fsd_unload(adm_wt_get_localmb(), fs);
}

/**
 * \brief Local function; Create config files. Called from the current node
 *        (not a clustered command)
 *
 * \param[in] new_generation_number Number that will be used for the message.
 * \param[in] node                  Additional node to add in the config
 *                                  (coming from nodeadd), or NULL.
 *
 * \return error code
 */
static int gfs_create_config_local(admwrk_ctx_t *ctx,
				   uint64_t new_generation_number,
				   bool barrier)
{
  int error_val = EXA_SUCCESS;
  exalog_debug("Create config files with version %"PRIu64, new_generation_number);
  error_val=gfs_create_config_file_local(new_generation_number);
  if (barrier)
    {
      error_val = admwrk_barrier(ctx, error_val, "FS: Create config files");
    }
  if (error_val != EXA_SUCCESS) /* Rollback to previous configuration */
    {
      gfs_create_config_file_local(generation_number);
    }
  else
    {
      /* Commit generation number */
      generation_number = new_generation_number;
    }
  return error_val;
}

/**
 * \brief Local function; Create config files. Called from the leader
 *        (a clustered command)
 */
void gfs_create_config_local_with_ack(admwrk_ctx_t *ctx, void *msg)
{
  int error_val = EXA_SUCCESS;
  create_config_info_t* config = msg;
  uint64_t new_generation_number = config->generation_number;
  error_val = gfs_create_config_local(ctx, new_generation_number, true);
  admwrk_ack(ctx, error_val);
}

/**
 * \brief Necessary job to add a node to the GFS config,
 *        and tell GFS about it.
 *        (this is called in parallel on all nodes)
 *        This function is really badly-designed, as is the interface for node_add.
 *        See "gfs_node_add_commit" below. TODO : please delete me.
 *
 * \param[in] node        Structure of the additional node. Not NULL.
 *
 * \return error code.
 */
static int gfs_node_add(admwrk_ctx_t *ctx, struct adm_node *node)
{
  return EXA_SUCCESS;
}

/**
 * \brief Necessary job to add a node to the GFS config,
 *        and tell GFS about it.
 *        (this is called in parallel on all nodes)
 *        It is not allowed to fail here. WHY ? See the whole node_add API.
 *
 * \param[in] node        Structure of the additional node. Not NULL.
 */
static void gfs_node_add_commit(admwrk_ctx_t *ctx, struct adm_node *node)
{
  int ret;
  uint64_t old_generation_number = generation_number;
  uint64_t new_generation_number = generation_number + 1;
  exalog_debug("Add a node for Seanodes FS.");

  /* GFS manages to update on all nodes, but we want to manage
     the version number of the GFS configuration number in a clustered manner.
     Reminder : GFS builds a CRC upon the config file, therefore it needs to be
     exactly the same on all nodes */
  ret = gfs_create_config_local(ctx, new_generation_number, true);

  if (ret != EXA_SUCCESS)
    return;

  if (adm_leader_id == adm_my_id)
    {
        ret = fsd_gfs_update_config(adm_wt_get_localmb());
    }
  ret = admwrk_barrier(ctx, ret, "FS: Update Seanodes FS");
  if (ret != EXA_SUCCESS)
    {
      /* Rollback to old config so new nodes won't be disappointed.
	 Don't perform a barrier, this may fail like before. */
      gfs_create_config_local(ctx, old_generation_number, false);
    }
  return;
}

/**
 * \brief Helper function. Set n first nodes of the config file as Gulm servers.
 *
 * \param[in] number    Number of gulm masters.
 */
static void set_n_first_nodes_as_gulm_masters(int number)
{
  int count=0;
  struct adm_node* node;
  exa_nodeset_reset(&gulm_masters);
  adm_cluster_for_each_node(node)
    {
      count++;
      if (count<=number) exa_nodeset_add(&gulm_masters, node->id);
      else break;
    }
}

/**
 * \brief Choose the Gulm master, amongst the list of masters.
 *
 * \param[in] nodes_up    Choose the gulm masters inside this list of nodes up.
 */
static void choose_designated_gulm_master(exa_nodeset_t* nodes_up)
{
  int found = false;
  exa_nodeset_iter_t iter_masters;
  exa_nodeid_t current_master;
  exa_nodeset_iter_init(&gulm_masters, &iter_masters);
  while (exa_nodeset_iter(&iter_masters, &current_master))
    {
      if (exa_nodeset_contains(nodes_up, current_master))
	{
	  /* This one is alive. Let it be the chosen one. */
	  designated_gulm_master = current_master;
	  found = true;
	  break;
	}
    }
  if (!found) /* No master is alive. We shouldn't send commands to
			 GFS here.*/
    {
      designated_gulm_master = EXA_NODEID_NONE;
    }
}

/**
 * \brief Choose the Gulm master, amongst the list of masters OR create the list
 *        if it doesn't exist.
 *
 * \param[in] nodes_up    Choose the gulm masters inside this list of nodes up.
 */
static void local_gulm_masters_selection(exa_nodeset_t* nodes_up)
{
  /* Recompute gulm_masters list if not already done or GFS not started.
     This function must be called on the leader of a newly formed cluster
     OR on a member of a previous cluster. */
  if ((exa_nodeset_count(&gulm_masters)==0) || (!is_clustered_started(FS_NAME_GFS)))
    {
      exa_nodeset_t gulm_masters_param_set;
      /* Get list if it exists. Otherwise, generate one with the first nodes */
      adm_cluster_get_param_nodeset(&gulm_masters_param_set, EXA_OPTION_FS_GULM_MASTERS);
      if (exa_nodeset_count(&gulm_masters_param_set) == 0) /* default == not set */
	{
	  if (adm_cluster_nb_nodes() < 3) /* 1,2 = 1 master */
	    {
	      set_n_first_nodes_as_gulm_masters(1);
	    }
	  else if  (adm_cluster_nb_nodes() < 5) /* 3,4 = 3 masters */
	    {
	      set_n_first_nodes_as_gulm_masters(3);
	    }
	  else /* 5, +  = 5 masters */
	    {
	      set_n_first_nodes_as_gulm_masters(5);
	    }

	  exalog_debug("Changing gulm masters list to generated list"
		       " '" EXA_NODESET_FMT "'", EXA_NODESET_VAL(& gulm_masters));
	}
      else
	{
          exalog_debug("Changing gulm masters list to parameterized list '" EXA_NODESET_FMT "'",
                       EXA_NODESET_VAL(&gulm_masters_param_set));
	  if ( (exa_nodeset_count(&gulm_masters_param_set) == 1) ||
	       (exa_nodeset_count(&gulm_masters_param_set) == 3) ||
	       (exa_nodeset_count(&gulm_masters_param_set) == 5))
	    {
	      exa_nodeset_copy(&gulm_masters, &gulm_masters_param_set);
	    }
	}
    }
  /* Choose a master : amongst the list of potential masters, if none is selected
     or the current one is dead (or will be), get one that is alive.
     Remember : it must work when GFS starts and during the recovery. */

  exalog_debug("Actual real gulm master '%i' is UP ? %i",
	       designated_gulm_master,
	       exa_nodeset_contains(nodes_up, designated_gulm_master));

  if ((designated_gulm_master == EXA_NODEID_NONE) /* First start */
      ||
      (!exa_nodeset_contains(nodes_up, designated_gulm_master))) /* Master is dead */
    {
      choose_designated_gulm_master(nodes_up);
    }
  exalog_debug("Real gulm master is '%i'", designated_gulm_master);
}

/**
 * \brief Choose the Gulm master, amongst the list of masters OR create the list
 *        if it doesn't exist, and send this to all other nodes.
 *
 * \param[in] nodes_up    Choose the gulm masters inside this list of nodes up.
 */
static int gulm_masters_selection(admwrk_ctx_t *ctx, exa_nodeset_t* nodes_up)
{
  int ret;
  gfs_update_gulm_info_t gulm_info;
  local_gulm_masters_selection(nodes_up);
  /* Now send this to all nodes, even if there are no changes on this node.
     This is mandatory for nodes coming UP. */
  gulm_info.gulm_update_type = GULM_UPDATE_TYPE_MASTERS;
  gulm_info.designated_gulm_master = designated_gulm_master;
  exa_nodeset_copy(&gulm_info.gulm_masters, &gulm_masters);
  ret = admwrk_exec_command(ctx, MY_SERVICE_ID, RPC_SERVICE_FS_SHM_UPDATE,
			    &gulm_info, sizeof(gfs_update_gulm_info_t));
  return ret;
}

/**
 * \brief  Global recovery for all GFS file systems.
 *         Manage shared memory segment.
 *
 * \param[in] nodes_up_in_progress   The nodes UP in the service
 * \param[in] nodes_down_in_progress The nodes DOWN in the service
 * \param[in] committed_up           The nodes already UP.
 *
 * \return NODE_DOWN or SUCCESS
 */
int gfs_global_recover(admwrk_ctx_t *ctx,
		       exa_nodeset_t* nodes_up_in_progress,
		       exa_nodeset_t* nodes_down_in_progress,
		       exa_nodeset_t* committed_up)
{
  int error_val = EXA_SUCCESS;
  gfs_update_gulm_info_t gulm_info;
  gfs_update_cman_info_t cman_info;
  exa_nodeset_t all_nodes_up;

  /* First, choose the gulm masters, before telling some nodes are UP or DOWN.
   Nodes UP are all nodes that are UP during the recovery. */
  if (gfs_using_gulm())
    {
      exa_nodeset_copy(&all_nodes_up, nodes_up_in_progress);
      exa_nodeset_sum(&all_nodes_up, committed_up);
      exa_nodeset_substract(&all_nodes_up, nodes_down_in_progress);
      error_val = gulm_masters_selection(ctx, &all_nodes_up);
      if (error_val == -ADMIND_ERR_NODE_DOWN)
	{
	  return error_val;
	}
    }

  /* If we handle GFS, check it is still allowed */
  if (fs_handle_gfs)
    {
      fs_iterator_t iter_gfs;
      fs_data_t* fs = NULL;
      init_fs_iterator(&iter_gfs);
      while ((fs = iterate_fs_instance(&iter_gfs, true, FS_NAME_GFS, true)) != NULL)
	{
	  int error;
	  /* Check the volume is DOWN, or there is no Gulm master anymore. */
	  if ((fs_get_data_device_offline(ctx, fs)) ||
	      ((designated_gulm_master == EXA_NODEID_NONE) && gfs_using_gulm()))
	    {
	      /* Check the FS is MOUNTED somewhere */
	      exa_nodeset_t nodes_list;
	      error = generic_fs_get_started_nodes(ctx, fs, &nodes_list, NULL);
	      if (error)
		{
		  return error;
		}
	      if (exa_nodeset_count(&nodes_list) != 0)
		{
		  /* We got DOWN with a GFS mounted somewhere. GFS cannot be handled anymore
		     till the next cluster reboot */
		  fs_handle_gfs = 0;
		}
	    }
	}
    }

  /* If we did check it wasn't allowed, or if this wasn't allowed
     from a previous recovery, tell nodes UP about this */
  if (!fs_handle_gfs)
    {
      /* Incoming nodes must set this flag, too */
      int error_val = admwrk_exec_command(ctx, MY_SERVICE_ID, RPC_SERVICE_FS_HANDLE_GFS,
					  &fs_handle_gfs, sizeof(fs_handle_gfs));
      /* The only acceptable error is NODE DOWN */
      if (error_val==-ADMIND_ERR_NODE_DOWN) return error_val;
    }


  /* Update the memory segments, to tell our Gulm friends to start their very special own recovery together */
  if (gfs_using_gulm())
    {
      exa_nodeset_copy(&gulm_info.nodes_up_in_progress, nodes_up_in_progress);
      exa_nodeset_copy(&gulm_info.nodes_down_in_progress, nodes_down_in_progress);
      exa_nodeset_copy(&gulm_info.nodes_up_committed, committed_up);
      exa_nodeset_substract( &gulm_info.nodes_up_committed, nodes_down_in_progress);
      gulm_info.gulm_update_type = GULM_UPDATE_TYPE_UP_DOWN;
      error_val = admwrk_exec_command(ctx, &adm_service_fs, RPC_SERVICE_FS_SHM_UPDATE,
				      &gulm_info, sizeof(gulm_info));
      exalog_debug("gfs recover : nodes up " EXA_NODESET_FMT " nodes down " EXA_NODESET_FMT
		   " shm update returned %i ",
		   EXA_NODESET_VAL(nodes_up_in_progress),
		   EXA_NODESET_VAL(nodes_down_in_progress),
		   error_val);
    }
  /* Tell CMAN if a node is gone */
  if (gfs_using_dlm() && (exa_nodeset_count(nodes_down_in_progress)))
    {
      exa_nodeset_copy(&cman_info.nodes_down_in_progress, nodes_down_in_progress);
      error_val = admwrk_exec_command(ctx, &adm_service_fs, RPC_SERVICE_FS_UPDATE_CMAN,
				      &cman_info, sizeof(cman_info));
      exalog_debug("gfs recover : nodes up " EXA_NODESET_FMT " nodes down " EXA_NODESET_FMT
		   " shm update returned %i ",
		   EXA_NODESET_VAL(nodes_up_in_progress),
		   EXA_NODESET_VAL(nodes_down_in_progress),
		   error_val);
      if (error_val == -ADMIND_ERR_NODE_DOWN)
	{
	  return error_val;
	}
      /* A "cman_tool killnode" error must be ignored, here. It would make the recovery fail */
      error_val = EXA_SUCCESS;
    }

  return error_val;
}

/**
 * \brief Stop a node other than ourself, and tell GFS about it.
 *        Manage Gulm master if necessary.(this is called in
 *        parallel on all nodes)
 *
 * \param[in] nodes_to_stop     Nodes that will be stopped.
 * \param[in] nodes_up          Nodes UP at this time.
 *
 * \return ERROR if we don't manage GFS anymore, or success
 */
int gfs_manage_node_stop(const exa_nodeset_t *nodes_to_stop, exa_nodeset_t* nodes_up)
{
  exa_nodeset_iter_t iter_node;
  exa_nodeid_t node_to_stop;
  exa_nodeset_t nodes_still_up;

  if (!fs_handle_gfs)
    {
      return -FS_ERR_HANDLE_GFS;
    }

  if (gfs_using_gulm() && membership_shm_info)
    /* maybe membership_shm_info is not inited if recovery did not happen (we are stopped) */
    {
      /* Choose a new master if the curent one goes DOWN.
	 This must be done before stopping the node.
	 Hopefully, all nodes in the cluster will have all info to
	 select the same new master*/
      exa_nodeset_copy(&nodes_still_up, nodes_up);
      exa_nodeset_substract(&nodes_still_up, nodes_to_stop);
      if (!exa_nodeset_contains(&nodes_still_up, designated_gulm_master))
	{
	  choose_designated_gulm_master(&nodes_still_up);
	  membership_shm_info->gulm_master = designated_gulm_master;
	}
      /* Time to tell GFS the node will stop */
      exa_nodeset_iter_init(nodes_to_stop, &iter_node);
      while (exa_nodeset_iter(&iter_node, &node_to_stop))
	{
	  membership_shm_info->nodes_info[node_to_stop].node_state=0;
	  membership_shm_info->nodes_info[node_to_stop].state_changed=1;
	  exalog_debug("manage_membership_on_shm_local STOP %i %s",
		       node_to_stop, membership_shm_info->nodes_info[node_to_stop].hostname);
	}
    }
  return EXA_SUCCESS;
}

/**
 * \brief Delete a node. It must be deleted from the Gulm masters list
 *        if necessary (recompute list).
 *
 * \param[in] node        Nodes that will be deleted.
 * \param[in] nodes_up    List of remaining nodes up. Used to choose
 *                        new masters' list.
 */
void gfs_nodedel(admwrk_ctx_t *ctx, struct adm_node *node, exa_nodeset_t* nodes_up)
{
  /* erase it from the shared segment, otherwise the same node name could appear
     2 times with different nodeid's on some nodes, and 1 time on others. */
  memset(membership_shm_info->nodes_info[node->id].hostname, 0,
	sizeof(membership_shm_info->nodes_info[node->id].hostname));
  /* Recompute master nodes list, will erase "designated_gulm_master" if it is deleted.
     This should NOT be called with Gulm started.
     On other cases, this is called on recovery, to update the shm before any start/stop,
     but the nodedel command is different, as it deeply changes the list of nodes. */
  exa_nodeset_reset(&gulm_masters);
  designated_gulm_master = EXA_NODEID_NONE;
  local_gulm_masters_selection(nodes_up);
}

/**
 * \brief Create shared memory segment for other clustered FS stacks.
 *        for every node.
 */
void manage_membership_on_shm_local(admwrk_ctx_t *ctx, void *msg)
{
  gfs_update_gulm_info_t* gulm_info = msg;
  int error_val = EXA_SUCCESS;
  exa_nodeset_iter_t iter_nodes;
  exa_nodeid_t current_node;
  struct adm_node *node;

  exalog_debug("manage_membership_on_shm_local");

  /* Initialize the shared memory segment with appropriate values */
  if (membership_shm_info == NULL)
    {
      int shmid;
      key_t fs_shm_key;
      char create_buffer[EXA_MAXSIZE_BUFFER];

      os_snprintf(create_buffer, EXA_MAXSIZE_BUFFER, "touch %s", EXA_SHARED_MEM_FILE_PATH);
      error_val = system(create_buffer);
      EXA_ASSERT_VERBOSE(error_val == 0, "failed to exec system(\"%s\")",
			 create_buffer);
      fs_shm_key = ftok(EXA_SHARED_MEM_FILE_PATH, 0);
      EXA_ASSERT_VERBOSE(fs_shm_key != -1 ,"Cannot initialize shared memory key");

      /* Try to attach shm, if it exists. */
      if ((shmid=shmget(fs_shm_key , sizeof(fs_membership_shm_info_t), IPC_CREAT | IPC_EXCL | 0600 ))==-1)
	{
	  /* It doesn't exist, create it */
	  EXA_ASSERT_VERBOSE(errno != -EEXIST,
			     "Error trying to get an existing shared memory segment : it doesn't exist.");
	  shmid=shmget(fs_shm_key, sizeof(fs_membership_shm_info_t) , 0);
	   /* It doesn't exist, create it */
	  EXA_ASSERT_VERBOSE(shmid != -1, "Error trying to get an existing shared memory segment");
	}
      membership_shm_info = (fs_membership_shm_info_t*)shmat(shmid, NULL, 0);
      EXA_ASSERT_VERBOSE( membership_shm_info != (fs_membership_shm_info_t*)-1,
			  "Error trying to attach shared memory segment");

      /* Fill with simple values */
      memset(membership_shm_info, 0, sizeof(fs_membership_shm_info_t));
    }

  adm_cluster_for_each_node(node)
  {
    strlcpy(membership_shm_info->nodes_info[node->id].hostname, node->hostname,
            sizeof(membership_shm_info->nodes_info[0].hostname));
  }

#ifdef DEBUG
  membership_shm_info->revision = EXANODES_GFS_COMPATIBILITY_NUMBER;
#else
  membership_shm_info->revision = 0;
#endif

  switch (gulm_info->gulm_update_type)
    {
    case GULM_UPDATE_TYPE_UP_DOWN:
      {
	exa_nodeset_iter_init(&gulm_info->nodes_up_in_progress, &iter_nodes);
	while (exa_nodeset_iter(&iter_nodes, &current_node))
	  {
	    membership_shm_info->nodes_info[current_node].node_state=1;
	    membership_shm_info->nodes_info[current_node].state_changed=1;
	    exalog_debug("manage_membership_on_shm_local UP %i %s",
			 current_node, membership_shm_info->nodes_info[current_node].hostname);
	  }
	exa_nodeset_iter_init(&gulm_info->nodes_down_in_progress, &iter_nodes);
	while (exa_nodeset_iter(&iter_nodes, &current_node))
	  {
	    membership_shm_info->nodes_info[current_node].node_state=0;
	    membership_shm_info->nodes_info[current_node].state_changed=1;
	    exalog_debug("manage_membership_on_shm_local DOWN %i %s",
			 current_node, membership_shm_info->nodes_info[current_node].hostname);
	  }
	exa_nodeset_iter_init(&gulm_info->nodes_up_committed, &iter_nodes);
	while (exa_nodeset_iter(&iter_nodes, &current_node))
	  {
	    membership_shm_info->nodes_info[current_node].node_state=1;
	  }
      } break;
    case GULM_UPDATE_TYPE_MASTERS:
      {
	exa_nodeset_copy(&gulm_masters, &gulm_info->gulm_masters);
	if (gulm_info->designated_gulm_master == EXA_NODEID_NONE)
	  {
	    /* Stop monitoring Gulm if admind was doing it. Otherwise it would Assert
	       if gulm exits unexpectedly */
	    gfs_pre_unload();
	  }
	designated_gulm_master = gulm_info->designated_gulm_master;
	membership_shm_info->gulm_master = gulm_info->designated_gulm_master;
      } break;
    default:
      EXA_ASSERT_VERBOSE(0, "Unknown type of command for gfs_update_gulm_info_t");
    }
  admwrk_ack(ctx, error_val);
}

/**
 * \brief Tell CMAN that there was a change in the membership.
 */
void update_cman_local(admwrk_ctx_t *ctx, void *msg)
{
    gfs_update_cman_info_t* info_cman = msg;
    int error_val = EXA_SUCCESS;
    int final_error_val = EXA_SUCCESS;
    exa_nodeset_iter_t iter_nodes;
    exa_nodeid_t current_node;
    exa_nodeset_iter_init(&info_cman->nodes_down_in_progress, &iter_nodes);
    while (exa_nodeset_iter(&iter_nodes, &current_node))
      {
	struct adm_node* node_to_kill;
	node_to_kill = adm_cluster_get_node_by_id(current_node);
	error_val = fsd_cman_update(adm_wt_get_localmb(), node_to_kill->hostname);
	/* XXX : What to do when it fails ? Continue, abort ? */
	if (error_val)
	  final_error_val = error_val;
      }
    admwrk_ack(ctx, final_error_val);
}

/**
 * \brief Ask one node to call sfs_jadd on a locally mounted fs.
 */
void gfs_add_logs_local(admwrk_ctx_t *ctx, void *msg)
{
  gfs_add_logs_info_t* info_fs = msg;
  int ret_logs = EXA_SUCCESS;
  int logs = 0;
  if (exa_nodeset_first(&info_fs->nodes_mounted) == adm_my_id)
    {
      logs = fsd_set_gfs_logs(adm_wt_get_localmb(), &info_fs->fs, info_fs->number_of_logs);
      if (logs != info_fs->number_of_logs)
	{
	  exalog_error("Unable to increase the number of logs. "
		       "Final log count is %d whereas it should be %"PRId64,
		       logs, info_fs->number_of_logs);
	  ret_logs = -FS_ERR_INCREASE_LOGS;
	}
    }
  admwrk_ack(ctx, ret_logs);
}

/**
 * \brief Ask SFS to set the new tuning values (RA, fuzzy_statfs)
 */
void gfs_update_tuning(admwrk_ctx_t *ctx, void *msg)
{
  fs_data_t* fs = (fs_data_t*)msg;
  int set_tuning = fsd_update_gfs_tuning(adm_wt_get_localmb(), fs);
  admwrk_ack(ctx, set_tuning);
}

/**
 * \brief Set the number of journals and the read-ahead for GFS.
 *
 * \param[in] fs                  Filesystem data chunk
 * \param[in] parameter           Option to change.
 * \param[in] value               Set to this value.
 *
 * \return 0 on success or an error code.
 */
static exa_error_code gfs_tune(admwrk_ctx_t *ctx, fs_data_t* fs,
			       const char* parameter, const char* value)
{
  if (!fs_handle_gfs)
    {
      return -FS_ERR_HANDLE_GFS;
    }

  if (!strcmp(parameter, EXA_FSTUNE_GFS_LOGS))
    {
      int number_of_logs;
      struct adm_volume *volume;
      int64_t sizeKB;
      int64_t additional_logs;
      exa_error_code ret_vrt_resize, ret_update, ret_get_started_nodes, ret_add_logs;
      exa_nodeset_t nodes_list, nodes_list_ro;
      gfs_add_logs_info_t logs_info;

      /* FIXME number_of_logs should be unsigned => simplify with to_uint() */
      if (to_int(value, &number_of_logs) != EXA_SUCCESS || number_of_logs < 0)
          return -EINVAL;

      if (number_of_logs < fs->clustered.gfs.nb_logs)
	{
	  return -FS_ERR_LESS_LOGS;
	}

      volume = fs_get_volume(fs);

      /* Check : one node has the FS mounted RW somewhere */
      ret_get_started_nodes = generic_fs_get_started_nodes(ctx, fs,
							   &nodes_list, &nodes_list_ro);
      if (ret_get_started_nodes != EXA_SUCCESS)
	return ret_get_started_nodes;

      exa_nodeset_substract(&nodes_list, &nodes_list_ro);

      if (exa_nodeset_count(&nodes_list) == 0)
	{
	  return -FS_ERR_INCREASE_LOGS_NEED_MOUNT_RW;
	}

      /* 1st step : need to extend the volume for the space needed
	            for this number of logs */
      additional_logs = number_of_logs - fs->clustered.gfs.nb_logs;
      sizeKB = fs->sizeKB + additional_logs * GFS_LOG_SIZE_MB * 1024;
      exalog_info("Need to expand for %"PRId64" logs,"
		  " which means new volume size is : %"PRId64" KB"
		  " (old size was : %"PRId64" KB)",
		  additional_logs, sizeKB, fs->sizeKB);
      ret_vrt_resize = vrt_master_volume_resize(ctx, volume, sizeKB);
      if (ret_vrt_resize)
	return ret_vrt_resize;

      /* 2nd step : call gfs_add */
      logs_info.fs = *fs;
      logs_info.number_of_logs = number_of_logs;
      exa_nodeset_copy(&logs_info.nodes_mounted, &nodes_list);
      ret_add_logs = admwrk_exec_command(ctx, &adm_service_fs, RPC_SERVICE_FS_GFS_ADD_LOGS,
				      &logs_info, sizeof(logs_info));

      if (ret_add_logs < 0)
	return ret_add_logs;

      /* update the FS info */
      fs->clustered.gfs.nb_logs = number_of_logs;
      fs->sizeKB = sizeKB;
      ret_update = fs_update_tree(ctx, fs);
      return ret_update;
    }
  else if (!strcmp(parameter, EXA_FSTUNE_READAHEAD))
    {
      int ret_get_size, ret_update, ret_set_readahead;
      ret_get_size = exa_get_size_kb(value, &fs->clustered.gfs.read_ahead);

      if (ret_get_size != 0)
	  return ret_get_size;

      ret_update = fs_update_tree(ctx, fs);
      if (ret_update != EXA_SUCCESS)
	{
	  return ret_update;
	}
      ret_set_readahead = admwrk_exec_command(ctx, &adm_service_fs,
					      RPC_SERVICE_FS_GFS_UPDATE_TUNING,
					      fs, sizeof(fs_data_t));
      return ret_set_readahead;
    }
  else if (!strcmp(parameter, EXA_FSTUNE_DEMOTE_SECS))
    {
      int ret_update, ret_set_demote_secs;

      errno = 0;
      fs->clustered.gfs.demote_secs = strtoul(value, NULL, 0);
      if (errno != 0)
        return -errno;
      if (fs->clustered.gfs.demote_secs > EXA_FSTUNE_DEMOTE_SECS_MAX)
        return -ERANGE;

      ret_update = fs_update_tree(ctx, fs);
      if (ret_update != EXA_SUCCESS)
        return ret_update;
      ret_set_demote_secs = admwrk_exec_command(ctx, &adm_service_fs,
                                                RPC_SERVICE_FS_GFS_UPDATE_TUNING,
                                                fs, sizeof(fs_data_t));
      return ret_set_demote_secs;
    }
    else if (!strcmp(parameter, EXA_FSTUNE_GLOCK_PURGE))
    {
      int ret_update, ret_set_glock_purge;

      errno = 0;
      fs->clustered.gfs.glock_purge = strtoul(value, NULL, 0);
      if (errno != 0)
        return -errno;
      if (fs->clustered.gfs.glock_purge > 100)
        return -ERANGE;

      ret_update = fs_update_tree(ctx, fs);
      if (ret_update != EXA_SUCCESS)
        return ret_update;
      ret_set_glock_purge = admwrk_exec_command(ctx, &adm_service_fs,
                                                RPC_SERVICE_FS_GFS_UPDATE_TUNING,
                                                fs, sizeof(fs_data_t));
      return ret_set_glock_purge;
    }
  else if (!strcmp(parameter, EXA_FSTUNE_GFS_FUZZY_STATFS))
    {
      int ret_update, ret_set_fuzzy_statfs;
      bool new_statfs;
      if (!strcmp(value, ADMIND_PROP_TRUE))
	new_statfs = true;
      else if (!strcmp(value, ADMIND_PROP_FALSE))
	new_statfs = false;
      else
	return -EINVAL;
      fs->clustered.gfs.fuzzy_statfs = new_statfs;
      ret_update = fs_update_tree(ctx, fs);
      if (ret_update != EXA_SUCCESS)
	{
	  return ret_update;
	}
      ret_set_fuzzy_statfs = admwrk_exec_command(ctx, &adm_service_fs,
						 RPC_SERVICE_FS_GFS_UPDATE_TUNING,
						 fs, sizeof(fs_data_t));
      return ret_set_fuzzy_statfs;
    }
  return generic_fs_tune(ctx, fs, parameter, value);
}

/**
 * \brief Return one tuning parameter of a SFS.
 *
 * param[in]  fs               represents the FS whose tuning must be caught.
 * param[out] tune_name_value  structure to fill with the name/value pair
 * param[out] error            error code.
 *
 * \return false if it was the last name or an error occurred, true instead
 *         and fill tune_name_value
 */
static bool gfs_gettune(admwrk_ctx_t *ctx, fs_data_t* fs,
				struct tune_t* tune, int* error)
{
  *error = EXA_SUCCESS;
  if (!strcmp(tune_get_name(tune), ""))
    {
      tune_set_name(tune, EXA_FSTUNE_GFS_LOGS);
      tune_set_description(tune, "Number of nodes that can concurrently mount the FS");
      tune_set_nth_value(tune, 0,
	       "%"PRIu64, fs->clustered.gfs.nb_logs);
      return true;
    }
  else if (!strcmp(tune_get_name(tune), EXA_FSTUNE_GFS_LOGS))
    {
      tune_set_name(tune, EXA_FSTUNE_READAHEAD);
      tune_set_nth_value(tune, 0,
	       "%"PRIu64"K", fs->clustered.gfs.read_ahead);
      tune_set_description(tune, "Read-ahead for this FS with explicit unit (1K, 1M...)");
      return true;
    }
  else if (!strcmp(tune_get_name(tune), EXA_FSTUNE_READAHEAD))
    {
      tune_set_name(tune, EXA_FSTUNE_GFS_RG_SIZE);
      tune_set_nth_value(tune, 0,
	       "%"PRIu64, fs->clustered.gfs.rg_size);
      tune_set_description(tune, "Resource groups size in MB (READ-ONLY VALUE)");
      return true;
    }
  else if (!strcmp(tune_get_name(tune), EXA_FSTUNE_GFS_RG_SIZE))
    {
      tune_set_name(tune, EXA_FSTUNE_DEMOTE_SECS);
      tune_set_nth_value(tune, 0,
	       "%u", fs->clustered.gfs.demote_secs);
      tune_set_description(tune, "Seconds before demoting locks");
      return true;
    }
  else if (!strcmp(tune_get_name(tune), EXA_FSTUNE_DEMOTE_SECS))
    {
      tune_set_name(tune, EXA_FSTUNE_GLOCK_PURGE);
      tune_set_nth_value(tune, 0,
	       "%u", fs->clustered.gfs.glock_purge);
      tune_set_description(tune, "Percentage of glock purge");
      return true;
    }
  else if (!strcmp(tune_get_name(tune), EXA_FSTUNE_GLOCK_PURGE))
    {
      tune_set_name(tune, EXA_FSTUNE_GFS_FUZZY_STATFS);
      tune_set_nth_value(tune, 0,
	       fs->clustered.gfs.fuzzy_statfs ? ADMIND_PROP_TRUE : ADMIND_PROP_FALSE);
      tune_set_description(tune, "Activate the fuzzy_statfs that speeds up 'df'");
      return true;
    }
  else if (!strcmp(tune_get_name(tune), EXA_FSTUNE_GFS_FUZZY_STATFS))
    {
      tune_set_name(tune, "");
    }
  return generic_fs_gettune(ctx, fs, tune, error);
}

/**
 * \brief Is it allowed to delete that node ?
 *
 * \param [in] node   : Node to delete.
 *
 * return EXA_SUCCESS if allowed, error code otherwise
 */
int gfs_check_nodedel(admwrk_ctx_t *ctx, struct adm_node *node)
{
  if (loaded && gfs_using_gulm())
    {
      return -FS_ERR_GFS_CANNOT_DELETE_MASTER;
    }
  if (!loaded)
    {
      exa_nodeset_t gulm_masters_param_set;
      adm_cluster_get_param_nodeset(&gulm_masters_param_set, EXA_OPTION_FS_GULM_MASTERS);
      /* Check it is not a master given by the user */
      if (exa_nodeset_contains(&gulm_masters_param_set, node->id))
	{
	  return -FS_ERR_GFS_MUST_CHANGE_MASTER;
	}
    }
  return EXA_SUCCESS;
};

/**
 * \brief manage_gfs_local : Can we still manage GFS filesystems ?
 */
void manage_gfs_local(admwrk_ctx_t *ctx, void *msg)
{
    uint64_t* info_handle_gfs = msg;
    int error_val = EXA_SUCCESS;
    fs_handle_gfs = *info_handle_gfs;
    admwrk_ack(ctx, error_val);
}

/*
 * \brief Answer that a GFS volume is SHARED (not private)
 */
static bool gfs_is_volume_private(void)
{
  return false;
}

/**
 * List of filesystem types that this API handles
 */
static const char* gfs_name_list[]={ FS_NAME_GFS, NULL};

/* === Client API ==================================================== */

/* --- gfs_definition --------------------------------------------- */
/**
 * Internal functions that this API uses
 */
const fs_definition_t gfs_definition=
{
  .type_name_list               = gfs_name_list,
  .check_before_start           = clustered_check_before_start,
  .check_before_stop            = generic_fs_check_before_stop,
  .parse_fscreate_parameters    = gfs_parse_fscreate_parameters,
  .pre_create_fs                = gfs_pre_create_fs,
  .create_fs                    = gfs_create_fs,
  .start_fs                     = gfs_start_fs,
  .post_prepare                 = gfs_post_prepare,
  .pre_unload                   = gfs_pre_unload,
  .node_stop                    = gfs_node_stop,
  .stop_fs                      = gfs_stop_fs,
  .node_add                     = gfs_node_add,
  .node_add_commit              = gfs_node_add_commit,
  .resize_fs                    = gfs_resize_fs,
  .check_fs                     = clustered_check_fs,
  .tune                         = gfs_tune,
  .gettune                      = gfs_gettune,
  .fill_data_from_config        = gfs_fill_data_from_config,
  .specific_fs_recovery         = clustered_specific_fs_recovery,
  .is_volume_private            = gfs_is_volume_private
};
