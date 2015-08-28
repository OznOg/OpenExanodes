/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/**@file vrt_request.c
 *
 * @brief This file contains the heart of the VRT engine, through
 * which all I/O requests performed on the Exanodes virtual block
 * device are performed. Its goal is to ask the layout how to process
 * the requests, and to perform them accordingly.
 *
 * The entry point of the VRT engine is the vrt_make_request()
 * function. This function is the make_request() function of the
 * Exanodes virtual block device : it replaces the default
 * __make_request() function available in drivers/block/ll_rw_blk.c in
 * the Linux kernel. Every time a request is issued on the virtual
 * block device, this vrt_make_request() function is called with a bio
 * describing the request to perform.
 *
 * The virtualizer doesn't use any request waitqueue, because it
 * doesn't need to cluster the requests. It only redirects the I/O
 * requests made on volumes (virtual storage space) to the real
 * devices containing the data (real storage space). So we don't
 * implement a request() function (like in the NBD) but rather a
 * make_request() function.
 *
 * To perform the request, the VRT engine first allocates a so-called
 * vrt_request. Each vrt_request represents a single request issued by
 * the kernel. Once this structure has been allocated and initialized,
 * the engine asks the layout for its needs in terms of struct
 * vrt_io_op. An <i>IO</i> represents an I/O that the layout needs to
 * make to perform the global request. With this information, the VRT
 * engine allocates enough struct vrt_io_op, and the corresponding
 * number of bios.
 *
 * Once this is done, the engine asks the layout to fill the IO
 * structures. The layout can describe which IO needs to be made. Once
 * this is done, the engine fills the corresponding bio
 * (vrt_request_prepare()) and perform the I/Os (vrt_request_perform()).
 *
 * When an I/O is terminated, the vrt_end_io() function gets called by
 * the underlying block device driver (because the b_end_io field of
 * each bio is set to point to this function). This function
 * changes the state of the I/O to either IO_OK or IO_FAILURE, and if
 * it's the last I/O of the vrt_request, then vrt_next_step_io() is
 * called.
 *
 * This function asks again the layout for more I/O to perform. There
 * can be two reasons for which there might be more I/O :
 *  - because some of the I/Os reported failure
 *  - because the layout needs multiple I/O rounds to complete the
 *    global request (for example in a RAID5-like layout)
 *
 * So, the layout can eventually fill the I/O again. If it didn't fill
 * any new I/O, then the global request is terminated : all data
 * structures are freed and we signal the kernel that the request is
 * completed (using the b_end_io function of the original buffer
 * head).
 *
 * Otherwise, the new I/O have to be performed. As the
 * vrt_next_step_io() is executed in an interrupt context, we cannot ask
 * the kernel to perform this I/O directly. Instead, we had them in a
 * global linked list (vrt_pending_req_list), and wake up a kernel
 * thread (vrt_thread()), which will later on process the I/Os of all
 * pending requests in the global linked list. To do so, the thread
 * fills the bios and calls vrt_request_perform(), just as we
 * did for the first round of I/Os.
 *
 * When these new I/Os are completed, the vrt_end_io() function is
 * called again, and the process repeats again and again, until the
 * layout says that everything is done.
 */

#include <string.h>

#include "vrt/virtualiseur/include/vrt_request.h"

#include "common/include/exa_error.h"
#include "common/include/threadonize.h"

#include "os/include/os_time.h"
#include "os/include/os_atomic.h"
#include "os/include/os_error.h"

#include "log/include/log.h"

#include "vrt/common/include/waitqueue.h"

#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_mempool.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_perf.h"
#include "vrt/virtualiseur/include/vrt_stats.h"

#include "common/include/exa_nbd_list.h" /* for bio pool */

#include "vrt/virtualiseur/src/vrt_module.h"

/** Replaying requests:
 *
 * When a write request returns IO error, it may mean that the location is
 * locked by the NBD because of the rebuilding. In this case, we try the same
 * write request later. It is the "replay request" feature.
 *
 * We do not try to perform the replayed request immediately in order to avoid
 * busy wait. Instead, we wait VRT_REPLAY_REQUEST_DELAY_MSEC milliseconds before
 * performing IO again. Each struct vrt_request has a "replay_date"
 * field specifying the date from we can perform the IO.
 */
#define VRT_REPLAY_REQUEST_DELAY_MSEC 1000

/**
 * Memory pools for the allocation of struct vrt_request, struct
 * vrt_io_op and bios.
 */
struct vrt_object_pool *vrt_req_pool, *io_pool, *bio_pool, *barrier_pool;

static struct {
    os_thread_t tid;    /** vrt thread id */
    bool ask_terminate; /** ask the vrt_thread() to terminate */
    os_atomic_t event;
    wait_queue_head_t wq;
} vrt_thread;

/**
 * The list of request headers that needs to be processed by the
 * thread.
 *
 * We do not use the kernel <linux/list.h> facilities, because we
 * don't want to hold the lock for too much time in the kernel
 * thread. Using a head and tail pointer, we can simply save them in
 * the thread, set them to NULL, and release the lock.
 *
 * We have both a head and tail pointers because we want to add the
 * new request headers to be processed at the end of the list, in
 * order to keep the good order.
 */
struct vrt_request_list
{
    struct vrt_request *head;
    struct vrt_request *tail;
    unsigned int count;
    os_thread_mutex_t lock;
};

/** List of requests reinitialized by vrt_request_cancel(). Requests in that
 * list are not counted in initialized_request_count.
 */

static struct vrt_request_list vrt_suspended_req_list;

/** List of requests which need to be rebuilt with build_io_for_req. They are
 * added in that list by layouts with vrt_wakeup_request() or when the group is
 * resumed from vrt_suspended_req_list. Requests in that list are counted in
 * initialized_request_count.
 */

static struct vrt_request_list vrt_tobuild_req_list;

/** List of requests already built by build_io_for_req. Their IO are ready to
 * perform. Requests in that list are counted in initialized_request_count.
 */

static struct vrt_request_list vrt_pending_req_list;

/* XXX 512 is legacy, I do not know if this value is not too large nor large
 * enougth*/
#define NB_BIO_PER_POOL 512
struct nbd_root_list pool_of_bio;

static void bio_put(blockdevice_io_t *bio)
{
    nbd_list_post(&pool_of_bio.free, bio, -1);
}

static blockdevice_io_t *bio_alloc(void)
{
   blockdevice_io_t *bio =  nbd_list_remove(&pool_of_bio.free, NULL, LISTWAIT);
   EXA_ASSERT(bio != NULL);

   return bio;
}

/**
 * Initialize a list of requests.
 *
 * @param[in] list    The list of requests
 */
static void vrt_req_list_init(struct vrt_request_list *list)
{
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    os_thread_mutex_init(&list->lock);
}

static void vrt_req_list_clean(struct vrt_request_list *list)
{
    os_thread_mutex_destroy(&list->lock);
}

/**
 * Add a request header in a list of requests to be handled
 * later by the virtualizer thread
 *
 * @param[in] list    The list of requests
 * @param[in] vrt_req The request header
 */
static void vrt_req_list_add(struct vrt_request_list *list, struct vrt_request *vrt_req)
{
    os_thread_mutex_lock(& list->lock);

    EXA_ASSERT (vrt_req->next == NULL);

    /* Add the request to the list of pending requests that have to
       be handled by the kernel thread */
    if (list->tail != NULL)
    {
	EXA_ASSERT (list->head != NULL);
	list->tail->next = vrt_req;
	list->tail       = vrt_req;
    }
    else
    {
	EXA_ASSERT (list->head == NULL);
	list->tail = vrt_req;
	list->head = vrt_req;
    }

    list->count ++;

    os_thread_mutex_unlock(& list->lock);
}

/**
 * Take a list of requests from the specified list to be handled
 * by the virtualizer thread. The caller can go through the list
 * with ->next.
 *
 * The specified list will be empty after this function.
 *
 * @param[in] list    The list of requests
 * @return The first request header of the list
 */
static struct vrt_request *vrt_req_list_take(struct vrt_request_list *list)
{
    struct vrt_request *cur;

    /* The global list of tobuild request headers can be accessed
       concurrently by other functions (e.g. vrt_end_io() and
       friends). */
    os_thread_mutex_lock(& list->lock);

    /* As the semaphore used to wake us up is not a binary
       semaphore, we can be woken up without anything to do (all
       requests were processed by a previous turn of the loop). We
       detect this case early and go to sleep again. */
    if (list->count == 0)
    {
	EXA_ASSERT (list->head == NULL);
	EXA_ASSERT (list->tail == NULL);
	os_thread_mutex_unlock(& list->lock);
	return NULL;
    }

    /* Keep the list head somewhere in a local variable, empty the
       list and immediatly release the lock, so that it doesn't
       remain held for a long time */
    EXA_ASSERT (list->head != NULL);
    EXA_ASSERT (list->tail != NULL);
    cur = list->head;
    list->head  = NULL;
    list->tail  = NULL;
    list->count = 0;

    os_thread_mutex_unlock(& list->lock);

    return cur;
}

/**
 * Check that all io / barriers returned before recall build_io_for_req
 * */
static int vrt_check_for_build_io_for_req(struct vrt_request *request)
{
    struct vrt_io_op *io;
    struct vrt_barrier_op *barrier;

    for (io = request->io_list; io != NULL; io = io->next)
    {
	if (io->state != IO_DONT_PROCESS &&
	    io->state != IO_OK           &&
	    io->state != IO_FAILURE)
	{
	    exalog_error("io %p in req %p in invalid state: %d",
                         io, request, io->state);
	    return FALSE;
	}
    }

    barrier = request->barrier;
    if (barrier != NULL
        && barrier->state != BARRIER_DONT_PROCESS
        && barrier->state != BARRIER_OK
        && barrier->state != BARRIER_FAILURE)
    {
        exalog_error("barrier in req %p in invalid state: %d",
                     request, barrier->state);
        return FALSE;
    }

    return TRUE;
}

/**
 * Replay the request later
 */
static void vrt_request_schedule_replay(struct vrt_request *vrt_req)
{
    /* add the request in the pending list in order to be replayed later */
    vrt_req->replay_date = os_gettimeofday_msec() + VRT_REPLAY_REQUEST_DELAY_MSEC;
    vrt_req_list_add(&vrt_pending_req_list, vrt_req);
}

/**
 * This function is called by vrt_end_io() every time all I/O in a
 * given request header are finished. The aim of this function is to
 * ask the layout for more I/O or the result of the request.
 *
 * @param[in] vrt_req The request header corresponding to the request currently
 * being handled
 */
static void vrt_next_step_io(struct vrt_request *vrt_req)
{
    struct vrt_io_op *cur_io;

    EXA_ASSERT(vrt_req);
    EXA_ASSERT(vrt_req->ref_vol);

    EXA_ASSERT(vrt_req->next == NULL);

    /* if some vrt_io_op are still in IO_TO_PROCESS, we need to process again
     * this request. This can happen when the NBD locking block a request. */
    for (cur_io = vrt_req->io_list; cur_io != NULL; cur_io = cur_io->next)
	if (cur_io->state == IO_TO_PROCESS)
        {
            vrt_request_schedule_replay(vrt_req);
            return;
        }

    vrt_req_list_add(&vrt_tobuild_req_list, vrt_req);
    vrt_thread_wakeup();
}

/**
 * This callback is invoked every time a disk barrier is completed.
 * Its purpose is to update the volume barrier status and decrease the
 * count of disks on which the volume barrier is pending.
 *
 * @param[out] private_data  A volume barrier.
 * @param[in]  err           The error returned by bd_submit_barrier().
 */
static void vrt_barrier_cb(blockdevice_io_t *bio, int err)
{
    struct vrt_barrier_op *barrier = bio->private_data;

    if (err)
	barrier->state = BARRIER_FAILURE;
    else if (barrier->state != BARRIER_FAILURE)
	barrier->state = BARRIER_OK;

    bio_put(bio);

    if (!os_atomic_dec_and_test(&barrier->remaining_disks))
	return;

    if (os_atomic_dec_and_test(&barrier->vrt_req->remaining))
	vrt_next_step_io(barrier->vrt_req);
}

/**
 * This function is called asynchronously by the kernel when an I/O is
 * terminated.
 *
 * @param[in] bio         The bio corresponding to the terminated I/O
 * @param[in] bytes_done  Number of bytes that were completed
 * @param[in] error       Result of the bio submission
 */
static void vrt_end_io(blockdevice_io_t *bio, int error)
{
    struct vrt_io_op *ref_io = bio->private_data;

    EXA_ASSERT(ref_io->state == IO_TO_PROCESS);

    /* An IO FAILURE may be either a real error or a temporary error due to
     * the NBD locking. */
    switch (error)
    {
    case EXA_SUCCESS:
        /* It was not an error */
        ref_io->state = IO_OK;
        break;

    case -EIO:
        /* real IO error */
        ref_io->state = IO_FAILURE;
        break;

    case -EAGAIN:
        /* The zone was locked by the NBD. Retry later. */
        ref_io->state = IO_TO_PROCESS;
        break;

    default:
        EXA_ASSERT_VERBOSE(false, "Unexpected error %d for bio %p\n", error, bio);
        break;
    }

    VRT_PERF_IO_OP_END(ref_io);

    /* If there aren't any remaining processing I/O in the list, we call
       vrt_next_step_io() to process the next step of the request */
    if (os_atomic_dec_and_test(&ref_io->vrt_req->remaining))
	vrt_next_step_io(ref_io->vrt_req);
}

/**
 * Send all I/O requests registered in the given request header to the
 * kernel
 *
 * @param[in] vrt_req The request header
 */
static int vrt_request_perform(struct vrt_request *vrt_req)
{
    struct vrt_io_op *curr_io;

    /* Prevent vrt_next_step_io() to be called before this function ends. */
    os_atomic_inc(& vrt_req->remaining);

    for (curr_io = vrt_req->io_list ; curr_io != NULL ; curr_io = curr_io->next)
    {
        bool flush_cache = false; /* Initialized here to silence ICC */
        blockdevice_io_type_t type = BLOCKDEVICE_IO_READ; /* Same */

	if (curr_io->state != IO_TO_PROCESS)
	    continue;

	EXA_ASSERT (curr_io->vrt_req == vrt_req);
	EXA_ASSERT (curr_io->bio != NULL);

	/* Formerly we tried to 'respect original bio flags' by propagating
         * these flags to NBD.  We removed them because the NBD ignores all
         * these flags except the 'barrier' (and only on writing) one but the
         * barrier information is already handled by 'flush_cache'.
         *
	 * rw = curr_io->iotype |
	 *   (vrt_req->ref_bio->type &
	 *    ((1 << BIO_RW_FAILFAST) | (1 << BIO_RW_SYNC) | (1 << BIO_RW_BARRIER)));
         */

	VRT_PERF_IO_OP_SUBMIT(vrt_req, curr_io);

        switch (curr_io->iotype)
        {
            case VRT_IO_TYPE_READ:
                type = BLOCKDEVICE_IO_READ;
                flush_cache = false;
                break;

            case VRT_IO_TYPE_WRITE:
                type = BLOCKDEVICE_IO_WRITE;
                flush_cache = false;
                break;

            case VRT_IO_TYPE_WRITE_BARRIER:
                type = BLOCKDEVICE_IO_WRITE;
                flush_cache = true;
                break;

            case VRT_IO_TYPE_NONE:
                EXA_ASSERT_VERBOSE(false, "Cannot make a request of type 'none'");
                break;
        }

        blockdevice_submit_io(curr_io->rdev->blockdevice, curr_io->bio, type,
                              curr_io->offset, curr_io->data, curr_io->size,
                              flush_cache, curr_io, vrt_end_io);
    }

    if (vrt_req->barrier != NULL && vrt_req->barrier->state == BARRIER_TO_PROCESS)
    {
        struct vrt_group *group = VRT_REQ_GET_GROUP(vrt_req);
        struct vrt_realdev *rdev;
        storage_rdev_iter_t iter;

	/* FIXME: We invoke bd_submit_barrier() with the appropriate
	 * callback for *all* the disks that are part of the group.
	 * This is sub optimal since the barrier may be not intended for
	 * all disks.
	 */
        storage_rdev_iterator_begin(&iter, group->storage);
        while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
	{
	    if (rdev_is_ok(rdev))
	    {
		blockdevice_io_t *bio = bio_alloc();

                blockdevice_submit_io(rdev->blockdevice, bio, BLOCKDEVICE_IO_WRITE,
                                      0, NULL, 0, true, vrt_req->barrier,
                                      vrt_barrier_cb);
	    }
	}
        storage_rdev_iterator_end(&iter);
    }

    /* If there aren't any remaining processing I/O in the list, we call
       vrt_next_step_io() to process the next step of the request */
    /* FIXME this was introduced to fix bug #1705 , and I think this is not a
     * proper patch: In fact, the os_atomic_inc() at the begining of the
     * function has the side effect to prevent b_end_io() to call
     * vrt_next_step_io() and thus prevent vrt_req to be destroyed while we are
     * iterating on the last IO. The problem is that vrt_req->remaining is
     * supposed to tell the number of IOs that are being performed, and changing
     * this value here is a crappy trick to make a kind of ref count on vrt_req.
     * But the consequence is that we need to check also here that all IO were
     * not finished before leaving, that's why next test is mandatory. */
    if (os_atomic_dec_and_test (& vrt_req->remaining))
	vrt_next_step_io(vrt_req);

    return EXA_SUCCESS;
}

/**
 * Allocate IO structures and associated bios for the given request
 * header. It also builds the chained list of IO structures and
 * associate one bio to each IO structure
 *
 * @param[in] vrt_req The request header for which structures have to be
 * allocated
 *
 * @param[in] io_count The number of IO structures to allocate for the
 * request header (is also the number of bios to allocate, because
 * there's one bio per IO struct)
 *
 * @param[in] sync_afterward  Will this request make a barrier ?
 * the request header.
 */
static void vrt_alloc_structs(struct vrt_request *vrt_req,
                              unsigned int io_count,
                              bool sync_afterward)
{
    int i;

    EXA_ASSERT (io_count != 0);

    vrt_req->io_list = NULL;
    for (i = 0; i < io_count; i++)
    {
	struct vrt_io_op *io;

	io = vrt_mempool_object_alloc (io_pool);
	EXA_ASSERT (io != NULL);

	io->bio = vrt_mempool_object_alloc (bio_pool);
	EXA_ASSERT (io->bio != NULL);

	io->state = IO_DONT_PROCESS;

	io->next = vrt_req->io_list;
	vrt_req->io_list = io;
    }

    vrt_req->barrier = NULL;
    if (sync_afterward)
    {
	struct vrt_barrier_op *barrier;
	barrier = vrt_mempool_object_alloc(barrier_pool);
	EXA_ASSERT(barrier != NULL);

	barrier->state = BARRIER_DONT_PROCESS;
	vrt_req->barrier = barrier;
    }
}

/**
 * Free the IO structures and associated bios of the given request
 * header.
 *
 * @param[in] vrt_req The request header
 */
static void vrt_free_structs(struct vrt_request *vrt_req)
{
    struct vrt_io_op *io;

    io = vrt_req->io_list;
    while (io != NULL)
    {
	struct vrt_io_op *next;
	next = io->next;

	EXA_ASSERT (io->bio != NULL);

	vrt_mempool_object_free (bio_pool, io->bio);
	vrt_mempool_object_free (io_pool, io);

	io = next;
    }

    if (vrt_req->barrier != NULL)
        vrt_mempool_object_free(barrier_pool, vrt_req->barrier);
}

/**
 * Prepare the operations of a request by filling the list of buffer
 * head associated to the list of I/O as built by the layout and
 * computing the number of operations that will be done during the
 * current round.
 *
 * @param[in] vrt_req The request header
 */
static void vrt_request_prepare(struct vrt_request *vrt_req)
{
    struct vrt_io_op *io;

    EXA_ASSERT(os_atomic_read(&vrt_req->remaining) == 0);

    for (io = vrt_req->io_list ; io != NULL ; io = io->next)
    {
	if (io->state != IO_TO_PROCESS)
	    continue;

	EXA_ASSERT(io->bio != NULL);
	EXA_ASSERT(io->data != NULL);

	os_atomic_inc(&vrt_req->remaining);

    }

    if (vrt_req->barrier != NULL && vrt_req->barrier->state == BARRIER_TO_PROCESS)
    {
        struct vrt_group *group = VRT_REQ_GET_GROUP(vrt_req);
        struct vrt_realdev *rdev;
        storage_rdev_iter_t iter;

        os_atomic_set(&vrt_req->barrier->remaining_disks, 0);
        storage_rdev_iterator_begin(&iter, group->storage);
        while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
	    if (rdev_is_ok(rdev))
                os_atomic_inc(&vrt_req->barrier->remaining_disks);
        storage_rdev_iterator_end(&iter);

        vrt_req->barrier->vrt_req = vrt_req;
        os_atomic_inc(&vrt_req->remaining);
    }

    vrt_req->replay_date = 0;
}

/**
 * Resets the bio to a sane state. This allows to restore the buffer
 * heads or bios to a sane state, before re-using them again for the
 * next round of IOs.
 *
 * @param[in] vrt_req The request
 */
static void vrt_reset_buffer_heads(struct vrt_request *vrt_req)
{
    struct vrt_io_op *io;

    for (io = vrt_req->io_list ; io != NULL ; io = io->next)
    {
	memset(io->bio, 0, sizeof(blockdevice_io_t));
    }
}

/**
 * Finalize the request described in the given request header by
 * freeing all used memory and signaling the kernel using the
 * b_end_io() function of the original bio.
 *
 * @param[in] vrt_req The request header describing the request to be
 * terminated
 *
 * @param[in] failed  true if the IO failed false otherwise
 */
static void vrt_end_request(struct vrt_request *vrt_req, bool failed)
{
    blockdevice_io_t *bio;
    struct vrt_group *group;
    struct vrt_volume *volume;

    bio = vrt_req->ref_bio;

    group = VRT_REQ_GET_GROUP(vrt_req);
    volume = vrt_req->ref_vol;

    os_atomic_dec(& group->initialized_request_count);
    EXA_ASSERT (os_atomic_read (& group->initialized_request_count) >= 0);

    /* The barrier is completed. Wake up all tasks that were waiting for
     * the barrier completion.
     */
    if (bio == volume->barrier_bio)
    {
	volume->barrier_bio = NULL;
	wake_up_all(&volume->barrier_post_req_wq);
    }

    if (os_atomic_read (& group->initialized_request_count) == 0)
	wake_up_all (& group->recover_wq);

    /* If the request count reaches 0, then it's time to wake up the
     * process (if any) that want to submit its barrier.
     */
    if (os_atomic_dec_and_test(&volume->inprogress_request_count))
	wake_up_all(&volume->barrier_req_wq);

    EXA_ASSERT (os_atomic_read (& volume->inprogress_request_count) >= 0);

    wake_up_all (& volume->cmd_wq);

    VRT_PERF_END_REQUEST(vrt_req);
    vrt_stat_request_done(vrt_req, failed);

    vrt_free_structs(vrt_req);
    vrt_mempool_object_free (vrt_req_pool, vrt_req);

    /* Tells the kernel that the request is finished */
    blockdevice_end_io(bio, failed ? -EIO : 0);
}

static void
vrt_request_cancel (struct vrt_request *vrt_req)
{
    struct vrt_group *group;
    struct vrt_io_op *io;

    group = VRT_REQ_GET_GROUP(vrt_req);
    EXA_ASSERT(group);

    group->layout->cancel_req(vrt_req);

    /* reinitialize the request */
    for (io = vrt_req->io_list ; io; io = io->next)
	io->state = IO_DONT_PROCESS;

    if (vrt_req->barrier != NULL)
	vrt_req->barrier->state = BARRIER_DONT_PROCESS;

    os_atomic_dec(& group->initialized_request_count);
    EXA_ASSERT (os_atomic_read (& group->initialized_request_count) >= 0);

    if (os_atomic_read (& group->initialized_request_count) == 0)
	wake_up_all (& group->recover_wq);

    EXA_ASSERT(vrt_check_for_build_io_for_req(vrt_req));
    vrt_req_list_add(& vrt_suspended_req_list, vrt_req);
}

/**
 * Process suspended requests of the global vrt_suspended_req_list
 * list. This function is called by the virtualizer thread in order to
 * resume requests if theses requests can be resumed.
 */
static void vrt_process_suspended_requests(struct vrt_request *list)
{
    struct vrt_request *cur, *next;

    cur = list;

    while (cur != NULL)
    {
	struct vrt_group *group;

	/* Save the list next element pointer, and mark the current
	   element as being outside of the list. This is not
	   mandatory, but vrt_next_step_io() checks that the element
	   to be inserted is not already in the list. This is
	   safer. */
	next = cur->next;
	cur->next = NULL;

	group = VRT_REQ_GET_GROUP(cur);

	os_thread_rwlock_rdlock(&group->suspend_lock);
	if (group->suspended)
	    vrt_req_list_add(& vrt_suspended_req_list, cur);
	else
	{
	    os_atomic_inc (& group->initialized_request_count);
	    vrt_req_list_add(& vrt_tobuild_req_list, cur);
	}

        os_thread_rwlock_unlock(&group->suspend_lock);

	cur = next;
    }
}

/**
 * Process tobuild requests of the global vrt_tobuild_req_list
 * list. This function is called by the virtualizer thread in order to
 * process the next step of all tobuild requests.
 */
static void vrt_process_tobuild_requests(struct vrt_request *list)
{
    vrt_req_status_t status;
    struct vrt_request *cur, *next;

    cur = list;

    while (cur != NULL)
    {
	struct vrt_group *group;

	/* Save the list next element pointer, and mark the current
	   element as being outside of the list. This is not
	   mandatory, but vrt_next_step_io() checks that the element
	   to be inserted is not already in the list. This is
	   safer. */
	next = cur->next;
	cur->next = NULL;

	group = VRT_REQ_GET_GROUP(cur);

	os_thread_rwlock_rdlock(&group->suspend_lock);
	if (group->suspended)
	{
	    os_thread_rwlock_unlock(&group->suspend_lock);
	    vrt_request_cancel(cur);
	    cur = next;
	    continue;
	}
        os_thread_rwlock_rdlock(&group->status_lock);

	if (VRT_REQ_GET_GROUP(cur)->status == EXA_GROUP_OFFLINE
	    && cur->iotype != VRT_IO_TYPE_READ)
	    status = VRT_REQ_FAILED;
        else
	{
	    /* Build the request */
	    EXA_ASSERT(vrt_check_for_build_io_for_req(cur));
	    status = group->layout->build_io_for_req(cur);
	}

        os_thread_rwlock_unlock(&group->suspend_lock);
        os_thread_rwlock_unlock(&group->status_lock);

	switch(status)
	{
        case VRT_REQ_SUCCESS:
	    vrt_end_request(cur, false);
	    break;

        case VRT_REQ_FAILED:
	    vrt_end_request(cur, true);
	    break;

        case VRT_REQ_UNCOMPLETED:
	    vrt_req_list_add(& vrt_pending_req_list, cur);
	    /* No need to wake up ourselves */
	    break;

        case VRT_REQ_POSTPONED:
	    break;

        default:
	    EXA_ASSERT_VERBOSE (FALSE, "Unexpected status %d\n", status);
	    break;
	}

	cur = next;
    }
}

/**
 * Process pending requests of the global vrt_pending_req_list
 * list. This function is called by the virtualizer thread in order to
 * process the next step of all pending requests.
 */
static void vrt_process_pending_requests(struct vrt_request *list)
{
    struct vrt_request *cur, *next;

    cur = list;

    while (cur != NULL)
    {
	struct vrt_group *group;

	/* Save the list next element pointer, and mark the current
	   element as being outside of the list. This is not
	   mandatory, but vrt_next_step_io() checks that the element
	   to be inserted is not already in the list. This is
	   safer. */
	next = cur->next;
	cur->next = NULL;

	group = VRT_REQ_GET_GROUP(cur);

	os_thread_rwlock_rdlock(&group->suspend_lock);
	/* Group is currently suspended, re-initialize the request and
	   re-insert it into the list for future processing (when the
	   group will be activated again) */
	if (group->suspended)
        {
	    os_thread_rwlock_unlock(&group->suspend_lock);
	    vrt_request_cancel(cur);

	    cur = next;
	    continue;
        }

	EXA_ASSERT (! group->suspended);

	/* If this request must be performed after a specified date */
	if (cur->replay_date != 0 &&
	    cur->replay_date > os_gettimeofday_msec())
	{
	    /* We are too early to perform this request. Add it to the pending
	     * list to perform it later */
	    os_thread_rwlock_unlock(&group->suspend_lock);
	    vrt_req_list_add(& vrt_pending_req_list, cur);

	    /* we do not need to wake up the thread: the timer will do it */

	    cur = next;
	    continue;
	}

	os_thread_rwlock_unlock(&group->suspend_lock);

	/* Group is working, run the I/Os */
	vrt_reset_buffer_heads(cur);

	vrt_request_prepare(cur);
	vrt_request_perform(cur);

	cur = next;
    }
}

/**
 * Wake up the virtualizer thread
 */
void vrt_thread_wakeup (void)
{
    os_atomic_set(&vrt_thread.event, 1);
    wake_up(& vrt_thread.wq);
}

/**
 * The thread of the VRT engine. Its role is to run all the pending
 * IOs of the request headers registered in the global vrt_vrt_req_list.
 *
 * We cannot directly ask the kernel to perform the I/Os in
 * vrt_next_step_io() because this function runs in an
 * interrupt-context, and while the io_request_lock is
 * taken. generic_make_request() also wants this lock, and do not
 * support to be called from an interrupt-context.
 *
 * We cannot use a tasklet, because a tasklet runs also in an
 * interrupt context.
 *
 * This is why we use a thread (which runs normally, like any other
 * process) to ask the kernel to perform new I/Os.
 *
 * @see vrt_next_step_io for more information on the use of this
 * thread
 *
 * @param[in] data Unused.
 */
static void vrt_thread_engine(void *data)
{
    exalog_as(EXAMSG_VRT_ID);

    while (1)
    {
	struct vrt_request *vrt_req_list;

	wait_event_or_timeout(vrt_thread.wq, os_atomic_read(&vrt_thread.event) != 0, VRT_REPLAY_REQUEST_DELAY_MSEC);
	os_atomic_set(&vrt_thread.event, 0);

	if (vrt_thread.ask_terminate)
	    break;

	/* resume suspended requests if the group is resumed. If the group's
         * still suspended, they'll go straight back where they came from.
         * FIXME Ain't that stupid? Shouldn't we check if the group's
         * resumed here?
         */
	vrt_req_list = vrt_req_list_take(& vrt_suspended_req_list);
	vrt_process_suspended_requests(vrt_req_list);

	/* build requests with build_io_for_req. They'll come out of here
         * either done, uncompleted or postponed. If uncompleted,
         * they'll move to the the pending_req list.
         */
	vrt_req_list = vrt_req_list_take(& vrt_tobuild_req_list);
	vrt_process_tobuild_requests(vrt_req_list);

	/* perform IO of each requests */
	vrt_req_list = vrt_req_list_take(& vrt_pending_req_list);
	vrt_process_pending_requests(vrt_req_list);
    }
}

/** Wake up a request previously postponed by VRT_REQ_POSTPONED
 *
 * Warning: layouts need to call vrt_thread_wakeup() after vrt_wakeup_request()
 * */
void vrt_wakeup_request (struct vrt_request *vrt_req)
{
    EXA_ASSERT(vrt_req);

    /* add the request in the tobuild list */
    vrt_req_list_add(& vrt_tobuild_req_list, vrt_req);
}

/**
 * This function replaces the default __make_request of the Exanodes
 * virtual block device in the kernel. It gets called by the block
 * subsystem of the kernel everytime a request is issued on the
 * virtual block device.
 *
 * This code and the layout code should never use the bio->b_data
 * field, because it can be NULL on 32 bits machines with HIGHMEM
 * activated. We don't touch it, and let the lower layers of the block
 * subsystem do the correct create_bounce() call when needed.
 *
 * @param[in] bdev  The block device
 *
 * @param[in] bio The bio that describe the request
 */
void vrt_make_request(void *private_data, blockdevice_io_t *bio)
{
    unsigned int io_count;
    bool sync_afterward;
    struct vrt_volume *volume = private_data;
    struct vrt_group *group;
    struct vrt_request *vrt_req;

    vrt_io_type_t io_type = VRT_IO_TYPE_NONE;

    /* Return an error if a barrier is requested and support for
     * barriers is disabled.
     */
    if (bio->flush_cache && !vrt_barriers_enabled())
    {
	blockdevice_end_io(bio, -EOPNOTSUPP);
	return;
    }

    /* Handle iotype */
    EXA_ASSERT(BLOCKDEVICE_IO_TYPE_IS_VALID(bio->type));
    switch (bio->type)
    {
    case BLOCKDEVICE_IO_READ:
	io_type = VRT_IO_TYPE_READ;
        break;
    case BLOCKDEVICE_IO_WRITE:
        if (bio->flush_cache)
            io_type = VRT_IO_TYPE_WRITE_BARRIER;
        else
            io_type = VRT_IO_TYPE_WRITE;
        break;
    }

    if (volume == NULL)
    {
	exalog_info("VRT IO error, volume NULL");
	blockdevice_end_io(bio, -EIO);
	return;
    }

    VRT_PERF_MAKE_REQUEST(io_type, bio);

    EXA_ASSERT (volume->group != NULL);
    group = volume->group;

    os_atomic_inc(&volume->inprogress_request_count);

    os_thread_rwlock_rdlock(&group->suspend_lock);
    while (group->suspended)
    {
	os_thread_rwlock_unlock(&group->suspend_lock);
	wait_event(group->suspended_req_wq, (group->suspended == FALSE));
	os_thread_rwlock_rdlock(&group->suspend_lock);
    }
    os_thread_rwlock_unlock(&group->suspend_lock);

    while (volume->frozen)
    {
	if (os_atomic_dec_and_test(&volume->inprogress_request_count))
	    wake_up_all(&volume->barrier_req_wq);
	wait_event(volume->frozen_req_wq, (volume->frozen == FALSE));
	os_atomic_inc(&volume->inprogress_request_count);
    }

    /* A barrier is requested. */
    if (bio->flush_cache)
    {
        do
        {
            /* we loop here, because even if volume->barrier_bio == NULL another
             * process can began to process its barrier before this one */
            wait_event(volume->barrier_post_req_wq, (volume->barrier_bio == NULL));
            os_thread_mutex_lock(&volume->barrier_lock);
            if (volume->barrier_bio == NULL)
                volume->barrier_bio = bio;
            os_thread_mutex_unlock(&volume->barrier_lock);
        }
        while (volume->barrier_bio != bio);

	/* Wait until all previous requests are completed. */
	if (! os_atomic_dec_and_test(&volume->inprogress_request_count))
	    wait_event(volume->barrier_req_wq,
		       os_atomic_read(&volume->inprogress_request_count) == 0);
	os_atomic_inc(&volume->inprogress_request_count);
    }

    /* If a barrier is in-progress, and if we are not issuing the bio
     * flagged as barrier, we sleep until the end of the barrier.
     */
    else if (volume->barrier_bio != NULL)
    {
	if (os_atomic_dec_and_test(&volume->inprogress_request_count))
	    wake_up_all(&volume->barrier_req_wq);
	wait_event(volume->barrier_post_req_wq, (volume->barrier_bio == NULL));
	os_atomic_inc(&volume->inprogress_request_count);
    }

    vrt_req = vrt_mempool_object_alloc (vrt_req_pool);
    EXA_ASSERT (vrt_req);

    /* Init request header.  We cannot memset vrt_req to 0 because in case of a
     * request of type normal_sub, the field vrt_req->io_list is prefilled by the
     * parent request. */
    memset(vrt_req->private_data, 0, sizeof(vrt_req->private_data));
    vrt_req->ref_bio        = bio;
    vrt_req->iotype        = io_type;
    os_atomic_set(& vrt_req->remaining, 0);
    vrt_req->next          = NULL;
#ifdef WITH_PERF
    vrt_req->nb_io_ops = 0;
#endif

    vrt_req->ref_vol = volume;

    /* Update statistics */
    vrt_stat_request_begin(vrt_req);

    /* Ask the layout for its needs in terms of number of IO structures,
       and allocates memory accordingly */
    group->layout->declare_io_needs(vrt_req, &io_count, &sync_afterward);
    vrt_alloc_structs(vrt_req, io_count, sync_afterward);

    group->layout->init_req (vrt_req);

    os_atomic_inc (& group->initialized_request_count);

    vrt_req_list_add(&vrt_tobuild_req_list, vrt_req);
    vrt_thread_wakeup();
}

static int vrt_thread_start(void)
{
    init_waitqueue_head(&vrt_thread.wq);
    os_atomic_set(&vrt_thread.event, 1);

    vrt_thread.ask_terminate = false;

    if (!exathread_create_named(&vrt_thread.tid, VRT_THREAD_STACK_SIZE,
                                vrt_thread_engine, NULL, "vrt_thread"))
    {
        clean_waitqueue_head(&vrt_thread.wq);
        exalog_error("Cannot spawn virtualizer thread.");
        return -EXA_ERR_THREAD_CREATE;
    }

    return EXA_SUCCESS;
}

/**
 * Initialize the VRT engine (allocate memory, initialize locks
 * and semaphores, create the vrt_thread() kernel thread)
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int vrt_engine_init (int max_requests)
{
    int ret;

    vrt_req_pool = vrt_mempool_create (sizeof(struct vrt_request),
				       max_requests);
    if (vrt_req_pool == NULL)
    {
	exalog_error("Cannot create object pool for vrt_request (%d objs of size %" PRIzu ")",
                     max_requests, sizeof (struct vrt_request));
	ret = -ENOMEM;
	goto vrt_request_pool_error;
    }

    io_pool = vrt_mempool_create (sizeof(struct vrt_io_op), max_requests);
    if (io_pool == NULL)
    {
	exalog_error("Cannot create object pool for vrt_io_op (%d objs of size %" PRIzu ")",
                     max_requests, sizeof (struct vrt_io_op));
	ret = -ENOMEM;
	goto vrt_io_op_pool_error;
    }

    bio_pool = vrt_mempool_create (sizeof(blockdevice_io_t), max_requests);
    if (bio_pool == NULL)
    {
	exalog_error("Cannot create object pool for bio (%d objs of size %" PRIzu ")",
                     max_requests, sizeof (blockdevice_io_t));
	ret = -ENOMEM;
	goto vrt_bio_pool_error;
    }

    barrier_pool = vrt_mempool_create(sizeof(struct vrt_barrier_op), max_requests);
    if (barrier_pool == NULL)
    {
	exalog_error("Cannot create barrier pool (%d objs of size %" PRIzu ")",
                     max_requests, sizeof(struct vrt_barrier_op));
	ret = -ENOMEM;
	goto vrt_barrier_pool_error;
    }

   nbd_init_root(NB_BIO_PER_POOL, sizeof(blockdevice_io_t), &pool_of_bio);

    vrt_req_list_init(&vrt_pending_req_list);
    vrt_req_list_init(&vrt_tobuild_req_list);
    vrt_req_list_init(&vrt_suspended_req_list);

    ret = vrt_thread_start();
    if (ret == EXA_SUCCESS)
        return EXA_SUCCESS;

    vrt_req_list_clean(&vrt_pending_req_list);
    vrt_req_list_clean(&vrt_tobuild_req_list);
    vrt_req_list_clean(&vrt_suspended_req_list);
    vrt_mempool_destroy(barrier_pool);
    nbd_close_root(&pool_of_bio);
vrt_barrier_pool_error:
    vrt_mempool_destroy(bio_pool);
vrt_bio_pool_error:
    vrt_mempool_destroy(io_pool);
vrt_io_op_pool_error:
    vrt_mempool_destroy(vrt_req_pool);
vrt_request_pool_error:
    return ret;
}


static void vrt_thread_stop(void)
{
    vrt_thread.ask_terminate = true;
    vrt_thread_wakeup();

    os_thread_join(vrt_thread.tid);

    clean_waitqueue_head(&vrt_thread.wq);
}

/**
 * Cleanup the VRT engine
 */
void vrt_engine_cleanup(void)
{
    vrt_thread_stop();

    vrt_req_list_clean(&vrt_pending_req_list);
    vrt_req_list_clean(&vrt_tobuild_req_list);
    vrt_req_list_clean(&vrt_suspended_req_list);

    vrt_mempool_destroy(bio_pool);
    vrt_mempool_destroy(barrier_pool);
    vrt_mempool_destroy(io_pool);
    vrt_mempool_destroy(vrt_req_pool);
    nbd_close_root(&pool_of_bio);
}

