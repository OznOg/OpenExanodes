/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <string.h>
#include <errno.h>

#include "common/include/exa_thread_name.h"
#include "common/include/exa_error.h"
#include "log/include/log.h"
#include "nbd/common/nbd_common.h"
#include "nbd/serverd/nbd_disk_thread.h"
#include "nbd/serverd/nbd_serverd.h"
#include "os/include/os_stdio.h"
#include "rdev/include/exa_rdev.h"

/*
 * Short description of locking process
 * - Sending lock
 * - server receive lock and wait for all pending request already sent to exa_rdev
 * - server signal that the lock is now ok
 * - server continue to read new header and check for a unlocking
 * - if server read a header in a locked area and wasnot already unlocked
 * 		- it repost the header
 *              - wait for all pending request already sent to exa_rdev
 *              - wait for a unlocking
 */

int __exa_disk_zone_lock(device_t *dev, long first_sector, long size_in_sector,
                         bool lock)
{
  header_t *header = nbd_list_remove(&dev->disk_queue.root->free, NULL, LISTWAIT);
  EXA_ASSERT(header != NULL);

  header->type = NBD_HEADER_LOCK;
  header->lock.sector = first_sector;
  header->lock.sector_nb = size_in_sector;

  header->lock.op = lock ? NBD_REQ_TYPE_LOCK : NBD_REQ_TYPE_UNLOCK;

  nbd_list_post(&dev->disk_queue, header, -1);

  os_sem_wait(&dev->lock_sem_disk);

  return dev->locking_return;
}

/**
 * unlock the last lock pending on the disk
 * @param dev the disk to lock
 * @param first_sector first sector of the zone to lock
 * @param size_in_sector number of sector to lock
 * @return EXA_SUCCESS or -1
 */
int exa_disk_unlock_zone(device_t *dev, long first_sector, long size_in_sector)
{
    return __exa_disk_zone_lock(dev, first_sector, size_in_sector, false);
}

/**
 * Lock a zone on the device, when the function will return, the disk will be locked
 * @param dev the disk to lock
 * @param first_sector first sector of the zone to lock
 * @param size_in_sector number of sector to lock
 * @return EXA_SUCCESS or -1
 */
int exa_disk_lock_zone(device_t *dev, long first_sector, long size_in_sector)
{
    return __exa_disk_zone_lock(dev, first_sector, size_in_sector, true);
}

/**
 * merge the lock/unlock header to the list of the locked zone of the device
 * set disk_device->locking_return according to the succes (0) or error (-1)
 * of the merge
 * @param disk_device device
 * @param header lock/unlocking to merge
 */
static void td_merge_lock(device_t *disk_device, header_t *header)
{
    int i;

    EXA_ASSERT(header->type = NBD_HEADER_LOCK);
    EXA_ASSERT(header->lock.op == NBD_REQ_TYPE_LOCK
               || header->lock.op == NBD_REQ_TYPE_UNLOCK);

    switch (header->lock.op)
    {
    case NBD_REQ_TYPE_LOCK:
        if (disk_device->nb_locked_zone > NBMAX_DISK_LOCKED_ZONES)
        {
            disk_device->locking_return = -1;
            return;
        }
        else
        {
            struct locked_zone *locked_zone = &disk_device->locked_zone[disk_device->nb_locked_zone];

            locked_zone->sector    = header->lock.sector;
            locked_zone->sector_count = header->lock.sector_nb;

            disk_device->nb_locked_zone++;
            disk_device->locking_return = 0;
        }

        return;

    case NBD_REQ_TYPE_UNLOCK:
        for (i = 0; i < disk_device->nb_locked_zone; i++)
        {
            struct locked_zone *locked_zone = &disk_device->locked_zone[i];

            if (locked_zone->sector == header->lock.sector
                && locked_zone->sector_count == header->lock.sector_nb)
            {
                disk_device->locking_return = 0;
                disk_device->nb_locked_zone--;

                /* The array is not sorted but need to be consolidated, thus
                 * when removing an element which is not at the end, we fill
                 * the 'hole' by moving the last element to this place. */
                if (i < disk_device->nb_locked_zone) /* last zone */
                    disk_device->locked_zone[i] = disk_device->locked_zone[disk_device->nb_locked_zone];

                return;
            }
        }

        disk_device->locking_return = -1;
        return;
    }
}

/**
 * check if a header is a read or write in a locked zone
 * @param disk_device the device
 * @param header we want to know if it accesses to locked zone
 * @return false not locked
 *         true  locked
 */
static bool td_is_locked(device_t *disk_device, header_t *header)
{
    int i;

    for (i = 0; i < disk_device->nb_locked_zone; i++)
    {
        const struct locked_zone *locked_zone = &disk_device->locked_zone[i];
        if (header->io.sector < locked_zone->sector + locked_zone->sector_count
            && header->io.sector + header->io.sector_nb > locked_zone->sector)
            return true;
    }

    return false;
}

/**
 * send one request to device, it validate there is no problem with the
 *
 * @header IN request to do
 *         OUT last request done
 * @return EXA_RDEV_REQUEST_END_OK       new request submitted successfully and header
 *                                       contains an old request succesfully done
 *         EXA_RDEV_REQUEST_END_ERROR    new request submitted successfully
 *                                       and header contains an old request that fail
 *         RDEV_REQUEST_NOT_ENOUGH_FREE_REQ not enough resources to submit a new request
 */
static int exa_td_process_one_request(header_t **header,
                                      device_t *disk_device)
{
  void * buffer;
  int sector_nb;
  uint64_t sector;
  int retval;
  header_t *req_header = *header;

  /* FIXME this is a ugly hack to prevent compiler to complain about
   * uninitialized variable. Actually, this is because the request type
   * itself is f***ed up (no type and the funky use os bit masks...)
   * Please remove this whe reworking header_t content... */
  rdev_op_t op = (rdev_op_t)-1;

  /* submit this new request to exa_rdev and so to the disk driver */
  sector_nb = req_header->io.sector_nb;

  buffer = req_header->io.buf;

  sector = req_header->io.sector;

  EXA_ASSERT(NBD_REQ_TYPE_IS_VALID(req_header->io.request_type));
  switch (req_header->io.request_type)
  {
  case NBD_REQ_TYPE_READ:
      EXA_ASSERT(!req_header->io.flush_cache);
      op = RDEV_OP_READ;
      break;
  case NBD_REQ_TYPE_WRITE:
      if (req_header->io.flush_cache)
          op = RDEV_OP_WRITE_BARRIER;
      else
          op = RDEV_OP_WRITE;
      break;
  }

  /* Be carefull the 'header' pointer can be modified */
  retval = exa_rdev_make_request_new(op, (void *)header,
                                     sector + RDEV_RESERVED_AREA_IN_SECTORS,
                                     sector_nb, buffer, disk_device->handle);

  if (retval == RDEV_REQUEST_NOT_ENOUGH_FREE_REQ)
    return RDEV_REQUEST_NOT_ENOUGH_FREE_REQ;

  if (*header != NULL)
      (*header)->io.result = retval == RDEV_REQUEST_END_OK ? 0 : -EIO;

  if (retval < 0)
    return RDEV_REQUEST_END_ERROR;

  return retval;
}

/**
 * The request is finished, ackwoledge upper layer.
 * This function reuse the request for the answer, thus req is modified.
 *
 * @param disk_device    the device
 * @param header of the ended request
 */
static void handle_completed_io(device_t *disk_device, header_t *req)
{
    EXA_ASSERT(NBD_REQ_TYPE_IS_VALID(req->io.request_type));

    if (req->io.result != 0)
        exalog_trace("error %d: #%"PRIu64". %s (%d) %d sector at sector %"PRId64,
                     req->io.result, req->io.req_num,
                     (req->io.request_type == NBD_REQ_TYPE_READ) ? "READ" : "WRITE",
                     req->io.request_type, req->io.sector_nb, req->io.sector);

    nbd_server_end_io(req);
}

/**
 * Pick up a new request (sent by clientd).
 * This function is NOT blocking.
 *
 * @param disk_device  disk on which we pick a request.
 * return a request to process or NULL if the is no incoming request.
 */
static header_t *pick_one_req(device_t *disk_device)
{
   return nbd_list_remove(&disk_device->disk_queue, NULL, LISTNOWAIT);
}

/** An error code means finished if it is OK or ERROR.
 * TODO this has nothing to do here. As a matter of fact, this thread
 * does not care if the IO was successful or not, it just cares if it is
 * finished or not. This means that a single error code would suffice. */
#define means_finished(err) ((err) == RDEV_REQUEST_END_OK \
                             || (err) == RDEV_REQUEST_END_ERROR)

/**
 * Wait for the completion of a request and acknowledges upper layer it was
 * done. This function is blocking in case there are actually pending IO. If
 * not, it immediatly returns RDEV_REQUEST_ALL_ENDED.
 *
 * @param disk_device  disk on which the request is.
 *
 * return RDEV_REQUEST_END_ERROR, RDEV_REQUEST_END_OK or RDEV_REQUEST_ALL_ENDED
 */
static int wait_and_complete_one_io(device_t *disk_device)
{
    header_t *req;
    int err = exa_rdev_wait_one_request((void *)&req, disk_device->handle);

    EXA_ASSERT(err == RDEV_REQUEST_ALL_ENDED || err == RDEV_REQUEST_END_ERROR
               || err == RDEV_REQUEST_END_OK);

    if (!means_finished(err))
        return err;

    req->io.result = err == RDEV_REQUEST_END_OK ? 0 : -EIO;

    handle_completed_io(disk_device, req);

    return err;
}

static void __wait_for_all_completion(device_t *disk_device)
{
    while (wait_and_complete_one_io(disk_device) != RDEV_REQUEST_ALL_ENDED)
        ;
}

/**
 * Waits for a new incoming request.
 *
 * @param disk_device  disk on which we wait a request.
 * @param ms_timeout   timeout in milliseconds
 *
 * return a new request if one arrived before the timeout exipred.
 *        NULL it timeout expired.
 */
static header_t *wait_new_req(device_t *disk_device, int ms_timeout)
{
    struct nbd_list *lists_in[1];
    bool has_elt[1];
    header_t *req = NULL;

    lists_in[0] = &disk_device->disk_queue;
    if (nbd_list_select(lists_in, has_elt, 1, ms_timeout) != 0)
        req = nbd_list_remove(&disk_device->disk_queue, NULL, LISTNOWAIT);

    return req;
}

static int submit_req(device_t *disk_device, header_t **_req)
{
  header_t *req = *_req;

  if (req->type == NBD_HEADER_LOCK)
  {
      td_merge_lock(disk_device, req);

      __wait_for_all_completion(disk_device);

      nbd_list_post(&nbd_server.list_root.free, req, -1);
      *_req = NULL;
      os_sem_post(&disk_device->lock_sem_disk);
      return RDEV_REQUEST_NONE_ENDED;
  }

  if (req->io.sector_nb == 0) /* Is a flush */
  {
      EXA_ASSERT(req->io.request_type == NBD_REQ_TYPE_WRITE);
      __wait_for_all_completion(disk_device);
      /* Once the flush is called, we wait for all pending IO to
       * finish. Upon return we have the guaranty that every
       * pending operation on disk is finished and drive is
       * flushed. */
      exa_rdev_flush(disk_device->handle);
      req->io.result = 0;
      return RDEV_REQUEST_END_OK;
  }

  if (!req->io.bypass_lock && td_is_locked(disk_device, req))
  {
      /* Being here means that the IO cannot be done because of locks, thus
       * we send back a EAGAIN to caller to tell that the IO must be
       * re-submitted later. */
      req->io.result = -EAGAIN;
      return RDEV_REQUEST_END_OK;
  }

  return exa_td_process_one_request(_req, disk_device);
}

/**
 * Main thread to process disk, each disk have an instance of this thread
 * @param p the (device_t *) that describe this disk
 * @return
 */
void exa_td_main(void *p)
{
  bool pending_io = false;
  device_t *disk_device;
  char myname[32];

  exalog_as(EXAMSG_NBD_SERVER_ID);

  disk_device = (device_t *)p;

  os_snprintf(myname, 31, "serv%s", disk_device->path);
  exa_thread_name_set(myname);

  memset(&disk_device->locked_zone, 0xEE, sizeof(disk_device->locked_zone));
  disk_device->nb_locked_zone = 0;

#define run (!disk_device->exit_thread)
  while (run)
  {
      int err;

      header_t *req = pick_one_req(disk_device);

      if (req == NULL && pending_io)
      {
          /* NOTE: This code make the thread to wait for the completion
           * of at least one IO for an indefinite time (it calls
           * 'exa_rdev_wait_one_request'). If during this time a new
           * request occurs it will not be sent to the disk and I think it
           * can cause some performance problems.
           *
           * The point here is that we must poll the block device to be
           * noticed of the IO completion. One can convince himself of the
           * problem by looking at 'exa_rdev_make_request_new'
           * prototype. This function sends a new request to a device but
           * it also gets the reply for some other request.
           */
          err = wait_and_complete_one_io(disk_device);
          if (err == RDEV_REQUEST_ALL_ENDED)
              pending_io = false;
          continue;
      }
      else if (req == NULL && !pending_io)
          do {
              req = wait_new_req(disk_device, 200 /* timeout in ms */);
          } while (req == NULL && run);

      /* If req != NULL, the request must be handled otherwise it would be
       * leaked. So even if run is false, we submit the IO and then exit the
       * loop, the wait_for_all_completion will eventually make sure that any
       * pending IOs was answerd. */
      if (req == NULL)
          break;

      /* being here means a req needs to be handled */
      EXA_ASSERT(req != NULL);

      do {
          err = submit_req(disk_device, &req);
          if (err == RDEV_REQUEST_NOT_ENOUGH_FREE_REQ)
          {
              /* There was no room in kernel for this IO, thus
               * we try to complete pending IOs to make some free
               * space in kernel. */
              int err2 = wait_and_complete_one_io(disk_device);
              /* There MUST be pending IOs thus kernel cannot return
               * RDEV_REQUEST_ALL_ENDED */
              EXA_ASSERT(err2 != RDEV_REQUEST_ALL_ENDED);
          }
          else
              pending_io = true;
      } while (err == RDEV_REQUEST_NOT_ENOUGH_FREE_REQ);

      EXA_ASSERT(means_finished(err) || err == RDEV_REQUEST_NONE_ENDED);

      if (means_finished(err))
          handle_completed_io(disk_device, req);
  }

  /* Before leaving, make sure that all requests were successfully answerd
   * by kernel. */
  __wait_for_all_completion(disk_device);
}
