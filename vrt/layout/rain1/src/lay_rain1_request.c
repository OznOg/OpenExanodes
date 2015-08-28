/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */



#include <string.h>

#include "common/include/exa_error.h"
#include "common/include/exa_assert.h"

#include "os/include/os_atomic.h"
#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_request.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_nodes.h"
#include "vrt/virtualiseur/include/vrt_volume.h"

#include "vrt/layout/rain1/src/lay_rain1_metadata.h"
#include "vrt/layout/rain1/src/lay_rain1_status.h"
#include "vrt/layout/rain1/src/lay_rain1_striping.h"
#include "vrt/layout/rain1/src/lay_rain1_request.h"

#include "os/include/os_error.h"

/** State of a rain1 request */
typedef enum
{
#define RAIN1_REQUEST_STATE__FIRST RAIN1_REQUEST_BEGIN
    RAIN1_REQUEST_BEGIN,
    RAIN1_REQUEST_READ,

    RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL,
    RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH,
    RAIN1_REQUEST_START_METADATA_WRITE,
    RAIN1_REQUEST_DO_METADATA_WRITE,
    RAIN1_REQUEST_CONTINUE_METADATA_WRITE,

    RAIN1_REQUEST_START_USER_DATA_WRITE,

    RAIN1_REQUEST_DO_USER_BARRIER_WRITE,

    RAIN1_REQUEST_DO_USER_DATA_WRITE,
    RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE,

    RAIN1_REQUEST_SUCCESS,
    RAIN1_REQUEST_IOERROR_TRIGGERED,
    RAIN1_REQUEST_FAILED
#define RAIN1_REQUEST_STATE__LAST RAIN1_REQUEST_FAILED
} rain1_request_state_t;
#define RAIN1_REQUEST_STATE_IS_VALID(state) \
    ((state) >= RAIN1_REQUEST_STATE__FIRST && (state) <= RAIN1_REQUEST_STATE__LAST)

/**
 * Read states path:
 *
 * RAIN1_REQUEST_BEGIN
 * |-> RAIN1_REQUEST_FAILED
 * `-> RAIN1_REQUEST_READ
 *     `-> RAIN1_REQUEST_SUCCESS
 *
 *
 * Write states path:
 *
 * RAIN1_REQUEST_BEGIN
 * |-> RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL
 * |   |--------------------------------------------------------------------.
 * |   `-> RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH                              |
 * |       `----------------------------------------------------------------|
 * |-> RAIN1_REQUEST_START_METADATA_WRITE <---------------------------------|
 * |   `-> RAIN1_REQUEST_DO_METADATA_WRITE                                  |
 * |       `-> RAIN1_REQUEST_CONTINUE_METADATA_WRITE <.                     |
 * |           `-.------------------------------------'                     |
 * `-------------`-> RAIN1_REQUEST_START_USER_DATA_WRITE <------------------|
 *                   |-> RAIN1_REQUEST_DO_USER_BARRIER_WRITE                |
 *                   `---`-> RAIN1_REQUEST_DO_USER_DATA_WRITE               |
 *                           `-> RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE <.  |
 *                               `-.-------------------------------------'  |
 *                                 `-> RAIN1_REQUEST_SUCCESS                |
 *                                                                          |
 * RAIN1_REQUEST_IOERROR_TRIGGERED <----------------------------------------'
 * `-> RAIN1_REQUEST_FAILED
 *
 */

static vrt_req_status_t
rain1_req_set_state(struct vrt_request *vrt_req, rain1_request_state_t state)
{
    rain1_request_state_t *dest_state;

    COMPILE_TIME_ASSERT(sizeof(state) <= VRT_PRIVATE_DATA_SIZE);

    dest_state = (rain1_request_state_t *) vrt_req->private_data;
    *dest_state = state;

    switch (state)
    {
    case RAIN1_REQUEST_BEGIN:
    case RAIN1_REQUEST_READ:
    case RAIN1_REQUEST_START_METADATA_WRITE:
    case RAIN1_REQUEST_DO_METADATA_WRITE:
    case RAIN1_REQUEST_CONTINUE_METADATA_WRITE:
    case RAIN1_REQUEST_START_USER_DATA_WRITE:
    case RAIN1_REQUEST_DO_USER_BARRIER_WRITE:
    case RAIN1_REQUEST_DO_USER_DATA_WRITE:
    case RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE:
    case RAIN1_REQUEST_IOERROR_TRIGGERED:
        return VRT_REQ_UNCOMPLETED;
    case RAIN1_REQUEST_SUCCESS:
        return VRT_REQ_SUCCESS;
    case RAIN1_REQUEST_FAILED:
        return VRT_REQ_FAILED;
    case RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL:
    case RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH:
        return VRT_REQ_POSTPONED;
    }

    EXA_ASSERT(false);
    return VRT_REQ_FAILED;
}

static rain1_request_state_t
rain1_req_get_state(const struct vrt_request *vrt_req)
{
    rain1_request_state_t state = *(rain1_request_state_t *)(vrt_req->private_data);
    EXA_ASSERT_VERBOSE(RAIN1_REQUEST_STATE_IS_VALID(state),
                       "Invalid state %d for request %p", state, vrt_req);
    return state;
}

/**
 * Fill the list of IOs given by the VRT engine for read requests
 *
 * Called from the RAIN1_REQUEST_BEGIN state
 *
 * @param[in] vrt_req      The request header from the VRT engine
 */
static rain1_request_state_t
rain1_fill_io_read(struct vrt_request *vrt_req)
{
    struct rdev_location rdev_loc[3];
    unsigned int nb_rdev_loc;
    struct vrt_io_op *io;
    unsigned int src;

    EXA_ASSERT (vrt_req->iotype == VRT_IO_TYPE_READ);

    /* Initialize barrier to BARRIER_DONT_PROCESS
     * FIXME why ? */
    if (vrt_req->barrier != NULL)
	vrt_req->barrier->state = BARRIER_DONT_PROCESS;

    /* Convert the logical position on the volume into several physical positions */
    rain1_volume2rdev(RAIN1_GROUP(vrt_req->ref_vol->group),
                      vrt_req->ref_vol->assembly_volume,
                      vrt_req->ref_bio->start_sector,
                      rdev_loc, &nb_rdev_loc, 3);

    /* Initialize all vrt_io_op to IO_DONT_PROCESS */
    for (io = vrt_req->io_list ; io != NULL ; io = io->next)
	io->state = IO_DONT_PROCESS;

    io = vrt_req->io_list;

    /* Find a replica to read */
    for (src = 0; src < nb_rdev_loc; src++)
	if (rain1_rdev_location_readable(& rdev_loc[src]))
	    break;

    /* No readable replica have been found */
    if (src == nb_rdev_loc)
	return RAIN1_REQUEST_FAILED;

    io->iotype  = VRT_IO_TYPE_READ;
    io->data    = vrt_req->ref_bio->buf;
    io->size    = vrt_req->ref_bio->size;
    io->vrt_req = vrt_req;
    io->rdev    = rdev_loc[src].rdev;
    io->offset  = rdev_loc[src].sector;
    io->state   = IO_TO_PROCESS;

    return RAIN1_REQUEST_READ;
}

/**
 * Fill a volume barrier, that must be issued on all writable disks
 * that are part of the group *before* issuing the writes or metadata
 * writes flagged as BIO_RW_BARRIER. It is mandatory to issue this
 * barrier on *all* disks because it is the only way to ensure that
 * all previous write requests are stored on non-volatile storage.
 *
 * Called from the following states:
 * - RAIN1_REQUEST_START_METADATA_WRITE
 * - RAIN1_REQUEST_DO_USER_BARRIER_WRITE
 *
 * @param[out] vrt_req  The request header from the VRT engine
 */
static void rain1_fill_barrier(struct vrt_request *vrt_req)
{
    struct vrt_io_op *io;

    EXA_ASSERT(vrt_req->iotype == VRT_IO_TYPE_WRITE
               || vrt_req->iotype == VRT_IO_TYPE_WRITE_BARRIER);

    for (io = vrt_req->io_list; io != NULL; io = io->next)
	io->state = IO_DONT_PROCESS;

    vrt_req->barrier->state = BARRIER_TO_PROCESS;
}

static void rain1_init_io_replicas(struct vrt_request *vrt_req,
                                   struct rdev_location rdev_loc[3],
                                   unsigned int nb_rdev_loc, void *data,
                                   uint32_t size, vrt_io_type_t io_type,
                                   bool process_all_replicas)
{
    struct vrt_io_op *io;
    unsigned int dst;
    unsigned int io_to_process = 0;

    /* Initialize barrier to BARRIER_DONT_PROCESS
     * FIXME why ? */
    vrt_req->barrier->state = BARRIER_DONT_PROCESS;

    EXA_ASSERT(nb_rdev_loc >= 1);

    for (dst = 0, io = vrt_req->io_list; io != NULL; io = io->next, dst++)
    {
        io->state = IO_DONT_PROCESS;

	if (dst < nb_rdev_loc)
	{
	    io->iotype  = io_type;
	    io->data    = data;
	    io->size    = size;
	    io->vrt_req = vrt_req;
	    io->rdev    = rdev_loc[dst].rdev;
	    io->offset  = rdev_loc[dst].sector;
	    if (rdev_loc[dst].uptodate || process_all_replicas)
	    {
		io->state   = IO_TO_PROCESS;
		io_to_process++;
	    }
	}
	else
	    /* set rdev to NULL: it will be used in rain1_fill_io_metadata_second */
	    io->rdev = NULL;
    }

    /* Not all the replicas have been tested */
    EXA_ASSERT_VERBOSE(dst >= nb_rdev_loc,
		       "Not enough IO in the writing request: i=%u nb_rdev_loc=%u\n",
		       dst, nb_rdev_loc);
    EXA_ASSERT(io_to_process > 0);
}

/**
 * Fill the list of I/Os given by the VRT engine for write requests when
 * we are in rebuilding. This will perform the I/Os sequentially: first
 * on up-to-date devices and then on non-up-to-date devices.
 *
 * This function perform the 1st part.
 *
 * Called from the following state:
 * - RAIN1_REQUEST_DO_USER_DATA_WRITE
 *
 * @param[in] vrt_req      The request header from the VRT engine
 */
static void rain1_fill_io_write(struct vrt_request *vrt_req)
{
    struct rdev_location rdev_loc[3];
    unsigned int nb_rdev_loc;

    EXA_ASSERT(vrt_req->iotype == VRT_IO_TYPE_WRITE
            || vrt_req->iotype == VRT_IO_TYPE_WRITE_BARRIER);

    /* Convert the logical position on the volume into several physical positions. */
    rain1_volume2rdev(RAIN1_GROUP(vrt_req->ref_vol->group),
                      vrt_req->ref_vol->assembly_volume,
                      vrt_req->ref_bio->start_sector,
                      rdev_loc, &nb_rdev_loc, 3);

    rain1_init_io_replicas(vrt_req, rdev_loc, nb_rdev_loc,
                           vrt_req->ref_bio->buf, vrt_req->ref_bio->size,
                           vrt_req->iotype, false /* don't process outdated
                                                     replica */);
}


/**
 * Fill the list of I/Os given by the VRT engine for write requests when
 * we are in rebuilding. This will perform the I/Os sequentially: first
 * on up-to-date devices and then on non-up-to-date devices.
 *
 * This function perform the 2nd part.
 *
 * Called from the RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE state.
 *
 * @param[in] vrt_req      The request header from the VRT engine
 *
 * return wether some more IOs are needed.
 */
static bool rain1_fill_io_write_second(struct vrt_request *vrt_req)
{
    struct vrt_io_op *io;
    bool more_ios = false;

    EXA_ASSERT(vrt_req->iotype == VRT_IO_TYPE_WRITE
               || vrt_req->iotype == VRT_IO_TYPE_WRITE_BARRIER);

    /* Initialize barrier to BARRIER_DONT_PROCESS
     * FIXME why ? */
    vrt_req->barrier->state = BARRIER_DONT_PROCESS;

    for (io = vrt_req->io_list; io != NULL; io = io->next)
	if (io->rdev != NULL && io->state == IO_DONT_PROCESS)
	{
	    io->state = IO_TO_PROCESS;
	    more_ios = true;
	}

    return more_ios;
}

/**
 * Fill the list of I/Os given by the VRT engine for writing metadata.
 * If the disk group is not rebuilding, all the I/Os will be
 * performed. If the disk group is rebuilding, only I/Os on uptodate
 * locations will be performed.
 *
 * Called from the RAIN1_REQUEST_DO_METADATA_WRITE state.
 *
 * @param[in] vrt_req      The request header from the VRT engine
 */
static void rain1_fill_io_metadata(struct vrt_request *vrt_req)
{
    struct rdev_location rdev_loc[3];
    unsigned int nb_rdev_loc;

    rain1_group_t *lg = RAIN1_GROUP(vrt_req->ref_vol->group);
    unsigned int dzone_index, slot_index;
    assembly_volume_t *subspace = vrt_req->ref_vol->assembly_volume;
    slot_desync_info_t *block;
    bool process_all_replicas;

    /* FIXME : all these manipulations to get the metadata locations on devices
     * should be isolated with the dzone metadata treatments */

    /* Determine the dirty zone concerned, and the metadata block
     * containing the metadata of this dirty zone */
    rain1_volume2dzone(lg, subspace, vrt_req->ref_bio->start_sector,
		       &slot_index, &dzone_index);

    /* Find what are the physical locations of the metadata block. */
    rain1_dzone2rdev(lg, subspace->slots[slot_index], vrt_node_get_local_id(),
                     rdev_loc, &nb_rdev_loc, 3);

    block = subspace->slots[slot_index]->private;

    process_all_replicas = !rain1_group_is_rebuilding(lg);

    rain1_init_io_replicas(vrt_req, rdev_loc, nb_rdev_loc,
                           block->in_memory_metadata, METADATA_BLOCK_SIZE,
                           VRT_IO_TYPE_WRITE_BARRIER, process_all_replicas);
}

static bool rain1_fill_io_metadata_second(struct vrt_request *vrt_req)
{
    /* FIXME why are these two functions the same ?? */
    return rain1_fill_io_write_second(vrt_req);
}

/**
 * This function handles metadata writing for the given request. It
 * determines whether a metadata write is needed, and if so calls the
 * do_write_metadata() to do it. Otherwise, it waits until the
 * concerned metadata block has been written to the disk.
 *
 * @param[in] vrt_req The write request for which metadata handling
 *                    has to be done.
 *
 * @return TRUE if metadata has to be written, FALSE otherwise.
 */

static rain1_request_state_t rain1_metadata_write_needed(struct vrt_request *vrt_req)
{
    unsigned int slot_index, dzone_index;
    rain1_group_t *lg = RAIN1_GROUP(vrt_req->ref_vol->group);
    slot_desync_info_t *block;
    rain1_request_state_t state;
    desync_info_t *in_mem;
    desync_info_t *on_disk;
    assembly_volume_t *subspace = vrt_req->ref_vol->assembly_volume;

    /* Determine the dirty zone concerned, and the metadata block
       containing the metadata of this dirty zone */
    rain1_volume2dzone(lg, subspace, vrt_req->ref_bio->start_sector,
		       &slot_index, &dzone_index);

    block = subspace->slots[slot_index]->private;

    os_thread_mutex_lock(&block->lock);

    if (block->ongoing_flush)
    {
	/* add the request in the waiting list */
	list_add_tail(&vrt_req->wait_list, &block->wait_avail_list);
	os_thread_mutex_unlock(&block->lock);
	return RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL;
    }

    in_mem = &block->in_memory_metadata[dzone_index];
    on_disk = &block->on_disk_metadata[dzone_index];

    in_mem->write_pending_counter++;
    EXA_ASSERT(in_mem->write_pending_counter <= vrt_get_max_requests());

    /* We must write the metadata to the disk before performing the real
     * requests in two different cases:
     *
     * 1) The write_pending_counter is 1 in memory and 0 on disk, which means
     *     we're current the only write request on this dirty zone. So we must
     *     mark it as being accessed before really writing to it.
     *
     * 2) The global sync_tag is different from the sync_tag
     *    of the dirty zone we're accessing. The global sync_tag is
     *    updated when a device down event is received, and represent
     *    the current state of the devices. All writes must make sure
     *    that the dirty zone they are touching are properly marked with
     *    this sync_tag.
     */

    if ((in_mem->write_pending_counter == 1  &&
	 on_disk->write_pending_counter == 0) ||
	!sync_tag_is_equal(in_mem->sync_tag, lg->sync_tag))
    {
        in_mem->sync_tag = lg->sync_tag;

	/* Mark the block as busy so that other processes will block
	   until the metadata write is complete */
	block->ongoing_flush = true;

	state = RAIN1_REQUEST_START_METADATA_WRITE;
    } else
        state = RAIN1_REQUEST_START_USER_DATA_WRITE;

    os_thread_mutex_unlock(&block->lock);

    return state;
}

static void cancel_requests_waiting_write(slot_desync_info_t *block)
{
    struct vrt_request *vrt_req_waiting;
    struct vrt_request *vrt_req_temp;

    list_for_each_entry_safe(vrt_req_waiting, vrt_req_temp,
                             &block->wait_write_list, wait_list, struct vrt_request)
    {
        unsigned int slot_index, dzone_index;
        desync_info_t *in_mem;
        const rain1_group_t *rxg = RAIN1_GROUP(vrt_req_waiting->ref_vol->group);

	EXA_ASSERT(vrt_req_waiting->next == NULL);
	EXA_ASSERT(rain1_req_get_state(vrt_req_waiting) == RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH);

        list_del(&vrt_req_waiting->wait_list);

        rain1_volume2dzone(rxg, vrt_req_waiting->ref_vol->assembly_volume,
                           vrt_req_waiting->ref_bio->start_sector,
                           &slot_index, &dzone_index);

        in_mem = &block->in_memory_metadata[dzone_index];

        EXA_ASSERT(in_mem->write_pending_counter > 0);
        in_mem->write_pending_counter--;

        if (in_mem->write_pending_counter == 0)
            block->flush_needed = true;

        rain1_req_set_state(vrt_req_waiting, RAIN1_REQUEST_IOERROR_TRIGGERED);
        vrt_wakeup_request(vrt_req_waiting);
    }
}

static void schedule_requests_waiting_write(slot_desync_info_t *block)
{
    struct vrt_request *vrt_req_waiting;
    struct vrt_request *vrt_req_temp;

    list_for_each_entry_safe(vrt_req_waiting, vrt_req_temp,
                             &block->wait_write_list, wait_list, struct vrt_request)
    {
	EXA_ASSERT(vrt_req_waiting->next == NULL);
	EXA_ASSERT(rain1_req_get_state(vrt_req_waiting) == RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH);

        list_del(&vrt_req_waiting->wait_list);

        rain1_req_set_state(vrt_req_waiting, RAIN1_REQUEST_START_USER_DATA_WRITE);
	vrt_wakeup_request(vrt_req_waiting);
    }
}

static void cancel_requests_waiting_avail(slot_desync_info_t *block)
{
    struct vrt_request *vrt_req_waiting;
    struct vrt_request *vrt_req_temp;

    list_for_each_entry_safe(vrt_req_waiting, vrt_req_temp,
                             &block->wait_avail_list, wait_list, struct vrt_request)
    {
        list_del(&vrt_req_waiting->wait_list);
        EXA_ASSERT(rain1_req_get_state(vrt_req_waiting) == RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL);
        rain1_req_set_state(vrt_req_waiting, RAIN1_REQUEST_IOERROR_TRIGGERED);
        vrt_wakeup_request(vrt_req_waiting);
    }
}

/**
 * Travel through the list of requests that were waiting for this metadata
 * block, and do two different things depending on the request:
 * - if the request doesn't need to write metadata, increment the write pending
 *   counter and wake it up in the RAIN1_REQUEST_BEGIN_WRITE immediatly
 * - otherwise, put it in the wait_write_list of the metadata block
 */
static void schedule_requests_waiting_avail(slot_desync_info_t *block)
{
    struct vrt_request *vrt_req_waiting;
    struct vrt_request *vrt_req_temp;

    /* FIXME: We use a local variable instead of "block->ongoing_flush" to get
     * exactly the same behavior as former code.  It could be removed if we
     * ensure block->ongoing_flush == false when entering the function.
     */
    bool local_ongoing_flush = false;

    EXA_ASSERT(list_empty(&block->wait_write_list));

    list_for_each_entry_safe(vrt_req_waiting, vrt_req_temp, &block->wait_avail_list, wait_list, struct vrt_request)
    {
	unsigned int dzone_index, slot_index;
        desync_info_t *in_mem;
        bool metadata_flush_needed;
        const rain1_group_t *rxg = RAIN1_GROUP(vrt_req_waiting->ref_vol->group);

        list_del(&vrt_req_waiting->wait_list);

        EXA_ASSERT(rain1_req_get_state(vrt_req_waiting) == RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL);
	EXA_ASSERT(vrt_req_waiting->next == NULL);

        /* Determine the dirty zone concerned, and the metadata block
	   containing the metadata of this dirty zone */
	rain1_volume2dzone(rxg,
                           vrt_req_waiting->ref_vol->assembly_volume,
                           vrt_req_waiting->ref_bio->start_sector,
			   &slot_index, &dzone_index);

        in_mem = &block->in_memory_metadata[dzone_index];

        metadata_flush_needed = (in_mem->write_pending_counter == 0
                                 || sync_tag_is_greater(rxg->sync_tag, in_mem->sync_tag));

        EXA_ASSERT(in_mem->write_pending_counter < vrt_get_max_requests());

        in_mem->write_pending_counter++;
        in_mem->sync_tag = sync_tag_max(rxg->sync_tag, in_mem->sync_tag);

        if (!metadata_flush_needed)
        {
            /* Tell the engine to write the request data */
            rain1_req_set_state(vrt_req_waiting, RAIN1_REQUEST_START_USER_DATA_WRITE);
            vrt_wakeup_request(vrt_req_waiting);
        }
        else if (!local_ongoing_flush)
        {
            local_ongoing_flush = true;

            /* Tell the engine to flush the slot metadata corresponding to the request */
            rain1_req_set_state(vrt_req_waiting, RAIN1_REQUEST_START_METADATA_WRITE);
            vrt_wakeup_request(vrt_req_waiting);
        }
        else
        {
            /* Put the request back aside until the corresponding metadata is flushed */
            rain1_req_set_state(vrt_req_waiting, RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH);
            list_add_tail(&vrt_req_waiting->wait_list, &block->wait_write_list);
        }
    }

    block->ongoing_flush = local_ongoing_flush;

    EXA_ASSERT(list_empty(&block->wait_avail_list));
}

void rain1_schedule_aggregated_metadata(slot_desync_info_t *block, bool failure)
{
    if (failure)
    {
        /* FIXME: this termination on failure may lead to the desynchronization
         * between in-memory and on-disk meta-data that is observed in
         * 'rain1_group_metadata_flush' (see bug #4496)
         *
         * When the writing of some metadata fails (due to a disk down for
         * example) the on-disk and in-memory state is not changed but the
         * metadata block is tagged 'AVAILABLE' anyway. Then the flushing thread
         * can treat the block and find this unexpected desynchronization.
         *
         * This desynchronization concerns only the metadata writings before a
         * data writing (0->1), which mean that if the metadata writing has been
         * interrupted the data writing never occured and the storage is not
         * endangered.
         */

        cancel_requests_waiting_avail(block);
        block->ongoing_flush = false;
    }
    else
        schedule_requests_waiting_avail(block);
}

/**
 * This function is called when the metadata write I/Os have been
 * completed. It marks the metadata block as AVAILABLE and wakes up
 * processes waiting for the availibility of this block.
 *
 * @param[in] vrt_req The request
 */
static void block_signal_metadata_write_end(struct vrt_request *vrt_req,
                                            bool failure)
{
    assembly_volume_t *subspace = vrt_req->ref_vol->assembly_volume;
    rain1_group_t *lg = RAIN1_GROUP(vrt_req->ref_vol->group);
    slot_desync_info_t *block;
    unsigned int dzone_index, slot_index;

    /* Find the dirty zone and the metadata block */
    rain1_volume2dzone(lg, subspace, vrt_req->ref_bio->start_sector,
		       &slot_index, &dzone_index);

    block = subspace->slots[slot_index]->private;

    /* Here we must take the block->lock to avoid a race
       condition with the handle_write_metadata() function. */
    os_thread_mutex_lock(&block->lock);

    /* The "in_memory" and "on_disk" versions of the metadata are now
       synchronized as the "in_memory" version of metadata do not change
       during the metadata writing. */
    if (!failure)
    {
	memcpy(block->on_disk_metadata, block->in_memory_metadata,
               METADATA_BLOCK_SIZE);
        block->flush_needed = false;
    }

    /* The requests that were waiting for the completion of the write of
       this metadata block can be woken up, and they can restart in the
       RAIN1_REQUEST_BEGIN_WRITE because we are sure that the metadata
       are correctly updated with regard to these requests. */

    if (failure)
        cancel_requests_waiting_write(block);
    else
        schedule_requests_waiting_write(block);

    rain1_schedule_aggregated_metadata(block, failure);

    os_thread_mutex_unlock(&block->lock);

    /* wake up the thread to build the woken up requests */
    vrt_thread_wakeup ();
}

/**
 * This function is called at the end of the write request. It allows
 * to decrement the pending write counter, so that when all writes are
 * done, the counter is equal to 0.
 *
 * @param[in] vrt_req The request
 */
static void block_signal_pending_write_end(struct vrt_request *vrt_req)
{
    unsigned int slot_index, dzone_index;
    rain1_group_t *lg = RAIN1_GROUP(vrt_req->ref_vol->group);
    assembly_volume_t *subspace = vrt_req->ref_vol->assembly_volume;
    slot_desync_info_t *block;
    desync_info_t *in_mem;

    /* Find the dirty zone and the metadata block */
    rain1_volume2dzone(lg, subspace, vrt_req->ref_bio->start_sector,
		       &slot_index, &dzone_index);

    block = subspace->slots[slot_index]->private;

    os_thread_mutex_lock(&block->lock);

    in_mem = &block->in_memory_metadata[dzone_index];

    EXA_ASSERT(in_mem->write_pending_counter > 0);
    in_mem->write_pending_counter--;

    if (in_mem->write_pending_counter == 0)
	block->flush_needed = true;

    os_thread_mutex_unlock(&block->lock);
}

/**
 * Look for IO error in the list of IOs and its potential barrier.
 *
 * @param[in] vrt_req      The request header from the VRT engine
 *
 * @return true if there is 1 or more errors. false elsewhere.
 */
static bool rain1_request_has_error(struct vrt_request *vrt_req)
{
    struct vrt_io_op *io;

    if(vrt_req->barrier != NULL && vrt_req->barrier->state == BARRIER_FAILURE)
        return true;

    for (io = vrt_req->io_list; io != NULL; io = io->next)
	if (io->state == IO_FAILURE)
	    return true;

    return false;
}

static rain1_request_state_t rain1_request_fail(struct vrt_request *vrt_req)
{
    rain1_request_state_t state = rain1_req_get_state(vrt_req);

    switch (state)
    {
    case RAIN1_REQUEST_START_METADATA_WRITE:
    case RAIN1_REQUEST_DO_METADATA_WRITE:
    case RAIN1_REQUEST_CONTINUE_METADATA_WRITE:
            /* Fail the metadata and user request */
            block_signal_metadata_write_end(vrt_req, true);
            block_signal_pending_write_end(vrt_req);
            break;

    case RAIN1_REQUEST_START_USER_DATA_WRITE:
    case RAIN1_REQUEST_DO_USER_BARRIER_WRITE:
    case RAIN1_REQUEST_DO_USER_DATA_WRITE:
    case RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE:
    case RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH:
            /* Fail the user request */
            block_signal_pending_write_end(vrt_req);
            break;

    case RAIN1_REQUEST_IOERROR_TRIGGERED:
    case RAIN1_REQUEST_READ:
    case RAIN1_REQUEST_SUCCESS:
    case RAIN1_REQUEST_FAILED:
    case RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL:
    case RAIN1_REQUEST_BEGIN:
            /* Nothing to fail there */
            break;
    }

    return RAIN1_REQUEST_FAILED;
}

static vrt_req_status_t __rain1_build_io_for_read_req(struct vrt_request *vrt_req)
{
    rain1_request_state_t state = rain1_req_get_state(vrt_req);

    EXA_ASSERT(RAIN1_REQUEST_STATE_IS_VALID(state));

    if (rain1_request_has_error(vrt_req))
        state = rain1_request_fail(vrt_req);
    else
    {
        switch (state)
        {
        case RAIN1_REQUEST_BEGIN:
            state = rain1_fill_io_read(vrt_req);
            break;

        case RAIN1_REQUEST_READ:
                state = RAIN1_REQUEST_SUCCESS;
            break;

        /* Read requests do not require any metadata write */
        case RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL:
        case RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH:
        case RAIN1_REQUEST_START_METADATA_WRITE:
        case RAIN1_REQUEST_DO_METADATA_WRITE:
        case RAIN1_REQUEST_CONTINUE_METADATA_WRITE:
        case RAIN1_REQUEST_START_USER_DATA_WRITE:
        case RAIN1_REQUEST_DO_USER_BARRIER_WRITE:
        case RAIN1_REQUEST_DO_USER_DATA_WRITE:
        case RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE:
        case RAIN1_REQUEST_SUCCESS:
        case RAIN1_REQUEST_IOERROR_TRIGGERED:
        case RAIN1_REQUEST_FAILED:
            EXA_ASSERT(false);
        }
    }

    return rain1_req_set_state(vrt_req, state);
}

static vrt_req_status_t __rain1_build_io_for_write_req(struct vrt_request *vrt_req)
{
    rain1_request_state_t state;

    state = rain1_req_get_state(vrt_req);

    if (rain1_request_has_error(vrt_req))
        state = rain1_request_fail(vrt_req);
    else
    {
        switch (state)
        {

        /* New request, compute initial state */
        case RAIN1_REQUEST_BEGIN:
        case RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL:
        case RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH:
            state = rain1_metadata_write_needed(vrt_req);
            break;

        /* Layout metadata writing states */
        case RAIN1_REQUEST_START_METADATA_WRITE:
	    rain1_fill_barrier(vrt_req);
            state = RAIN1_REQUEST_DO_METADATA_WRITE;
            break;

        case RAIN1_REQUEST_DO_METADATA_WRITE:
            rain1_fill_io_metadata(vrt_req);
            state = RAIN1_REQUEST_CONTINUE_METADATA_WRITE;
            break;

        case RAIN1_REQUEST_CONTINUE_METADATA_WRITE:
            if (rain1_fill_io_metadata_second(vrt_req))
                state = RAIN1_REQUEST_CONTINUE_METADATA_WRITE;
            else
            {
                block_signal_metadata_write_end(vrt_req, false);
                state = RAIN1_REQUEST_START_USER_DATA_WRITE;
            }
            break;

        /* Start of user data handling. */
        case RAIN1_REQUEST_START_USER_DATA_WRITE:
            if (vrt_req->iotype == VRT_IO_TYPE_WRITE)
                state = RAIN1_REQUEST_DO_USER_DATA_WRITE;
            else
                state = RAIN1_REQUEST_DO_USER_BARRIER_WRITE;
            break;

        /* User data writing state of barrier I/Os. */
        case RAIN1_REQUEST_DO_USER_BARRIER_WRITE:
            rain1_fill_barrier(vrt_req);
            state = RAIN1_REQUEST_DO_USER_DATA_WRITE;
            break;

        /* User data writing states of normal I/Os. */
        case RAIN1_REQUEST_DO_USER_DATA_WRITE:
            rain1_fill_io_write(vrt_req);
            state = RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE;
            break;

        case RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE:
            if (rain1_fill_io_write_second(vrt_req))
                state = RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE;
            else
            {
                block_signal_pending_write_end(vrt_req);
                state = RAIN1_REQUEST_SUCCESS;
            }
            break;

        /* IO error */
        case RAIN1_REQUEST_IOERROR_TRIGGERED:
            state = RAIN1_REQUEST_FAILED;
	    /* no need to call block_signal_pending_write_end() because when we
             * where woken up in the state RAIN1_REQUEST_IOERROR_TRIGGERED,
             * write_pending_counter was decremented */
            break;

        case RAIN1_REQUEST_READ:
        case RAIN1_REQUEST_SUCCESS:
        case RAIN1_REQUEST_FAILED:
            EXA_ASSERT(false);
        }
    }

    /* Update state */
    return rain1_req_set_state(vrt_req, state);
}

/**
 * Function called by the virtualizer to fill the list of I/O to
 * process in order to perform the request described by the given
 * vrt_request. This function is called several times, because the
 * layout may need several successive rounds of I/O requests, or
 * because of errors.
 *
 * This function can use the private_data field of the vrt_request to
 * store information that will allow it to know at which step of the
 * process it is.
 *
 * @param vrt_req The request header
 */
vrt_req_status_t rain1_build_io_for_req(struct vrt_request *vrt_req)
{
    EXA_ASSERT(VRT_IO_TYPE_IS_VALID(vrt_req->iotype));

    switch (vrt_req->iotype)
    {
    case VRT_IO_TYPE_WRITE:
    case VRT_IO_TYPE_WRITE_BARRIER:
        return __rain1_build_io_for_write_req(vrt_req);

    case VRT_IO_TYPE_READ:
        return __rain1_build_io_for_read_req(vrt_req);

    case VRT_IO_TYPE_NONE:
        EXA_ASSERT(false);
    }

    EXA_ASSERT(false);

    /* Can't happen */
    return VRT_REQ_FAILED;
}

/**
 * Initialize a request
 *
 * @param[in] vrt_req The request
 */
void
rain1_init_req (struct vrt_request *vrt_req)
{
    rain1_req_set_state(vrt_req, RAIN1_REQUEST_BEGIN);
}

/**
 * Cancel a request
 *
 * @param[in] vrt_req The request to cancel
 */
void
rain1_cancel_req (struct vrt_request *vrt_req)
{
    rain1_request_state_t state;

    state = rain1_req_get_state(vrt_req);
    switch (state)
    {
    case RAIN1_REQUEST_START_METADATA_WRITE:
    case RAIN1_REQUEST_DO_METADATA_WRITE:
    case RAIN1_REQUEST_CONTINUE_METADATA_WRITE:
	block_signal_metadata_write_end(vrt_req, true);
        break;

    case RAIN1_REQUEST_START_USER_DATA_WRITE:
    case RAIN1_REQUEST_DO_USER_BARRIER_WRITE:
    case RAIN1_REQUEST_DO_USER_DATA_WRITE:
    case RAIN1_REQUEST_CONTINUE_USER_DATA_WRITE:
    case RAIN1_REQUEST_POSTPONED_UNTIL_FLUSH:
        block_signal_pending_write_end(vrt_req);
        break;

    case RAIN1_REQUEST_READ:
    case RAIN1_REQUEST_BEGIN:
    case RAIN1_REQUEST_POSTPONED_UNTIL_AVAIL:
    case RAIN1_REQUEST_IOERROR_TRIGGERED:
        /* Nothing to do in these cases */
        break;

    case RAIN1_REQUEST_SUCCESS:
    case RAIN1_REQUEST_FAILED:
        /* We shouldn't be there */
        EXA_ASSERT(false);
    }

    rain1_req_set_state(vrt_req, RAIN1_REQUEST_BEGIN);
}

/**
 * Function called by the virtualizer to know what are the needs for
 * I/O structure of the layout. It is called once for each request
 * received by the VRT engine.
 *
 * @param[in] vrt_req The descriptor of the original request.
 *
 * @param[out] io_count        Number of I/Os needed to process the request.
 * @param[out] sync_afterward  Ask to make a barriere once IO is done.
 */
void rain1_declare_io_needs(struct vrt_request *vrt_req,
                            unsigned int *io_count,
                            bool *sync_afterward)
{
    EXA_ASSERT(vrt_req != NULL);
    EXA_ASSERT(io_count != NULL);
    EXA_ASSERT(sync_afterward != NULL);

    EXA_ASSERT(VRT_IO_TYPE_IS_VALID(vrt_req->iotype));
    switch (vrt_req->iotype)
    {
    case VRT_IO_TYPE_WRITE:
    case VRT_IO_TYPE_WRITE_BARRIER:
	*io_count = 3;
        /* If the I/O type is WRITE_BARRIER, the VRT engine MUST
         * perform a barrier because it is requested by the upper
         * layer.
         * If the I/O type is simply WRITE, the VRT engine MAY perform
         * a barrier because the requested write MAY trigger metadata
         * writes (performed with barriers).
         */
        *sync_afterward = true;
        break;

    case VRT_IO_TYPE_READ:
	*io_count = 1;
        *sync_afterward = false;
        break;

    case VRT_IO_TYPE_NONE:
        EXA_ASSERT(false);
    }
}
