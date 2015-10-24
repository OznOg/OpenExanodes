/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "nbd/serverd/ndevs.h"
#include "nbd/serverd/nbd_disk_thread.h"
#include "nbd/serverd/nbd_serverd.h"

#include "nbd/common/nbd_tcp.h"

#include "nbd/service/include/nbd_msg.h"
#include "nbd/service/include/nbdservice_client.h"

#include "rdev/include/exa_rdev.h"

#include "log/include/log.h"

#include "examsg/include/examsg.h"

#include "common/include/daemon_api_server.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/threadonize.h"

#include "os/include/os_disk.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_thread.h"
#include "os/include/strlcpy.h"

static device_t *find_device_from_uuid(const exa_uuid_t *uuid)
{
    int i;

    for (i = 0; i < NBMAX_DISKS_PER_NODE; i++)
    {
        if (nbd_server.devices[i]
            && uuid_is_equal(uuid, &nbd_server.devices[i]->uuid))
            return nbd_server.devices[i];
    }

    return NULL;
}

void rebuild_helper_thread(void *p)
{
  ExamsgHandle mh;
  int err;

  exalog_as(EXAMSG_NBD_SERVER_ID);

  /* initialize examsg framework */
  mh = examsgInit(EXAMSG_NBD_LOCKING_ID);
  EXA_ASSERT(mh != NULL);

  err = examsgAddMbox(mh, EXAMSG_NBD_LOCKING_ID, 1, 5 * EXAMSG_MSG_MAX);
  EXA_ASSERT(err == 0);

  os_sem_post(&nbd_server.mailbox_sem);

  while (nbd_server.run)
  {
      device_t *device;
      ExamsgNbdLock nbd_lock_msg;
      ExamsgMID from;
      struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
      exa_nodeset_t dest_nodes;

      err = examsgWaitTimeout(mh, &timeout);
      /* Just in order to check stopping the thread is required*/
      if (err == -ETIME)
	  continue;

      if (err != 0)
      {
          exalog_error("Locking thread encountered error %s (%d) while "
                       "waiting in event loop.", exa_error_msg(err), err);
          continue;
      }

      err = examsgRecv(mh, &from, &nbd_lock_msg, sizeof(nbd_lock_msg));

      /* No message */
      if (err == 0)
	continue;

      if (err < 0)
      {
          exalog_error("Locking thread encountered error %s (%d) while "
                       "receiving a messsage.", exa_error_msg(err), err);
	  continue;
      }

      switch(nbd_lock_msg.any.type)
      {
      case EXAMSG_NBD_LOCK:
	  /* find device from name */
          /* FIXME devices lock is not held... it should */
          device = find_device_from_uuid(&nbd_lock_msg.disk_uuid);
	  if (device == NULL)
          {
              exalog_error("Unknown device with UUID " UUID_FMT, UUID_VAL(&nbd_lock_msg.disk_uuid));
              err = -CMD_EXP_ERR_UNKNOWN_DEVICE;
              break;
          }
          if (nbd_lock_msg.lock)
          {
              err = exa_disk_lock_zone(device, nbd_lock_msg.locked_zone_start,
                                          nbd_lock_msg.locked_zone_size);
              EXA_ASSERT_VERBOSE(err == 0, "Trying to lock too many zone "
                                 "(>%d). Last zone not succesfully locked "
                                 "(start = %" PRId64 ", size = %" PRId64 " ) "
                                 "on device UUID " UUID_FMT, NBMAX_DISK_LOCKED_ZONES,
                                 nbd_lock_msg.locked_zone_start,
                                 nbd_lock_msg.locked_zone_size,
                                 UUID_VAL(&nbd_lock_msg.disk_uuid));
          }
          else
          {
              err = exa_disk_unlock_zone(device, nbd_lock_msg.locked_zone_start,
                                            nbd_lock_msg.locked_zone_size);
              EXA_ASSERT_VERBOSE(err == 0, "Trying to unlock a never locked "
                                 "zone (unlocked zone start =%" PRId64 ", "
                                 "unlocked zone size = %" PRId64 ") on device"
                                 " UUID " UUID_FMT,
                                 nbd_lock_msg.locked_zone_start,
                                 nbd_lock_msg.locked_zone_size,
                                 UUID_VAL(&nbd_lock_msg.disk_uuid));
          }
	  break;

	default:
	  /* error */
	  EXA_ASSERT_VERBOSE(false, "Locking thread got unknown message of"
                             " type %d ", nbd_lock_msg.any.type);
	  break;
	}

      exa_nodeset_single(&dest_nodes, from.netid.node);
      examsgAckReply(mh, (Examsg *)&nbd_lock_msg, err, from.id, &dest_nodes);
    }

  examsgDelMbox(mh, EXAMSG_NBD_LOCKING_ID);
  examsgExit(mh);
}

/** get the number of sector of the device
 * \param device_path the device to get the number of sector
 * \param nb_sectors64 the number of sectors of the device
 * \return nb_sectors the returned number of sector
 */

static int get_nb_sectors(const char *device_path, uint64_t *nb_sectors)
{
  uint64_t device_size; /* in bytes */
  int retval;
  int fd;

  /* We need the read access to get the size. */
  if ((fd = os_disk_open_raw(device_path, OS_DISK_READ)) < 0)
  {
    exalog_error("cannot open device '%s'  error=%s ",
                 device_path, exa_error_msg(-fd));
    return -CMD_EXP_ERR_OPEN_DEVICE;
  }

  retval = os_disk_get_size(fd, &device_size);
  if (retval < 0)
  {
    exalog_error("os_disk_get_size() error=%s", exa_error_msg(retval));
    if (close(fd) != 0)
      exalog_error("can't EVEN close dev '%s'", device_path);
    return -EXA_ERR_IOCTL;
  }

  retval = close(fd);
  if (retval < 0)
  {
    retval = -errno;
    exalog_error("cannot close device '%s' error=%s ",
                 device_path, exa_error_msg(retval));
    return -CMD_EXP_ERR_CLOSE_DEVICE;
  }

  *nb_sectors = device_size / SECTOR_SIZE;

  /* remove the size of the reserved area for storing admind info */
  *nb_sectors -= RDEV_RESERVED_AREA_IN_SECTORS;

  /* Align the size on 1K
   * this is the best we can do to have the same size of devices on 2.4 and 2.6 kernels due to
   * the fact that kernel 2.4 rounds the size of devices with 1 K
   */
  *nb_sectors -= *nb_sectors % (1024 / SECTOR_SIZE);

  return EXA_SUCCESS;
}

/* A new device is handled by the server, do the init operations in order to make the device usable */
int export_device(const exa_uuid_t *uuid, char *device_path)
{
    device_t *dev;
    int i, err;

    /* If device was already exported, do nothing */
    if (find_device_from_uuid(uuid) != NULL)
        return EXA_SUCCESS;

    dev = os_malloc(sizeof(struct device));
    if (dev == NULL)
    {
        err = -NBD_ERR_MALLOC_FAILED;
        goto error;
    }

    dev->handle = NULL;
    err = -CMD_EXP_ERR_OPEN_DEVICE;

    dev->handle = exa_rdev_handle_alloc(device_path);
    if (dev->handle == NULL)
        goto error;

    err = get_nb_sectors(device_path, &dev->size_in_sectors);
    if (err != EXA_SUCCESS)
        goto error;

    uuid_copy(&dev->uuid, uuid);
    strlcpy(dev->path, device_path, sizeof(dev->path));

    for (i = 0; i < NBMAX_DISKS_PER_NODE; i++)
        if (nbd_server.devices[i] == NULL)
            break;

    if (i >= NBMAX_DISKS_PER_NODE)
    {
        exalog_error("maximum number of exportable devices exceeded");
        err = -NBD_ERR_NB_RDEVS_CREATED;
    }

    dev->dev_index = i;
    dev->exit_thread = false;

    nbd_init_list(&nbd_server.list_root, &dev->disk_queue);

    /* resource needed to lock/unlock a zone */
    os_sem_init (&dev->lock_sem_disk, 0);

    /* launch disk thread (TD) */
    if (!exathread_create_named(&nbd_server.td_pid[dev->dev_index],
                                NBD_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE,
                                exa_td_main, dev, "TD_thread"))
    {
        os_sem_destroy(&dev->lock_sem_disk);
        err = -NBD_ERR_THREAD_CREATION;
        goto error;
    }

    nbd_server.devices[dev->dev_index] = dev;

    return EXA_SUCCESS;

error:
    if (dev != NULL)
    {
        if (dev->handle != NULL)
            exa_rdev_handle_free(dev->handle);
        os_free(dev);
    }
    return err;
}

/* --- Ndevs manipulation functions -------------------------------- */
int server_add_client(char *node_name, char *net_id, exa_nodeid_t node_id)
{
  int err;

  if (strncmp(node_name, nbd_server.node_name, EXA_MAXSIZE_HOSTNAME) == 0)
      nbd_server.server_id = node_id;

  err = tcp_add_peer(node_id, net_id, nbd_server.tcp);

  if (err != EXA_SUCCESS)
      exalog_error("Tcp server returned an error when adding client %s",
		   node_name);

  return err;
}

int server_remove_client(exa_nodeid_t node_id)
{
  /* FIXME check return value */
  tcp_remove_peer(node_id, nbd_server.tcp);

  return EXA_SUCCESS;
}

int unexport_device(const exa_uuid_t *uuid)
{
  device_t *dev = find_device_from_uuid(uuid);
  if (dev == NULL)
    {
      exalog_error("can not remove unknown device with UUID = " UUID_FMT, UUID_VAL(uuid));
      return -CMD_EXP_ERR_UNKNOWN_DEVICE;
    }

  os_thread_mutex_lock(&nbd_server.mutex_edevs);
  /* ask the thread to terminate */
  dev->exit_thread = true;

  /* prevent any new IO to be put in device IO list */
  nbd_server.devices[dev->dev_index] = NULL;
  os_thread_mutex_unlock(&nbd_server.mutex_edevs);

  /* now we can join, because with the nbd_close_list()
   * we can assume was the disk thread will reach a cancelation point */
  os_thread_join(nbd_server.td_pid[dev->dev_index]);

  /* close the list used to disk queue */
  nbd_close_list(&dev->disk_queue);

  /* get back all header in the kernel exa_rdev to the free list and close the device */
  if (dev->handle != NULL)
      exa_rdev_handle_free(dev->handle);

  /* close the semaphore used by the disk */
  os_sem_destroy(&dev->lock_sem_disk);

  /* free used memory for the device */
  os_free(dev);

  return EXA_SUCCESS;
}

void nbd_ndev_getinfo(const exa_uuid_t *uuid, ExamsgID from)
{
    const device_t *dev;
    exported_device_info_t device_info;

    /* Do not send garbage from the stack when we return an error */
    /* FIXME this seems to be a side effect expected by upper layers...
     * They SHOULD NOT rely on the content of answer if it returns an error. */
    memset(&device_info, 0, sizeof(device_info));

    os_thread_mutex_lock(&nbd_server.mutex_edevs);

    dev = find_device_from_uuid(uuid);
    if (dev == NULL)
        device_info.status = -CMD_EXP_ERR_UNKNOWN_DEVICE;
    else
    {
        device_info.device_nb      = dev->dev_index;
        device_info.device_sectors = dev->size_in_sectors;
        device_info.status         = EXA_SUCCESS;
    }

    os_thread_mutex_unlock(&nbd_server.mutex_edevs);

    admwrk_daemon_reply(nbd_server.mh, from,
                        &device_info,  sizeof(device_info));
}
