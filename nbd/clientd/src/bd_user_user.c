/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "nbd/clientd/src/bd_user_user.h"

#include "nbd/clientd/src/nbd_stats.h"
#include "nbd/clientd/src/nbd_clientd_perf_private.h"
#include "nbd/clientd/src/nbd_clientd_private.h"
#include "nbd/clientd/src/nbd_blockdevice.h"

#include "nbd/common/nbd_common.h"

#include "common/include/exa_constants.h"

#include "log/include/log.h"

#include "os/include/os_string.h"
#include "os/include/os_stdio.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#include <errno.h>

struct __ndev
{
    blockdevice_t *blockdevice;
    bool up;
    bool suspended;
    char name[EXA_BDEV_NAME_MAXLEN + 1];
    exa_uuid_t uuid;
    exa_nodeid_t holder_id; /** id of the node having access to the physical device */

    /* When disk is imported, the server provides an id to identify the disk
     * more quickly than with a uuid. If -1, the id is unknown.
     * FIXME having a typedef would have this cleaner. */
    int32_t server_side_disk_uid;

    struct device_stats stats;
    struct ndev_perf perfs;

    bool free; /** Tells is entry is free or not */
};

/* FIXME This is NOT a queue, this just is an ELEMENT (of a queue). */
struct bd_kerneluser_queue
{
    blockdevice_io_t *bio;
    ndev_t *ndev;
    int req_num;
};

/* User structure pointer get by kernel mmap This structure must be read only */
static int bd_buffer_size;
static bool bd_barrier_enable;

static struct nbd_root_list request_root_list;
static struct nbd_list request_list;
static ndev_t device[NBMAX_DISKS];

/* used to don't have a device that be suspended/down/removed/resumed during
 * a exa_bdget_new_request() or a other function it
 * FIXME what is the data this lock is supposed to protect ? */
static os_thread_rwlock_t change_state;

const char *ndev_get_name(const ndev_t *ndev)
{
    return ndev->name;
}

static ndev_t *__get_ndev_from_uuid(const exa_uuid_t *uuid)
{
  int i;

  for (i = 0; i < NBMAX_DISKS; i++)
      if (!device[i].free && uuid_is_equal(uuid, &device[i].uuid))
	return &device[i];

  return NULL;
}

bool exa_bdinit(int buffer_size, int max_queue, bool barrier_enable)
{
    int i;

    bd_buffer_size = buffer_size;
    bd_barrier_enable = barrier_enable;

    if (nbd_init_root(max_queue, sizeof(struct bd_kerneluser_queue),
                      &request_root_list) < 0)
        return false;

    for (i = 0; i < NBMAX_DISKS; i++)
    {
        device[i].blockdevice = NULL;
        device[i].up = false;
        device[i].suspended = false;
        device[i].free = true;
        device[i].name[0] = '\0';
    }

    os_thread_rwlock_init(&change_state);

    nbd_init_list(&request_root_list, &request_list);

    return true;
}

void exa_bdend(void)
{
    nbd_close_root(&request_root_list);
}

void exa_bd_end_request(header_t *header)
{
    struct bd_kerneluser_queue *bdq = nbd_get_elt_by_num(header->req_num, &request_root_list);
    blockdevice_io_t *bio = bdq->bio;

    EXA_ASSERT(bdq != NULL);

    EXA_ASSERT(bdq->req_num == header->req_num);

    nbd_stat_request_done(&bdq->ndev->stats, header);
    clientd_perf_end_request(&bdq->ndev->perfs, header);

    nbd_list_post(&request_root_list.free, bdq, -1);

    blockdevice_end_io(bio, header->result);
}

void *exa_bdget_buffer(int num) /* reentrant */
{
    void *buffer;
    struct bd_kerneluser_queue *bdq;

    os_thread_rwlock_rdlock(&change_state);

    bdq = nbd_get_elt_by_num(num % 1000 , &request_root_list);
    EXA_ASSERT(bdq != NULL);
    EXA_ASSERT(bdq->req_num == num);

    /* FIXME : buffer addresse was no always valid we can have more than
     * 1 scatter/gather buffer (legacy comment -> parse error) */
    buffer = bdq->bio->buf;

    os_thread_rwlock_unlock(&change_state);

    return buffer;
}

blockdevice_t *exa_bdget_block_device(const exa_uuid_t *uuid)
{
    ndev_t *ndev = __get_ndev_from_uuid(uuid);

    return ndev == NULL ? NULL : ndev->blockdevice;
}

/* FIXME These aren't states but *operations* */
typedef enum {
    BDMINOR_SUSPEND = 1,
    BDMINOR_UP,
    BDMINOR_DOWN,
    BDMINOR_RESUME,
} bdminor_state_t;

#define BDMINOR_STATE_IS_VALID(s) \
    ((s) == BDMINOR_SUSPEND || (s) == BDMINOR_UP || (s) == BDMINOR_DOWN \
     || (s) == BDMINOR_RESUME)

/* Change status of a device
 * There two status an external Status and a internal NextStatus
 * BDMINOR_SUSPEND : no request from this minor will be ack by kernel, ever if it's send explicitelu ExaBDEndRequest()
 *		     Status get BDMINOR_SUSPEND
 *                   NextStatus get the old status (BDMINOR_UP or BDMINOR_DOWN
 * BDMINOR_UP      : only if Status is BDMINOR_SUSPEND otherwise do nothing
 *		     NextStatus get BDMINOR_UP
 * BDMINOR_DOWN      : only if Status is BDMINOR_SUSPEND otherwise do nothing
 *		     NextStatus get BDMINOR_DOWN
 * BDMINOR_RESUME   : only if Status is BDMINOR_SUSPEND otherwise do nothing
 *                   Status get NextStatus so
 *			if Status is now BDMINOR_UP, all request from this minor will be received from kernel and ack normally
 *                      if Status is now BDMINOR_DOWN, all pending request forom this minor (Kernel and User) will be end end with IO Error, for
 *							for all user pending request, no need to ack it.
 *							all ExaBDEndRequest for these request will fail
 * NOTICE: This function is re-entrant.
 */
static int exa_bdset_status(const exa_uuid_t *uuid, bdminor_state_t state)
{
    struct bd_kerneluser_queue *bdq;
    ndev_t *ndev;

    EXA_ASSERT_VERBOSE(BDMINOR_STATE_IS_VALID(state), "Invalid state %d for"
                       " device UUID " UUID_FMT, state, UUID_VAL(uuid));

    os_thread_rwlock_wrlock(&change_state);

    ndev = __get_ndev_from_uuid(uuid);

    if (ndev == NULL)
    {
        os_thread_rwlock_unlock(&change_state);
        exalog_error("Cannot set status for unknown device with UUID "
                     UUID_FMT, UUID_VAL(uuid));
        return -CMD_EXP_ERR_UNKNOWN_DEVICE;
    }

    switch (state)
    {
    case BDMINOR_SUSPEND:
	ndev->suspended = true;
	break;
    case BDMINOR_UP:
	if (ndev->suspended)
            ndev->up = true;
        nbd_stat_restart(&ndev->stats);
	break;
    case BDMINOR_DOWN:
	if (ndev->suspended)
            ndev->up = false;
	break;
    case BDMINOR_RESUME:
	if (!ndev->suspended)
            break;

        ndev->suspended = false;

        /* FIXME when a header_t is sent to serverd, the associated bdq
         * remains valid, but there is no more structure pointing to bdq.
         * Thus, when an serverd answers, the bdq is found thanks to
         * nbd_get_elt_by_num() (req_num is the identifier of the bdq).
         * But when a node crashes, all outstanding IOs are left pending and
         * clientd does not have the req_num list of bdq waiting for an
         * answer, so there are bdq left inaccessible. To allow to return IO
         * errors to caller, clientd relies on the loop below to find elements
         * of the nbd list that are NOT free and belonging to the given minor.
         * You may notice that IOs that where already answered in the above
         * loop have been freed, thus the loop below will not find them (so
         * there is only one end_io() call per bdq)
         */
        while (nbd_get_next_by_tag((void **)&bdq, (long long int)ndev, &request_root_list) == 0)
        {
            blockdevice_end_io(bdq->bio, -EIO);
            nbd_list_post(&request_root_list.free, bdq, -1);
        }
	break;
    }

    os_thread_rwlock_unlock(&change_state);

    return EXA_SUCCESS;
}

int client_suspend_device(const exa_uuid_t *uuid)
{
    return exa_bdset_status(uuid, BDMINOR_SUSPEND);
}

int client_resume_device(const exa_uuid_t *uuid)
{
    return exa_bdset_status(uuid, BDMINOR_RESUME);
}

int client_down_device(const exa_uuid_t *uuid)
{
    return exa_bdset_status(uuid, BDMINOR_DOWN);
}

void bd_get_stats(struct nbd_stats_reply *reply, const exa_uuid_t *uuid, bool reset)
{
    ndev_t *ndev = __get_ndev_from_uuid(uuid);

    if (ndev == NULL)
        return;

    nbd_get_stats(&ndev->stats, reply, reset);
}

static int prepare_req_header(struct bd_kerneluser_queue *bdq, int req_index,
                              struct header *req_header)
{
    req_header->req_num = req_index;
    bdq->req_num = req_header->req_num;

    EXA_ASSERT(BLOCKDEVICE_IO_TYPE_IS_VALID(bdq->bio->type));
    switch (bdq->bio->type)
    {
        case BLOCKDEVICE_IO_WRITE:
            req_header->request_type = NBD_REQ_TYPE_WRITE;
            break;
        case BLOCKDEVICE_IO_READ:
            req_header->request_type = NBD_REQ_TYPE_READ;
            break;
    }

    req_header->type = NBD_HEADER_RH;
    req_header->sector = bdq->bio->start_sector;
    req_header->sector_nb = BYTES_TO_SECTORS(bdq->bio->size);
    req_header->bypass_lock = bdq->bio->bypass_lock;
    req_header->flush_cache = bdq->bio->flush_cache;
    req_header->buf = NULL /* Dummy field not used in clientd */;

    /* get network device */
    req_header->disk_id = bdq->ndev->server_side_disk_uid;

    req_header->client_id = bdq->ndev->holder_id;

    /* FIXME
     * All this tagging and numbering stuff is brain dead: when send thread
     * finished sending the header_t of the IO, is should acknowledge clientd
     * which would put bdq elements into a kind of pending_bdq ndb_list so
     * that when answers from serverd eventually arrive, clientd could just
     * simply fetch the corresponding bdq in the list and not bother with
     * nbd_list ad hoc API (nbd_get_elt_by_num or nbd_get_next_by_tag) */
    nbd_set_tag(bdq, (long long int)bdq->ndev, &request_root_list);

    return 0;
}
static void exa_bdmake_request(ndev_t *ndev, blockdevice_io_t *bio)
{
    int req_index;
    header_t *req_header;
    struct bd_kerneluser_queue *bdq;

    /* FIXME: Does (bio->size == 0) still means the request is a barrier ?
     * If it does, we must use an explicit definition.
     * If it does not, we must remove that stuff or replace it by an assert
     */
    if (!bd_barrier_enable && bio->size == 0)
    {
        blockdevice_end_io(bio, 0);
        return;
    }

    os_thread_rwlock_rdlock(&change_state);

    while (ndev->suspended)
    {
        os_thread_rwlock_unlock(&change_state);
        /* Device is suspended, some status change is occuring, no need to add
         * the request yet. Sleep for now and see later what is the new
         * status of the device. */
        /* You may notice that there is a race: IOs that passed just before the
         * device was suspended were sent to serverd. This should not be a
         * problem as there are 2 solutions: either the IO can be done, either
         * the disk/node is dead and the IO will eventually end up with an IO
         * error (when going throught pending request of dead minors). */
        os_millisleep(200);
        os_thread_rwlock_rdlock(&change_state);
    }

    if (!ndev->up)
    {
        os_thread_rwlock_unlock(&change_state);
        blockdevice_end_io(bio, -EIO);
        return;
    }

    bdq = nbd_list_remove(&request_root_list.free, &req_index, LISTWAIT);
    EXA_ASSERT(bdq != NULL);

    bdq->bio = bio;
    bdq->ndev = ndev;

    /* FIXME: On the NBD side the barriers should always be enabled, it is the
     * responsability of the VRT and the FS to tells if they want to flush the
     * disk cache or not. */
    /* discard the 'flush_cache' bool if barrier is not enabled */
    if (!bd_barrier_enable)
        bio->flush_cache = false;

    req_header = nbd_list_remove(&nbd_client.recv_list.root->free, NULL, LISTWAIT);
    EXA_ASSERT(req_header != NULL);

    prepare_req_header(bdq, req_index, req_header);

    /*FIXME I still don't know what this is supposed to lock here... */
    os_thread_rwlock_unlock(&change_state);

    nbd_stat_request_begin(&ndev->stats, req_header);

    clientd_perf_make_request(req_header);

    header_sending(req_header);
}

/*
 * Create a new ndev device.
 * The ndev is created suspended and down */
int client_add_device(const exa_uuid_t *uuid, exa_nodeid_t node_id)
{
    ndev_t *ndev;
    int err, idx;

    ndev = __get_ndev_from_uuid(uuid);
    if (ndev != NULL)
        return EXA_SUCCESS;

    /* Find a new room for device. */
    for (idx = 0; idx < NBMAX_DISKS; idx++)
        if (device[idx].free)
            break;

    if (idx >= NBMAX_DISKS)
        return -NBD_ERR_NB_RDEVS_CREATED;

    ndev = &device[idx];

    os_thread_rwlock_wrlock(&change_state);

    err = nbd_blockdevice_open(&ndev->blockdevice,
                               BLOCKDEVICE_ACCESS_RW,
                               BYTES_TO_SECTORS(bd_buffer_size),
                               exa_bdmake_request, ndev);
    if (err == 0)
    {
        ndev->free = false;

        ndev->up = false;
        ndev->suspended = true;
        os_snprintf(ndev->name, sizeof(ndev->name), UUID_FMT, UUID_VAL(uuid));
        uuid_copy(&ndev->uuid, uuid);
        ndev->holder_id = node_id;

        nbd_stat_init(&ndev->stats);
        clientd_perf_dev_init(&ndev->perfs, uuid);
    }
    else
    {
        exalog_error("Cannot add device " UUID_FMT ": %s(%d)",
                     UUID_VAL(uuid), exa_error_msg(err), err);
        err = -EXA_ERR_CANT_GET_MINOR;
    }

    os_thread_rwlock_unlock(&change_state);

    return err;
}

int client_remove_device(const exa_uuid_t *uuid)
{
    blockdevice_t *bdev;
    ndev_t *ndev = __get_ndev_from_uuid(uuid);

    if (ndev == NULL)
    {
        exalog_error("Cannot remove unknown device UUID "
                     UUID_FMT, UUID_VAL(uuid));
        return -CMD_EXP_ERR_UNKNOWN_DEVICE;
    }

    exa_bdset_status(uuid, BDMINOR_SUSPEND);
    exa_bdset_status(uuid, BDMINOR_DOWN);
    exa_bdset_status(uuid, BDMINOR_RESUME);

    bdev = ndev->blockdevice;
    ndev->blockdevice = NULL;
    ndev->name[0] = '\0' ;

    nbd_stat_clean(&ndev->stats);

    ndev->free = true;

    EXA_ASSERT(blockdevice_close(bdev) == 0);

    return 0;
}

int exa_bdminor_bind_dev(const exa_uuid_t *uuid, uint64_t size_in_sector,
                         int device_nb)
{
    ndev_t *ndev = __get_ndev_from_uuid(uuid);
    int err = 0;

    if (ndev == NULL)
        return -1;

    ndev->server_side_disk_uid = device_nb;

    /* FIXME  Spurious locking: what is actually being changed in ndev, not
     * session itself. Using this lock here would mean that it should be used
     * in make_request... which is not done */
    os_thread_rwlock_wrlock(&change_state);

    /* set the correct size of the block device */
    if (ndev->blockdevice != NULL)
        err = blockdevice_set_sector_count(ndev->blockdevice, size_in_sector);

    os_thread_rwlock_unlock(&change_state);

    if (err != 0)
    {
        exalog_error("Cannot set the size %" PRIu64 "of device " UUID_FMT,
                     size_in_sector, UUID_VAL(uuid));
        return -NBD_ERR_SET_SIZE;
    }

    /* set the device to UP in the module */
    return exa_bdset_status(uuid, BDMINOR_UP);
}

