/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "target/linux_bd_target/module/bd_user_bd.h"
#include "target/linux_bd_target/module/bd_list.h"

#include "target/linux_bd_target/include/bd_user.h"

#include "target/linux_bd_target/module/bd_log.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_assert.h"

#include "os/include/os_assert.h"

#include <linux/bio.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched/signal.h>

#define EXA_BD_DEVICE_NAME "exa_bd" /**< name of exa_bd in /proc/devices */

#ifndef blk_queue_max_sectors
# define blk_queue_max_sectors blk_queue_max_hw_sectors
#endif

/* this request is a barrier : nbd will wait all previous
 * request completion, flush the disk cache,
 * make the write request
 * flush disk cache and tell this barrier is done */
/* n _bd internal value*/
#define BD_INFO_INTERNAL_BARRIER 8 /* internal in nbd to proceed sync and barrier */

#define EXA_BD_READAHEAD 8192

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
#  define BIO_OFFSET(b) (b)->bi_sector
#else
#  define BIO_OFFSET(b) (b)->bi_iter.bi_sector
#endif

#define BIO_SIZE(b) BYTES_TO_SECTORS((b)->bi_iter.bi_size)
#define BIO_NEXT(b) (b)->bi_next
#define BIO_VAR int __idx = 0

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
#define bio_disk(bio) (bio)->bi_bdev->bd_disk
#else
#define bio_disk(bio) (bio)->bi_disk
#endif

#define BIO_BD_MINOR(bio)  ((struct bd_minor *) bio_disk(bio)->\
                            private_data)

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)
#define bio_op(bio) ((bio)->bi_rw)
#endif

#define BIO_NEXT_MEM(bio, page, offset, size) \
    do \
    { \
        EXA_ASSERT_VERBOSE(bio->bi_io_vec != NULL, "bi_sector=%lu bi-size=%u bi_op=%lu bi_flags=%u bi_vcnt=%d", \
                           BIO_OFFSET(bio), bio->bi_iter.bi_size, (unsigned long )bio_op(bio), bio->bi_flags, bio->bi_vcnt);\
        page = bio->bi_io_vec[__idx].bv_page; \
        offset = bio->bi_io_vec[__idx].bv_offset; \
        size = bio->bi_io_vec[__idx].bv_len; \
        __idx++; \
        if (__idx >= bio->bi_vcnt) \
        { \
            bio = BIO_NEXT(bio); \
            if (bio == NULL) { \
                break;} \
            __idx = 0; \
        } \
    } while (0);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)
#define bio_barrier(bio) ((bio)->bi_rw == WRITE_FLUSH || (bio)->bi_rw == WRITE_FUA || (bio)->bi_rw == WRITE_FLUSH_FUA)
#else
#define bio_barrier(bio) ((bio)->bi_opf & REQ_PREFLUSH)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
# define bd_kmap_atomic(page, type) \
    kmap_atomic(page)
# define bd_kunmap_atomic(kvaddr, type) \
    kunmap_atomic(kvaddr)
#else
# define bd_kmap_atomic(page, type) \
    kmap_atomic(page, type)
# define bd_kunmap_atomic(kvaddr, type) \
    kunmap_atomic(kvaddr, type)
#endif

const char *bd_minor_name(const struct bd_minor *bd_minor)
{
    OS_ASSERT(bd_minor != NULL);
    OS_ASSERT(bd_minor->bd_gen_disk != NULL);

    return bd_minor->bd_gen_disk->disk_name;
}

/**
 * try to merge this bio with last request in the list
 * @param bio bio to merger
 * @param rw
 * @param info
 * @param list list of the minor
 *
 * @return true  merged
 *         false not merged
 */
static bool bd_concat_bio(struct bio *bio, int rw, int info, int max_req_size,
                         struct bd_list *list)
{
    struct bd_request *req;
    struct bio *bio_temp;
    int bio_sect;

    /* Take a handle on req */
    req = bd_get_and_lock_last_posted(list);
    if (req == NULL)
        return false;

    /* Check if the bio we are trying to concat is of the same kind of IO
     * (read, write, barrier etc.) than the request. */
    if (req->info != info || req->rw != rw)
    {
        bd_unlock_last_posted(list);
        return false;
    }

    /* Compute the size of the request and find out the last bio of req */
    bio_sect = 0;
    bio_temp = req->first_bio;
    while (BIO_NEXT(bio_temp) != NULL)
    {
        bio_sect += BIO_SIZE(bio_temp);
        bio_temp = BIO_NEXT(bio_temp);
    }
    bio_sect += BIO_SIZE(bio_temp);

    /* Check that request size + bio_size is not be biger than the maximum
     * request size allowed */
    if (bio_sect + BIO_SIZE(bio) > BYTES_TO_SECTORS(max_req_size))
    {
        bd_unlock_last_posted(list);
        return false;
    }

    /* Try to put bio in front: check if the reqest data are right after the
     * current bio */
    if (BIO_OFFSET(bio) + BIO_SIZE(bio) == BIO_OFFSET(req->first_bio))
    {
        BIO_NEXT(bio) = req->first_bio;
        req->first_bio = bio;
        bd_unlock_last_posted(list);
        return true;
    }

    /* Try to put bio in tail: check if the bio data are right after the
     * request one */
    if (BIO_SIZE(bio_temp) + BIO_OFFSET(bio_temp) == BIO_OFFSET(bio))
    {
        BIO_NEXT(bio_temp) = bio;
        bd_unlock_last_posted(list);
        return true;
    }

    bd_unlock_last_posted(list);

    /* The bio was not contiguous to the request, it cannot be concat */
    return false;
}


static void bd_end_one_req(struct bd_request *req, int err);

/* this file contain all function directly related to block driver
 * In this file bd_request (any process) are called asynchronously
 * all other function are called synchronously */

static struct bd_minor *minor_get_next(struct bd_minor *minor)
{
    if (minor == NULL)
        return NULL;

    if (minor->bd_next == NULL)
        return minor->bd_session->bd_minor;

    return minor->bd_next;
}

static struct bd_request *minor_get_req(struct bd_minor *minor)
{
    struct bd_request *req;

    if (minor->dead)
        return NULL;

    if (minor->bd_gen_disk->queue == NULL)
        return NULL;

    if (minor->need_sync == 1)
    {
        /* There are outstanding IOs, we need to wait their completion before
         * processing any new request on this minor */
        if (minor->current_run > 0)
            return NULL;

        /* There are no outstanding IOs => the sync was completed, we can reset
         * the flag */
        minor->need_sync = 0;
    }

    do {
        req = bd_list_remove(&minor->bd_list, LISTNOWAIT);

        /* try to find a non barrier request */
        if (req == NULL)
            return NULL;

        if (req->info == BD_INFO_INTERNAL_BARRIER)
        {
            bd_list_post(&minor->bd_list.root->free, req);

            if (minor->current_run > 0)
            {
                minor->need_sync = 1;
                /* stop processing this minor as a sync is awaited before resuming */
                return NULL;
            }
        }
    } while (req->info == BD_INFO_INTERNAL_BARRIER);

    return req;
}

/**
 * Searching for onr next request in all queue with a simple round robin
 * fairness algorithm
 * @param session target session
 * @return a Request pointer or NULL if all queue was full
 */
static struct bd_request *bd_next_queue(struct bd_session *session)
{
    struct bd_minor *minor;
    struct bd_request *req = NULL;

    if (session->bd_minor == NULL)
        return NULL;

    /* go thru all minors, starting after the last one processed */
    for (minor = minor_get_next(session->pending_minor);
         minor != NULL && minor != session->pending_minor;
         minor = minor_get_next(session->pending_minor))
    {
        req = minor_get_req(minor);
        if (req != NULL) /* Take the first available request */
            break;
    }

    session->pending_minor = minor;
    return req;
}


/*****************************************************************************
*  Function used to copy from/to (unaligned)buffer                          *
*****************************************************************************/
/* CheckUnaligned return zero if all buffer reside in complete buffer, return
 * non zero else in this case the return is the number of needed page
 */

static void check_size(struct bd_kernel_queue *Q)
{
    struct bio *bio;

    Q->bd_size_in_sector = 0;

    for (bio = Q->bd_req->first_bio; bio != NULL; bio = BIO_NEXT(bio))
        Q->bd_size_in_sector += BIO_SIZE(bio);
}


static void preprocessing_for_write(struct bd_kernel_queue *Q)
{
    unsigned long nb = 0;
    unsigned long flags;
    char *dest_addr;
    char *src_addr;
    struct page **page_tab =
        &(Q->bd_session->bd_unaligned_buf[bd_queue_num(Q) *
                                          (Q->bd_session->bd_buffer_size >>
                                           PAGE_SHIFT)]);
    unsigned long dest_addr_dec;
    struct page *page;
    int size;
    int offset;
    struct bio *bio = Q->bd_req->first_bio;

    BIO_VAR;

    local_irq_save(flags);
    do
    {
        BIO_NEXT_MEM(bio, page, offset, size);
        src_addr =  bd_kmap_atomic(page, KM_USER0);
        dest_addr = bd_kmap_atomic(*(page_tab + (nb >> PAGE_SHIFT)), KM_USER1);
        dest_addr_dec = (nb & (PAGE_SIZE - 1));
	/* cross page boundaries in the bd buffer ? */
        if (dest_addr_dec + size > PAGE_SIZE)
        {
            memcpy(dest_addr + dest_addr_dec,
                   src_addr + offset,
                   PAGE_SIZE - dest_addr_dec);
            bd_kunmap_atomic(dest_addr, KM_USER1);
	    /* next page of the bd buffer */
            dest_addr = bd_kmap_atomic(*(page_tab + (nb >> PAGE_SHIFT) + 1),
                                    KM_USER1);
            memcpy(dest_addr,
                   src_addr + offset + PAGE_SIZE - dest_addr_dec,
                   size - (PAGE_SIZE - dest_addr_dec));
        }
        else
            memcpy(dest_addr + dest_addr_dec, src_addr + offset, size);

        bd_kunmap_atomic(dest_addr, KM_USER1);
        bd_kunmap_atomic(src_addr, KM_USER0);
        nb = nb + size;
    } while (bio != NULL);
    local_irq_restore(flags);
}


static void postprocessing_for_read(struct bd_kernel_queue *Q)
{
    unsigned long nb = 0;
    struct page **page_tab =
        &(Q->bd_session->bd_unaligned_buf[bd_queue_num(Q) *
                                          (Q->bd_session->bd_buffer_size >>
                                           PAGE_SHIFT)]);
    unsigned long flags;
    char *source_addr;
    char *dest_addr;
    unsigned long source_addr_dec;
    struct page *page;
    int size;
    int offset;
    struct bio *bio = Q->bd_req->first_bio;

    BIO_VAR;

    local_irq_save(flags);
    do
    {
        BIO_NEXT_MEM(bio, page, offset, size);
        dest_addr = bd_kmap_atomic(page, KM_USER0);
        source_addr = bd_kmap_atomic(*(page_tab + (nb >> PAGE_SHIFT)), KM_USER1);
        source_addr_dec = (nb & (PAGE_SIZE - 1));
	/* cross page boundaries in the bd buffer ? */
        if (source_addr_dec + size > PAGE_SIZE)
        {
            memcpy(dest_addr + offset,
                   source_addr + source_addr_dec,
                   PAGE_SIZE - source_addr_dec);
            bd_kunmap_atomic(source_addr, KM_USER1);
	    /* next page of the bd buffer */
            source_addr = bd_kmap_atomic(*(page_tab + (nb >> PAGE_SHIFT) + 1),
                                      KM_USER1);
            memcpy(dest_addr  + offset + PAGE_SIZE - source_addr_dec,
                   source_addr,
                   size - (PAGE_SIZE - source_addr_dec));
        }
        else
            memcpy(dest_addr  + offset, source_addr + source_addr_dec, size);

        bd_kunmap_atomic(source_addr, KM_USER1);
        bd_kunmap_atomic(dest_addr, KM_USER0);
        nb = nb + size;
    } while (bio != NULL);
    local_irq_restore(flags);
}


/*-----------------------------
 * general block functions
 * ------------------------------*/

struct block_device_operations bd_blk_fops;

/** BdPrepareRequest ; map buffer in user mode or eventually allocate unaligned
 * buffer, copy data in it, and map it in user mode
 * it also filled the Q field BdOp, BdMajor, BdMinor, BdBlkSize, BdBlkNum
 * WARNING : it can only be called by bd_post_new_rq() called itself by
 * bd_flush_q()
 * @param[inout] it just need Q->BdReq been filled, it set the other parameter
 * excluding BdNext and BdPrev
 * @return  -1 == error means that you must try later when more resources are
 *                free
 *          0 == success */
int bd_prepare_request(struct bd_kernel_queue *Q)
{
    Q->bd_op = Q->bd_req->rw;
    Q->bd_minor = Q->bd_req->bd_minor->minor;
    check_size(Q);

    Q->bd_blk_num = BIO_OFFSET(Q->bd_req->first_bio);
    if (Q->bd_req->rw == WRITE && Q->bd_size_in_sector != 0)
        preprocessing_for_write(Q);

    return 0;
}

static void bd_end_io(struct bio *bio, int err)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,13,0)
     bio->bi_error = err;
#else
     bio->bi_status = err;
#endif
     bio_endio(bio);
#else
     bio_endio(bio, err);
#endif
}

/** Helper function used to share code between BdFlushQ and BdEndQ and
 * BdEndRequest
 * @param req the request to end
 * @param err error or success
 */
static void bd_end_one_req(struct bd_request *req, int err)
{
    struct bio *bio;

    if (req == NULL)
    {
        bd_log_error("BD : Severe errror : BdEndOneReq(NULL)= %d\n", err);
        return;
    }

    bio = req->first_bio;
    while (bio != NULL)
    {
        struct bio *next_bio = BIO_NEXT(bio);
        bd_end_io(bio, err);
        bio = next_bio;
    }
    bd_list_post(&req->bd_minor->bd_list.root->free, req);
}

/**
 * Terminate a request, transfert data if it's a read, release page if it's
 * bio directly alligned
 * @param Q the bd_kernel_queue of the request
 * @param io_lock if the caller have io_request_lock
 * @param err error or success
 */
void bd_end_request(struct bd_kernel_queue *Q, int err)
{
    struct bd_minor *bd_minor = Q->bd_req->bd_minor;

    bd_minor->current_run--;

    /* If wa have used some buffer, they must have be recopied to
     *buffer_head/bio
     */
    if (Q->bd_req->rw == READ)
        postprocessing_for_read(Q);     /* recopy buffer if read */

    bd_end_one_req(Q->bd_req, err);
}


/**
 * Ending all request of a queue
 * @param Q the kernel queue to flush
 * @param io_lock if the caller have io_request_lock
 * @param err error or succes for all request of this queue
 * @param session target session associated with queue device
 * @param minor minor number associated with queue device
 */
void bd_end_q(struct bd_minor *bd_minor, int err)
{
    struct bd_request *req = bd_minor->bd_session->pending_req;

    if (req != NULL && req->bd_minor->minor == bd_minor->minor)
    {
        bd_end_one_req(req, -EIO);
        bd_minor->bd_session->pending_req = NULL;
    }

    while ((req = bd_list_remove(&bd_minor->bd_list, LISTNOWAIT)) != NULL)
        bd_end_one_req(req, err);
}


/**
 * Try flush maximum request from all queue of this session, if Err != BD
 * @param session target session
 * @param err 0 try to add as many as we can request to bd_kernel_queue,
 * -EIO :all request must be remove and end with error
 * */
void bd_flush_q(struct bd_session *session)
{
    struct bd_request *req;

    do
    {
        /* take a new request only if there is no pending one */
        if (session->pending_req != NULL)
        {
            req = session->pending_req;
            session->pending_req = NULL;
        } else {
            req = bd_next_queue(session);
        }

        if (req == NULL)
            return; /* nothing to do */

        if (bd_post_new_rq(session, req) != 0)
        {
            session->pending_req = req;
            return; /* This Req cannot be added, so probably no more Req
                     * can be added now, but we keep this request that
                     * cannot be added */
        }
        /* the request was posted, update the outstanding counter */
        req->bd_minor->current_run++;
    } while (1);
}


/**
 * submit a bio with info
 * if it's barrier with data, it's separated in three request :
 * - wait for the flush of all request BD_INFO_INTERNAL_BARRIER
 *   (if BD_INFO_BARRIER)
 * - our request BD_INFO_BARRIER was keep (for all request)
 * - wait for the flush of all request BD_INFO_INTERNAL_BARRIER
 *   (if BD_INFO_BARRIER)
 *
 *   Flags
 *   BD_INFO_BARRIER no real meaning for exa_bd, sent to serverd
 *
 *   WARNING : if bd_list_remove (&bd_minor->bd_list.root->free, LISTWAIT)
 *             return NULL
 *             the list is closed and so are in ending phase, so if the
 *             BARRIER was not correctly enforced, its not a problem !
 */
static void bd_submit_bio_with_info(struct bio *bio, int rw)
{
    struct bd_minor *bd_minor = BIO_BD_MINOR(bio);
    struct bd_session *session = bd_minor->bd_session;
    struct bd_request *req = NULL, *reqpre = NULL, *reqpost = NULL;
    bool submited = false, needevent = false;
    int info = 0;
    int cpu;

    do
    {
        OS_ASSERT_VERBOSE(!bio_barrier(bio) || rw == 1, "%u %d",
                          (unsigned)bio_barrier(bio), rw);

        if (bio_barrier(bio) && session->bd_barrier_enable == 1)
            info = BD_INFO_BARRIER;

	cpu = part_stat_lock();
        part_stat_inc(cpu, &bd_minor->bd_gen_disk->part0, ios[rw]);
        part_stat_add(cpu, &bd_minor->bd_gen_disk->part0, sectors[rw],
		      bio_sectors(bio));
	part_stat_unlock();

        if (info != BD_INFO_BARRIER)
        {
            if (bd_concat_bio(bio, rw, info,
                              bd_minor->bd_session->bd_buffer_size,
                              &bd_minor->bd_list))
                return;
        }
        else
        {
            reqpre = bd_list_remove(&bd_minor->bd_list.root->free, LISTWAIT);
            reqpost = bd_list_remove(&bd_minor->bd_list.root->free, LISTWAIT);
            if (reqpre == NULL || reqpost == NULL)
                break;

            reqpre->first_bio = NULL;
            reqpre->info = BD_INFO_INTERNAL_BARRIER;
            reqpre->bd_minor = bd_minor;
            reqpost->first_bio = NULL;
            reqpost->info = BD_INFO_INTERNAL_BARRIER;
            reqpost->bd_minor = bd_minor;
        }

        req = bd_list_remove(&bd_minor->bd_list.root->free, LISTWAIT);

        if (req == NULL)
            break;

        req->first_bio = bio;
        req->bd_minor = bd_minor;
        req->rw = rw;
        req->info = info;

        if (reqpre != NULL)
        {
            if (bd_list_post(&bd_minor->bd_list, reqpre) < 0)
                break;

            needevent = true;
        }
        reqpre = NULL;

        if (bd_list_post(&bd_minor->bd_list, req) < 0)
            break;

        req = NULL;
        submited = true;
        needevent = true;

        if (reqpost != NULL)
            if (bd_list_post(&bd_minor->bd_list, reqpost) < 0)
                break;

        reqpost = NULL;
    } while (0);

    if (reqpre != NULL)
        bd_list_post(&bd_minor->bd_list.root->free, reqpre);

    if (reqpost != NULL)
        bd_list_post(&bd_minor->bd_list.root->free, reqpost);

    if (req != NULL)
        bd_list_post(&bd_minor->bd_list.root->free, req);

    if (!submited)
        bd_end_io(bio, -EIO);

    /* Don't explicitelly call BdFlusQ to avoid deadlock */
    if (needevent)
        bd_wakeup(session->bd_thread_event);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)

static blk_qc_t bd_make_request(struct request_queue *dummy, struct bio *bio)
{
    bd_submit_bio_with_info(bio, bio_data_dir(bio) == 0 ? READ : WRITE);
    return BLK_QC_T_NONE;
}

#else
#  if LINUX_VERSION_CODE > KERNEL_VERSION(3, 1, 0)
/* called asynchronously by the system to say there are new request to process*/
void bd_make_request(struct request_queue *dummy, struct bio *bio)
{
    bd_submit_bio_with_info(bio, bio_data_dir(bio) == 0 ? READ : WRITE);
}
#  else
/* called asynchronously by the system to say there are new request to process*/
int bd_make_request(struct request_queue *dummy, struct bio *bio)
{
    bd_submit_bio_with_info(bio, bio_data_dir(bio) == 0 ? READ : WRITE);
    return 0;
}
#  endif
#endif


int bd_register_drv(struct bd_session *session)
{
    session->bd_major = register_blkdev(session->bd_major, EXA_BD_DEVICE_NAME);
    if (session->bd_major <= 0)
        return -1;

    return 0;
}


int bd_minor_remove(struct bd_minor *bd_minor)
{
    struct bd_session *session = bd_minor->bd_session;

    if (bd_minor->dead)
        bd_end_q(bd_minor, -EIO);

    if (!atomic_dec_and_test(&bd_minor->use_count))
    {
        bd_log_info("Minor usecout %s (%d) == %d \n", bd_minor->bd_name,
                    bd_minor->minor, atomic_read(&bd_minor->use_count));
        return 0;
    }
    bd_log_info("RefCount reach 0 for minor %s (%d)\n",
                bd_minor->bd_name, bd_minor->minor);

    bd_end_q(bd_minor, -EIO);
    if (session->bd_minor_last == bd_minor)
        session->bd_minor_last = session->bd_minor;

    del_gendisk(bd_minor->bd_gen_disk);
    put_disk(bd_minor->bd_gen_disk);
    blk_cleanup_queue(bd_minor->bd_gen_disk->queue);
    bd_minor->minor = -1;       /* so it can be reused */
    bd_put_session(&session);
    return 0;
}


int bd_minor_set_size(struct bd_minor *bd_minor, unsigned long size_in512_bytes)
{
    struct block_device *bdev;

    bdev = bdget_disk(bd_minor->bd_gen_disk, 0);

    if (bdev)
    {
        set_capacity(bd_minor->bd_gen_disk, size_in512_bytes);
        bdev->bd_inode->i_size = (loff_t) size_in512_bytes << 9;
        bdput(bdev);
        return 0;
    }

    return -1;
}

int bd_minor_add_new(struct bd_session *session, int minor,
                     unsigned long size_in512_bytes, bool readonly)
{
    struct bd_minor *bd_minor;
    struct bd_minor *bd_minor_recycle = NULL, *prev_bd_minor;
    struct gendisk *gen_disk;
    struct request_queue *queue;
    char bd_name[BD_DISK_NAME_SIZE];

    if (!MINOR_IS_VALID(minor))
    {
        bd_log_error("BD : Error adding invalid minor %d\n", minor);
        return -1;
    }

    snprintf(bd_name, BD_DISK_NAME_SIZE, "exa_vol_%d", minor);

    bd_log_debug("BD : bd_newMinor adding %d size %ld name %s\n",
                 minor, size_in512_bytes, bd_name);

    prev_bd_minor = NULL;
    for (bd_minor = session->bd_minor; bd_minor != NULL;
         bd_minor = bd_minor->bd_next)
    {
        if (bd_minor->minor != -1)
        {
            if (strncmp(bd_name, bd_minor->bd_gen_disk->disk_name,
                        BD_DISK_NAME_SIZE - 1) == 0)
            {
                bd_log_error("BD : duplicate name %16s for minor %d and new "
                             "minor %d\n", bd_name, bd_minor->minor, minor);
                return -1;
            }
        }

        if (bd_minor->minor == -1 && atomic_read(&bd_minor->use_count) <= 0)
            bd_minor_recycle = bd_minor;

        if (bd_minor->minor == minor)
        {
            bd_log_error("BD: Trying to resize %ld:%d forbiden because it's "
                         "an living device\n", session->bd_major, minor);
            return -1;
        }

        prev_bd_minor = bd_minor;
    }

    if (bd_minor_recycle == NULL)
    {
        bd_minor = kmalloc(sizeof(struct bd_minor), GFP_KERNEL);
        bd_minor->bd_next = NULL;

        /* Add in tail, so check if there is at least one element */
        if (session->bd_minor == NULL)
            session->bd_minor = bd_minor;
        else
            prev_bd_minor->bd_next = bd_minor;
    }
    else
        bd_minor = bd_minor_recycle;

    OS_ASSERT(bd_minor != NULL);

    bd_init_list(&session->bd_root, &bd_minor->bd_list);

    bd_minor->current_run = 0;
    bd_minor->need_sync = 0;

    bd_minor->bd_session = session;

    queue = blk_alloc_queue(GFP_KERNEL);

    blk_queue_logical_block_size(queue, 512);
    blk_queue_max_sectors(queue, session->bd_buffer_size / 512);
    blk_queue_make_request(queue, bd_make_request);

    spin_lock_init(&bd_minor->bd_lock);
    queue->queue_lock = &bd_minor->bd_lock;
    queue->queuedata = bd_minor;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
    queue->backing_dev_info.ra_pages = EXA_BD_READAHEAD >> (PAGE_SHIFT - 9);
#else
    queue->backing_dev_info->ra_pages = EXA_BD_READAHEAD >> (PAGE_SHIFT - 9);
#endif

    /* no partition will be allowed */
    gen_disk = alloc_disk(1);
    if (gen_disk == NULL)
    {
        /* Delete the element only if it was newly allocated here, thus
         * it was NOT recycled */
        if (bd_minor_recycle == NULL)
            kfree(bd_minor);
        return -1;
    }

    /* Initialize gendisk */
    gen_disk->major        = session->bd_major;
    gen_disk->first_minor  = minor;
    gen_disk->fops         = &bd_blk_fops;
    gen_disk->private_data = bd_minor;
    gen_disk->queue        = queue;
    strlcpy(gen_disk->disk_name, bd_name, sizeof(gen_disk->disk_name));
    set_capacity(gen_disk, size_in512_bytes);
    if (readonly)
        set_disk_ro(gen_disk, readonly);
    bd_minor->bd_gen_disk = gen_disk;

    /* Set to 1 because we are using it right now */
    atomic_set(&bd_minor->use_count, 1);

    /* a new minor use the session */
    atomic_inc(&session->total_use_count);

    bd_minor->minor = minor;
    bd_minor->dead  = false;

    add_disk(gen_disk);

    return 0;
}


/* all other flush must be already done */
void bd_unregister_drv(struct bd_session *session)
{
    unregister_blkdev(session->bd_major, EXA_BD_DEVICE_NAME);
}

static int bd_blk_open(struct block_device *bdev, unsigned __bitwise__ mode)
{
    struct bd_minor *bd_minor = bdev->bd_disk->private_data;

    /* FIXME this test is fu***d up: there is a race with the part
     * of code that tries to unregister. */
    atomic_inc(&bd_minor->use_count);
    if (atomic_read(&bd_minor->use_count) == 1)
    {
        atomic_dec(&bd_minor->use_count);
        bd_log_info("Prevent opening %s minor %d count 0 while removing it\n",
                    bd_minor->bd_name, bd_minor->minor);
        return -1;
    }
    bd_log_info("Open %s minor %d count %d\n", bd_minor->bd_name,
                bd_minor->minor, atomic_read(&bd_minor->use_count));
    return 0;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int bd_blk_release(struct gendisk *disk, unsigned __bitwise__ mode)
{
    struct bd_minor *bd_minor = disk->private_data;

    bd_log_info("Close minor %d\n", bd_minor->minor);
    bd_minor_remove(bd_minor);

    return 0;
}
#else
static void bd_blk_release(struct gendisk *disk, unsigned __bitwise__ mode)
{
    struct bd_minor *bd_minor = disk->private_data;

    bd_log_info("Close minor %d\n", bd_minor->minor);
    bd_minor_remove(bd_minor);
}
#endif

struct block_device_operations bd_blk_fops =
{
    .owner   = THIS_MODULE,
    .open    = bd_blk_open,
    .release = bd_blk_release,
};
