/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file
 *
 * The rdev service:
 * - Scans the physical disks at recovery up and find those that have an UUID
 *   that matches a disk in the config file. This allow to find disks even
 *   though their paths change from one boot to another.
 * - Asks periodically the NBD to check if a disk got an I/O error. If so set
 *   the disk as broken and prevent upper services to use it.
 * - Stores the list of broken disks so they can't be used again after a reboot.
 */

#include "admind/services/rdev/src/rdev_sb.h"

#include "admind/services/rdev/include/rdev.h"
#include "admind/services/rdev/include/broken_disk_table.h"
#include "admind/services/rdev/include/rdev_config.h"

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admindstate.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"

#include "admind/include/evmgr_pub_events.h"

#include "log/include/log.h"

#include "common/include/threadonize.h"
#include "common/include/exa_env.h"
#include "common/include/exa_names.h"

#include "rdev/include/exa_rdev.h"
#include "nbd/service/include/nbdservice_client.h"
#ifdef WITH_MONITORING
#include "monitoring/md_client/include/md_notify.h"
#endif

#include "os/include/os_assert.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_disk.h"
#include "os/include/os_file.h"
#include "os/include/os_time.h"
#include "os/include/os_string.h"

/* time interval between two checks of disk status (in seconds) */
#define DISK_CHECK_INTERVAL 1

static broken_disk_table_t *broken_disks = NULL;

/* Pointer to an aligned buffer used by the disk checking thread */
static void *rdev_check_buffer = NULL;

static os_thread_t rdev_check_id;
static bool rdev_check_id_started = false;

static ExamsgHandle mh = NULL; /* For disk check thread */

static bool quit = true;

static void disk_checking_thread(void *dummy)
{
  exalog_as(EXAMSG_RDEV_ID);

  while (!quit)
  {
    int rdev_need_check = false;
    struct adm_disk *disk;

    adm_node_lock_disk_removal();

    adm_node_for_each_disk(adm_myself(), disk)
    {
      if (disk->local->rdev_req != NULL)
      {
        int state, last_state;

	last_state = disk->local->state;

        state = exa_rdev_test(disk->local->rdev_req,
                              rdev_check_buffer, RDEV_SUPERBLOCK_SIZE);

	/* if exa_rdev_test returns an error, the disk is considered in failure
	 * as we have no mean to know what really happened. */
	if (state < 0)
	{
	    exalog_error("testing rdev '%s' " UUID_FMT " failed: %s (%d)",
			 disk->path, UUID_VAL(&disk->uuid),
			 exa_error_msg(state), state);
	    state = EXA_RDEV_STATUS_FAIL;
	}

	if (state != last_state)
	{
	    if (state == EXA_RDEV_STATUS_FAIL)
		rdev_need_check = true;
	    disk->local->state = state;
	}
      }
    }

    adm_node_unlock_disk_removal();

    if (quit)
	break;

    if (rdev_need_check)
    {
      instance_event_msg_t msg;
      int ret;

      msg.any.type = EXAMSG_EVMGR_INST_EVENT;
      msg.event.id = EXAMSG_RDEV_ID;
      msg.event.state = INSTANCE_CHECK_DOWN;
      msg.event.node_id = adm_myself()->id;

      exalog_info("... broadcasting action: rdev check down");

      ret = examsgSend(mh, EXAMSG_ADMIND_EVMGR_ID,
                       EXAMSG_ALLHOSTS, &msg, sizeof(msg));
      EXA_ASSERT(ret == sizeof(msg));
    }

    os_sleep(DISK_CHECK_INTERVAL);
  }
}


/**
 * Initialize the RDEV service:
 * - start exa_rdev kernel module,
 * - allocate and initialize aligned buffers to read/writes superblocks.
 */
static int
rdev_init(int thr_nb)
{
  char path[OS_PATH_MAX];
  int err = 0;

  /* Load the broken disks table */
  err = exa_env_make_path(path, sizeof(path), exa_env_cachedir(), "broken_disks");
  if (err != 0)
      return err;

  err = broken_disk_table_load(&broken_disks, path, true /* open_read_write */);
  if (err != 0)
  {
      exalog_error("Failed loading the broken disk table: %s (%d)",
                    exa_error_msg(err), err);
      return err;
  }

  /* Initialize the rdev module */
  err = exa_rdev_static_init(RDEV_STATIC_CREATE);
  if (err != 0)
  {
    exalog_error("Failed initializing rdev statics: %s (%d)", exa_error_msg(err), err);
    goto cleanup_broken;
  }

  mh = examsgInit(EXAMSG_RDEV_ID);
  if (!mh)
  {
      exalog_error("Failed initializing messaging for disk checking thread");
      err = -ENOMEM;
      goto cleanup_rdev;
  }

  COMPILE_TIME_ASSERT(RDEV_SUPERBLOCK_SIZE <= SECTORS_TO_BYTES(RDEV_RESERVED_AREA_IN_SECTORS));

  rdev_check_buffer = os_aligned_malloc(RDEV_SUPERBLOCK_SIZE, 4096, NULL);
  if (!rdev_check_buffer)
  {
      exalog_error("Failed allocating disk checking buffer");
      err = -ENOMEM;
      goto cleanup_mh;
  }

  /* make sure the check thread will not quit */
  quit = false;

  /* launch the rdev checking thread */
  if (!exathread_create_named(&rdev_check_id, MIN_THREAD_STACK_SIZE,
                              disk_checking_thread, mh, "rdev_check"))
  {
      exalog_error("Failed creating disk checking thread");
      err = -EXA_ERR_DEFAULT;
      goto cleanup_rdev_check_buffer;
  }

  rdev_check_id_started = true;

  return EXA_SUCCESS;

cleanup_rdev_check_buffer:
    os_aligned_free(rdev_check_buffer);
    rdev_check_buffer = NULL;
cleanup_mh:
    examsgExit(mh);
    mh = NULL;
cleanup_rdev:
    exa_rdev_static_clean(RDEV_STATIC_DELETE);
cleanup_broken:
    broken_disk_table_unload(&broken_disks);
    return err;
}


/**
 * Cleanup the rdev service:
 * - free the buffers allocated bu rdev_init(),
 * - stop the exa_rdev kernel module.
 */
static int
rdev_shutdown(int thr_nb)
{
  broken_disk_table_unload(&broken_disks);

  /* close the rdev checking thread */
  quit = true;

  if (rdev_check_id_started)
      os_thread_join(rdev_check_id);
  rdev_check_id_started = false;

  os_aligned_free(rdev_check_buffer);

  exa_rdev_static_clean(RDEV_STATIC_DELETE);

  examsgExit(mh);

  return EXA_SUCCESS;
}

int rdev_remove_broken_disks_file(void)
{
    int err;
    char path[OS_PATH_MAX];

    if (broken_disks != NULL)
        return -EBUSY;

    err = exa_env_make_path(path, sizeof(path), exa_env_cachedir(), "broken_disks");
    if (err != 0)
        return err;

    if(unlink(path) != 0)
        return -errno;

    return 0;
}

/**
 * Stop a disk.
 */
static void rdev_stop_disk(struct adm_disk *disk, struct adm_node *node)
{
  /* Take the lock to prevent the check thread to test the device while
   * we are stopping it here. see bug #3876
   * FIXME This lock does not really know what it is protecting and from
   * what it protects it... A real rethink of struct disk ownership is needed
   */
  adm_node_lock_disk_removal();

  if (node == adm_myself() && disk->local->reachable)
  {
    int err = serverd_device_unexport(adm_wt_get_localmb(), &disk->uuid);
    if (err != EXA_SUCCESS)
      exalog_error("Cannot unexport disk " UUID_FMT " : %s(%d)",
                   UUID_VAL(&disk->uuid), exa_error_msg(err), err);

    exa_rdev_handle_free(disk->local->rdev_req);

    disk->local->state = EXA_RDEV_STATUS_FAIL;
    disk->local->rdev_req = NULL;
    disk->local->reachable = false;
  }

  disk->path[0] = '\0';

  adm_node_unlock_disk_removal();
}



/**
 * Synchronize the broken field of each disk with the content of the table
 * of broken disks. Request an NBD recovery if the status of one or several
 * disks changed.
 */
static void
rdev_update_disks(void)
{
  struct adm_node *node;

  adm_cluster_for_each_node(node)
  {
    struct adm_disk *disk;

    adm_node_for_each_disk(node, disk)
    {
      /* FIXME there is no check that the uuid in broken_disks are actually part
       * of the cluster */
      bool broken = broken_disk_table_contains(broken_disks, &disk->uuid);
      bool missing = disk->local != NULL && !disk->local->reachable;

      if (broken == disk->broken)
        continue;

      exalog_info("%s:"UUID_FMT" (%s) is %s%s", node->name,
                  UUID_VAL(&disk->uuid), disk->path,
                  broken ? "broken" : "not broken",
                  missing ? ", missing" : "");

      disk->broken = broken;
      if (disk->broken)
      {
        rdev_stop_disk(disk, node);
        inst_set_resources_changed_down(&adm_service_nbd);
      }
      else
        inst_set_resources_changed_up(&adm_service_nbd);
    }
  }
}

/**
 * Synchronize the table of broken disks with the broken field of each disk.
 */
static void
rdev_update_broken_disks(void)
{
  struct adm_node *node;
  struct adm_disk *disk;
  int i = 0;

  broken_disk_table_clear(broken_disks);

  adm_cluster_for_each_node(node)
  {
    adm_node_for_each_disk(node, disk)
    {
      /* FIXME Should assert, now that the broken disk table is able to hold
               the max number of disks */
      if(i >= NBMAX_DISKS)
      {
	exalog_error("the limit of %d broken disks has been reached",
		     NBMAX_DISKS);
        break;
      }

      if (!disk->broken)
       continue;

      exalog_debug("adding disk " UUID_FMT " into the broken disks table",
                  UUID_VAL(&disk->uuid));
      broken_disk_table_set_disk(broken_disks, i++, &disk->uuid);
    }
  }

  broken_disk_table_increment_version(broken_disks);
  broken_disk_table_write(broken_disks);
}

static bool __size_big_enough(uint64_t size)
{
    return size >= RDEV_RESERVED_AREA_IN_SECTORS * SECTOR_SIZE;
}

/**
 * Export newly up disks.
 */
static int nbd_recover_serverd_device_export(struct adm_disk *disk)
{
    int ret;

    exalog_debug("serverd_device_export(%s, " UUID_FMT ")",
                 disk->path, UUID_VAL(&disk->uuid));
    ret = serverd_device_export(adm_wt_get_localmb(), disk->path, &disk->uuid);
    if (ret == -ADMIND_ERR_NODE_DOWN)
      return ret;
    if (ret == -CMD_EXP_ERR_OPEN_DEVICE)
    {
      exalog_warning("Export device '%s', " UUID_FMT ": %s(%d)",
                     disk->path, UUID_VAL(&disk->uuid),
		     exa_error_msg(ret), ret);
      return EXA_SUCCESS;
    }
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS,
                       "Export device '%s', " UUID_FMT " failed: %s(%d)",
                       disk->path, UUID_VAL(&disk->uuid),
                       exa_error_msg(ret), ret);

    return EXA_SUCCESS;
}

/**
 * Try to start a local disk.
 *
 * @param[in] path: the path of the disk
 */
static int rdev_start_disk(const char *path)
{
  rdev_superblock_t*sb;
  struct adm_disk *disk;
  exa_uuid_t uuid;
  uint64_t size; /* bytes */
  int ret;
  int fd;

  /* Open the disk */
  fd = os_disk_open_raw(path, OS_DISK_READ | OS_DISK_DIRECT | OS_DISK_EXCL);
  if (fd < 0)
  {
    ret = fd;
    /* Don't log a message for "normal" errors, eg. already mounted or no medium */
    if (ret == -EBUSY || ret == -ENOMEDIUM)
      exalog_debug("open(%s) failed: %s", path, exa_error_msg(ret));
    else
      exalog_error("open(%s): %s", path, exa_error_msg(ret));
    return ret;
  }

  /* Check the size */
  ret = os_disk_get_size(fd, &size);
  if (ret != 0)
  {
    close(fd);
    exalog_error("Failed getting size of %s: %s (%d)", path,
                 exa_error_msg(ret), ret);
    return ret;
  }

  if (!__size_big_enough(size))
  {
    close(fd);
    exalog_debug("%s too small (%" PRIu64 " KB) to store rdev superblock (%d KB)",
		 path, size / 1024,
		 RDEV_RESERVED_AREA_IN_SECTORS * SECTOR_SIZE / 1024);
    return -VRT_ERR_RDEV_TOO_SMALL;
  }

  sb = os_aligned_malloc(RDEV_SUPERBLOCK_SIZE, 4096, NULL);
  if (sb == NULL)
  {
      close(fd);
      exalog_error("Cannot allocate memory to read of the superblock"
                   " on %s.", path);
      return -ENOMEM;
  }

  /* Read the superblock. */
  ret = read(fd, sb, RDEV_SUPERBLOCK_SIZE);
  if (ret < 0)
  {
    ret = -errno;
    close(fd); /* Careful close can modify errno */
    os_aligned_free(sb);
    exalog_error("The read of the superblock on %s failed: %s",
		 path, exa_error_msg(ret));
    return ret;
  }

  /* Read was done (valid or not) we can close the disk. */
  close(fd);

  EXA_ASSERT(ret == RDEV_SUPERBLOCK_SIZE);

  /* Check whether the right magic is present. */
  if (strcmp(sb->magic, EXA_RDEV_SB_MAGIC) != 0)
  {
    os_aligned_free(sb);
    exalog_debug("disk %s is not an Exanodes disk", path);
    return -ADMIND_ERR_DISK_ALIEN;
  }

  /* Check whether the disk UUID is known. */
  uuid_copy(&uuid, &sb->uuid);

  os_aligned_free(sb);

  disk = adm_cluster_get_disk_by_uuid(&uuid);
  if (disk == NULL)
  {
    exalog_debug("the config does not contain a disk with uuid " UUID_FMT,
		 UUID_VAL(&uuid));
    return -ADMIND_ERR_UNKNOWN_DISK_UUID;
  }

  /* Check whether the disk is in the right node. */
  if (disk->node_id != adm_myself()->id)
  {
    struct adm_node *node = adm_cluster_get_node_by_id(disk->node_id);
    exalog_error("Disk " UUID_FMT " was moved from node %s",
		 UUID_VAL(&uuid), node->name);
    return -ADMIND_ERR_MOVED_DISK;
  }

  /* Check whether the disk is unique. */
  EXA_ASSERT_VERBOSE(!disk->local->reachable,
                     "The disk " UUID_FMT " has been found twice (%s and %s)",
                     UUID_VAL(&uuid), path, disk->path);

  /* Initialize exa_rdev for future non-blocking writes of the superblocks. */
  EXA_ASSERT(disk->local->rdev_req == NULL);

  disk->local->rdev_req = exa_rdev_handle_alloc(path);
  if (disk->local->rdev_req == NULL)
  {
    exalog_error("exa_rdev_request_init(%s) disk " UUID_FMT " failed",
                 path, UUID_VAL(&uuid));
    return -ENODEV;
  }

  /* Set up disk data structure. */
  strlcpy(disk->path, path, sizeof(disk->path));
  disk->local->state = EXA_RDEV_STATUS_OK;

  ret = nbd_recover_serverd_device_export(disk);
  if (ret != EXA_SUCCESS)
      return ret;

  disk->local->reachable = true;

  return EXA_SUCCESS;
}

/**
 * Try to start all disks that match the given pattern.
 */
static void
rdev_start_all_disks_matching_pattern(const char *pattern)
{
    os_disk_iterator_t *iter;
    const char *disk;

    exalog_debug("Trying to start disks matching pattern '%s'", pattern);

    iter = os_disk_iterator_begin(pattern);
    if (iter == NULL)
    {
        exalog_error("failed allocating disk iterator for pattern '%s'", pattern);
        return;
    }

    while ((disk = os_disk_iterator_get(iter)) != NULL)
    {
        exalog_debug("Trying to start disk '%s'", disk);
        if (rdev_start_disk(disk) == EXA_SUCCESS)
            exalog_info("%s is an Exanodes disk", disk);
    }

    os_disk_iterator_end(iter);
}


/**
 * Try to start all disks.
 */
static void
rdev_start_all_disks(void)
{
  char patterns[EXA_MAXSIZE_PARAM_VALUE + 1];
  char *ptr, *cur;

  /* os_strtok() modifies its argument so we copy it before */
  os_strlcpy(patterns, adm_cluster_get_param_text("disk_patterns"), sizeof(patterns));

  cur = os_strtok(patterns, " ", &ptr);
  while (cur != NULL)
  {
      rdev_start_all_disks_matching_pattern(cur);
      cur = os_strtok(NULL, " ", &ptr);
  }
}

typedef struct broken_table_info_t
{
    uint64_t table_version;
    exa_uuid_t broken_table[NBMAX_DISKS];
} broken_table_info;

static int rdev_synchronise_broken_disk_table(int thr_nb)
{
    admwrk_request_t rpc;
    exa_nodeid_t nodeid;
    int ret;

    broken_table_info info;
    broken_table_info reply;
    exa_nodeid_t best_node_id = EXA_NODEID_NONE;
    uint64_t best_version = 0;
    exa_uuid_t best_broken_table[NBMAX_DISKS];
    const exa_uuid_t *local_broken_table;

    info.table_version = broken_disk_table_get_version(broken_disks);

    local_broken_table = broken_disk_table_get(broken_disks);
    memcpy(info.broken_table, local_broken_table, sizeof(info.broken_table));

    admwrk_bcast(thr_nb, &rpc, EXAMSG_SERVICE_RDEV_BROKEN_DISKS_EXCHANGE,
                 &info, sizeof(info));

    while (admwrk_get_bcast(&rpc, &nodeid, &reply, sizeof(reply), &ret))
    {
        if (ret == -ADMIND_ERR_NODE_DOWN)
            continue;
        EXA_ASSERT(ret == EXA_SUCCESS);

        if (reply.table_version >= best_version
            && nodeid < best_node_id)
        {
            best_version = reply.table_version;
            best_node_id = nodeid;
            memcpy(best_broken_table, reply.broken_table,
                   sizeof(reply.broken_table));
        }
    }

    if (best_node_id != adm_my_id && best_version > info.table_version)
    {
        broken_disk_table_set_version(broken_disks, best_version);
        return broken_disk_table_set(broken_disks, best_broken_table);
    }

    return EXA_SUCCESS;
}

/**
 * Local command for recovery of service RDEV:
 * - start local disks if the local node is going up,
 * - synchronize path, size and broken status of each disks between the nodes,
 * - request a recovery RESOURCES of service NBD if some disks were just started.
 */
static void
rdev_recover_local(int thr_nb, void *msg)
{
  exa_nodeid_t nodeid;
  exa_nodeset_t nodes_going_up;
  exa_nodeset_t nodes_going_down;
  exa_nodeset_t nodes_down;
  struct {
    struct {
      char path[EXA_MAXSIZE_DEVPATH];
    } disk[NBMAX_DISKS_PER_NODE];
  } info;
  size_t info_size;
  struct adm_disk *disk;
  struct adm_node *node;
  admwrk_request_t rpc;
  int down_ret;
  int ret;
  int i = -1;

  inst_get_nodes_going_up(&adm_service_rdev, &nodes_going_up);
  inst_get_nodes_going_down(&adm_service_rdev, &nodes_going_down);
  inst_get_nodes_down(&adm_service_rdev, &nodes_down);

  /* Avoid the blocking operation of starting the disks if we are not doing
     a recovery up on this node */

  if (adm_nodeset_contains_me(&nodes_going_up))
  {
    exalog_debug("I'm upping. Search Exanodes devices.");
    rdev_start_all_disks();
  }

  /*
   * We compute the exact used size of the message. Without that, the barrier
   * mailbox could receive (8 + 8 + (8 + 128) x 16) x 128 = 274 KB which is
   * more than its size (80 KB). With that, as we have no more than 512 disks
   * in the whole cluster, the max total size of the received messages is:
   * (8 + 128) x 512 + (8 + 8) x 128 = 70 KB.
   */
  info_size = sizeof(info) - sizeof(info.disk[0]) * NBMAX_DISKS_PER_NODE;

  adm_node_for_each_disk(adm_myself(), disk)
  {
    i++; /* start at 0 */

    strlcpy(info.disk[i].path, disk->path, sizeof(info.disk[i].path));

    info_size += sizeof(info.disk[i]);
  }

  admwrk_bcast(thr_nb, &rpc, EXAMSG_SERVICE_RDEV_VERSION, &info, info_size);
  while (admwrk_get_bcast(&rpc, &nodeid, &info, sizeof(info), &down_ret))
  {
    if (down_ret == -ADMIND_ERR_NODE_DOWN)
      continue;

    i = -1;
    adm_node_for_each_disk(adm_cluster_get_node_by_id(nodeid), disk)
    {
      i++; /* start at 0 */
      strlcpy(disk->path, info.disk[i].path, sizeof(disk->path));
    }
  }

  inst_set_resources_changed_up(&adm_service_nbd);

  exa_nodeset_sum(&nodes_down, &nodes_going_down);
  exa_nodeset_substract(&nodes_down, &nodes_going_up);

  adm_cluster_for_each_node(node)
      if (exa_nodeset_contains(&nodes_down, node->id))
          adm_node_for_each_disk(adm_cluster_get_node_by_id(node->id), disk)
              disk->path[0] = '\0';

  ret = rdev_synchronise_broken_disk_table(thr_nb);

  /* FIXME is this barrier needed ? */
  admwrk_barrier(thr_nb, ret, "RDEV: Syncing broken disks table");

  rdev_update_disks();

  admwrk_barrier(thr_nb, ret, "RDEV: writing broken disks table");

  /* FIXME this return SUCCESS even if some error occured (during send or
   * store, who knows...) */
  admwrk_ack(thr_nb, EXA_SUCCESS);
}


/**
 * Cluster command for recovery of service RDEV. Just call the local command.
 */
static int
rdev_recover(int thr_nb)
{
  return admwrk_exec_command(thr_nb, &adm_service_rdev,
			     RPC_SERVICE_RDEV_RECOVER, NULL, 0);
}


static void
rdev_diskdel(int thr_nb, struct adm_node *node, struct adm_disk *disk)
{
  rdev_stop_disk(disk, node);
  if (broken_disk_table_contains(broken_disks, &disk->uuid))
      rdev_update_broken_disks();
}


/**
 * Handle service RDEV stuff when stopping some nodes:
 * - stop the disks of these nodes,
 * - cleanup the table of broken disks is we stop ourself.
 */
static int
rdev_nodestop(int thr_nb, const exa_nodeset_t *nodes_to_stop)
{
  struct adm_node *node;
  struct adm_disk *disk;

  adm_cluster_for_each_node(node)
  {
    if (!exa_nodeset_contains(nodes_to_stop, node->id))
      continue;

    adm_node_for_each_disk(node, disk)
      rdev_stop_disk(disk, node);
  }

  if (adm_nodeset_contains_me(nodes_to_stop))
  {
    /* XXX This isn't nice: the in-memory version becomes unsynchronized with
           the on-disk version. Instead, why not just close the broken table?
           (where should it be opened, then? rdev_init still fine?) */
    broken_disk_table_clear(broken_disks);
    broken_disk_table_set_version(broken_disks, 0);

    adm_cluster_for_each_node(node)
      adm_node_for_each_disk(node, disk)
        disk->broken = false;
  }

  return EXA_SUCCESS;
}


/**
 * Handle service RDEV stuff when stopping all nodes:
 * - stop the disks,
 * - cleanup the table of broken disks.
 */
static void
rdev_stop_local(int thr_nb, void *msg)
{
  exa_nodeset_t *nodes_to_stop = &((stop_data_t *)msg)->nodes_to_stop;
  int ret;

  ret = rdev_nodestop(thr_nb, nodes_to_stop);

  admwrk_ack(thr_nb, ret);
}


/**
 * Cluster command to stop the service RDEV on all nodes.
 */
static int
rdev_stop(int thr_nb, const stop_data_t *stop_data)
{
  return admwrk_exec_command(thr_nb, &adm_service_rdev,
			     RPC_SERVICE_RDEV_STOP,
			     stop_data, sizeof(*stop_data));
}


/**
 * Local command for the recovery CHECK of service RDEV:
 * - check broken status of each local disk from serverd,
 * - synchronize broken status of disks between nodes,
 * - update the table of broken disks and increment its version,
 * - write the new table of broken disks on all disks that are up,
 * - requires a recovery RESOURCE of service NBD.
 */
static void rdev_check_down_local(int thr_nb, void *msg)
{
  exa_nodeid_t nodeid;
  admwrk_request_t handle;
  struct {
    bool broken;
    bool new_up;
    char path[EXA_MAXSIZE_DEVPATH];
  } info[NBMAX_DISKS_PER_NODE];
  size_t info_size;
  struct adm_disk *disk;
  struct adm_node *node;
  int ret_down;
  int i = -1;

  memset(&info, 0, sizeof(info));

  /*
   * We compute the exact used size of the message. Without that, the barrier
   * mailbox could receive (4 + 4 + 8 + 128) x 16 x 128 = 288 KB which is
   * more than its size (80 KB). With that, as we have no more than 512 disks
   * in the whole cluster, the max total size of the received messages is:
   * (4 + 4 + 8 + 128) x 512 = 72 KB.
   */
  info_size = 0;

  /* Check broken status of each local disk from serverd. */

  adm_node_for_each_disk(adm_myself(), disk)
  {
    i++;
    EXA_ASSERT(i < NBMAX_DISKS_PER_NODE);

    if (disk->local->reachable)
    {
      if (disk->local->state == EXA_RDEV_STATUS_OK)
	{
	  exalog_debug("disk " UUID_FMT " is not broken", UUID_VAL(&disk->uuid));
	  info[i].broken = false;
	}
      else
	{
	  exalog_debug("disk " UUID_FMT " is broken", UUID_VAL(&disk->uuid));
	  info[i].broken = true;
	}
      info[i].new_up = !disk->local->reachable; /* If disk is not up, it is a new disk */
    }
    else
    {
      exalog_debug("disk " UUID_FMT " is not reachable", UUID_VAL(&disk->uuid));
      info[i].broken = disk->broken;
      info[i].new_up = false; /* disk is not reachable, cannot recover it anyway */
    }

    strlcpy(info[i].path, disk->path, sizeof(info[i].path));

    info_size += sizeof(info[i]);
  }

  /* Synchronize broken status of disks between nodes. */

  admwrk_bcast(thr_nb, &handle, EXAMSG_SERVICE_RDEV_DEAD_INFO,
	       &info, info_size);

  while (admwrk_get_bcast(&handle, &nodeid, &info, sizeof(info), &ret_down))
  {
    /* The check should not be interrupted when a node crash. It should just
       continue without this node and return SUCCESS. */
    if (ret_down != EXA_SUCCESS)
      continue;

    i = -1;

    node = adm_cluster_get_node_by_id(nodeid);
    EXA_ASSERT(node != NULL);
    adm_node_for_each_disk(node, disk)
    {
      i++;
      if (disk->broken == info[i].broken)
      {
	  /* When the disk was missing and is coming back, its broken state
             is not changed */
	  if (info[i].new_up)
	      inst_set_resources_changed_up(&adm_service_rdev);
	  continue;
      }

      exalog_info("%s:"UUID_FMT" (%s) was %s and is now %s",
		  node->name, UUID_VAL(&disk->uuid), disk->path,
		  disk->broken ? "broken" : "not broken",
		  info[i].broken ? "broken" : "not broken");

      disk->broken = info[i].broken;

      if (disk->broken)
      {
#ifdef WITH_MONITORING
	  /* send a trap to monitoring daemon */
	  md_client_notify_disk_broken(adm_wt_get_localmb(),
				       &disk->vrt_uuid,
				       node->name,
				       disk->path);
#endif
	  rdev_stop_disk(disk, node);
          strlcpy(disk->path, info[i].path, sizeof(disk->path));

          inst_set_resources_changed_down(&adm_service_nbd);
      }
      else
      {
#ifdef WITH_MONITORING
	  /* send a trap to monitoring daemon */
	  md_client_notify_disk_up(adm_wt_get_localmb(),
				   &disk->vrt_uuid,
				   node->name,
				   disk->path);
#endif

        strlcpy(disk->path, info[i].path, sizeof(disk->path));

	inst_set_resources_changed_up(&adm_service_nbd);
      }
    }
  }

  /* Update the table of broken disks and increment its version. */
  rdev_update_broken_disks();

  admwrk_ack(thr_nb, EXA_SUCCESS);
}


/**
 * Cluster command for recovery CHECK of service RDEV.
 * Just call the local command.
 */
static int rdev_check_down(int thr_nb)
{
  return admwrk_exec_command(thr_nb, &adm_service_rdev,
			     RPC_SERVICE_RDEV_CHECK_DOWN, NULL, 0);
}


/**
 * Local command to add a new disk in an existing cluster:
 * - start the disk,
 * - request a recovery RESOURCE.
 */
static int
rdev_diskadd(int thr_nb, struct adm_node *node, struct adm_disk *disk,
	     const char *path)
{
  int ret;

  if (node == adm_myself())
    ret = rdev_start_disk(path);
  else
    ret = EXA_SUCCESS;

  /* FIXME there is no condition on the resource changed ? */
  inst_set_resources_changed_up(&adm_service_rdev);

  return ret;
}


/**
 * Initialize the superblock on a new disk.
 *
 * @param[in] path       Path of the disk
 * @param[in] disk_uuid  UUID to "tattoo" the disk with
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int rdev_initialize_sb(const char *path, const exa_uuid_t *disk_uuid)
{
  rdev_superblock_t *sb;
  uint64_t size;
  int ret;
  int fd;
  cl_error_desc_t err_desc;

  exalog_info("Initializing superblock of %s with uuid " UUID_FMT,
              path, UUID_VAL(disk_uuid));

  if (!rdev_is_path_available(path, &err_desc))
    return -RDEV_ERR_DISK_NOT_AVAILABLE;

  /* FIXME WIN32 */
  /* Check the disk is not an exanodes volume. */
  if (strncmp(path, "/dev/exa/", sizeof("/dev/exa/") - 1) == 0)
  {
    exalog_error("Cannot use the Exanodes volume '%s' as an Exanodes disk", path);
    return -ADMIND_ERR_VOLUME_AS_DISK;
  }

  /* Check there is no filesystem on the disk */
  if (os_disk_has_fs(path))
  {
      exalog_error("Can not use disk %s with already a filesystem on it", path);
      return -RDEV_ERR_INVALID_DEVICE;
  }

  fd = os_disk_open_raw(path, OS_DISK_RDWR | OS_DISK_DIRECT | OS_DISK_EXCL);

  if (fd < 0)
  {
    ret = fd;
    exalog_error("Failed opening %s: %s (%d)", path, exa_error_msg(ret), ret);
    return -RDEV_ERR_CANT_OPEN_DEVICE;
  }

  /* Check whether the disk is big enough to contain the superblock. */
  ret = os_disk_get_size(fd, &size);
  if (ret != 0)
  {
      close(fd);
      exalog_error("Failed getting size of %s: %s (%d)", path,
                   exa_error_msg(ret), ret);
      return ret;
  }

  if (!__size_big_enough(size))
  {
    close(fd);
    exalog_error("%s too small (%" PRIu64 " KB) to store rdev superblock (%d KB)",
		 path, size / 1024,
		 RDEV_RESERVED_AREA_IN_SECTORS * SECTOR_SIZE / 1024);
    return -VRT_ERR_RDEV_TOO_SMALL;
  }

  /* Write the superblocks. */
  sb = os_aligned_malloc(RDEV_SUPERBLOCK_SIZE, 4096, NULL);
  if (sb == NULL)
  {
     exalog_error("Failed to allocate superblock buffer");
     return -ENOMEM;
  }
  memcpy(sb->magic, EXA_RDEV_SB_MAGIC, sizeof(sb->magic));
  uuid_copy(&sb->uuid, disk_uuid);

  ret = write(fd, sb, RDEV_SUPERBLOCK_SIZE);

  os_aligned_free(sb);

  if (ret != RDEV_SUPERBLOCK_SIZE || close(fd) < 0)
  {
    ret = -errno;
    close(fd);
    exalog_error("Failed writing superblock to %s: %s (%d)", path,
                 exa_error_msg(ret), ret);
    return ret;
  }


  return EXA_SUCCESS;
}

/** Data that describes the RDEV service. */
const struct adm_service adm_service_rdev =
{
  .id = ADM_SERVICE_RDEV,
  .init = rdev_init,
  .shutdown = rdev_shutdown,
  .recover = rdev_recover,
  .stop = rdev_stop,
  .check_down = rdev_check_down,
  .check_up = NULL,
  .diskadd = rdev_diskadd,
  .diskdel = rdev_diskdel,
  .local_commands =
  {
    { RPC_SERVICE_RDEV_RECOVER,     rdev_recover_local      },
    { RPC_SERVICE_RDEV_STOP,        rdev_stop_local         },
    { RPC_SERVICE_RDEV_CHECK_DOWN,  rdev_check_down_local   },
    { RPC_COMMAND_NULL, NULL }
  }
};
