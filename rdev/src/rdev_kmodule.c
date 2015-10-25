/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "rdev/src/rdev_kmodule.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/file.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/socket.h>
#include <linux/pg.h>
#include <linux/version.h>
#include <net/sock.h>

#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"
#include "common/include/exa_names.h"

#include "os/include/os_inttypes.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
#define queue_max_phys_segments(queue) ((queue)->max_phys_segments)
#define queue_max_hw_segments(queue)   ((queue)->max_hw_segments)
#define queue_max_hw_sectors(queue)    ((queue)->max_hw_sectors)
#define queue_max_sectors(queue)       ((queue)->max_sectors)
#define get_capacity(disk)       ((disk)->capacity)
#endif

#define SECT_SIZE	512
#define SECT_SHIFT	9
#define SECT_PER_PAGE (PAGE_SIZE/SECT_SIZE)

/* Cannot check version of kernel directly for this as redhat backported the
 * modifications from 2.6.34 to 2.6.32 */
#if defined RHEL_MAJOR && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
#define queue_max_hw_segments   queue_max_segments
#define queue_max_phys_segments queue_max_segments
#endif

/* This is the number of async requests exa_rdev can handle at a very moment. */
#define EXA_RDEV_BH  64

/* EXA_RDEV_BVEC is actually the size of the bvect array, which means the size
 * of the page array that comes from the userland when performing IOs.
 * This MUST be large enougth to be able to store the whole request in a raw
 * thus should be configured in link with 'max_request_size'
 * FIXME add a parameter in module to be able to set this when starting. */
#define EXA_RDEV_BVEC  32 /* Should be large enougth to receive*/

/* size of the name  */
#define EXA_RDEV_NAME_SIZE 16

#define EXA_RDEV_BH_INDEX(bio, st) \
    ( ((unsigned long)bio - (unsigned long)&st->bh[0]) / sizeof(struct bh_stuff) )

#define EXA_RDEV_WRITE_MAX_CMD_LEN 19

struct exa_rdev_bh_struct {
    int pid_creator;
    char name_creator[EXA_RDEV_NAME_SIZE];
    enum {
        EXA_RDEV_LAST_OP_NONE = 0,
        EXA_RDEV_LAST_OP_READ,
        EXA_RDEV_LAST_OP_WRITE
    } last_op;

    int last_user_pid;
    char name_last_user[EXA_RDEV_NAME_SIZE];

    /* gloabal structure */
    struct semaphore sem_bh;

    /* index of first free bh_stuff */
    signed short bh_first_free;
    /* number of free  bh_stuff */
    signed short bh_free_nb;
    /* lock that protects free bh list */
    spinlock_t free_bh_lock;

    /* index of the first and last IO that was completed. All bh are linked
     * from this point so that fairness is kept (first ifinished IOs are checked
     * for completion first etc...) */
    signed short first_completed;
    signed short last_completed;
    /* lock that protects completed io list */
    spinlock_t completed_io_lock;

    struct bh_stuff {
        /* Index of next element in free or completed io lists. */
        signed short next;

        /* used to store private user data associated to the first element of a
         * request */
        user_land_io_handle_t private;

        int err;

        struct bio bio;

        struct bio_vec bio_vec[EXA_RDEV_BVEC];
    } bh[EXA_RDEV_BH];

    struct bdev_entry *bdev;

    /* prev struct that use the same bd */
    struct exa_rdev_bh_struct *prevbd;
    /* next struct that use the same bd */
    struct exa_rdev_bh_struct *nextbd;
};

struct exa_rdev_file_struct {
    struct semaphore sem_fd;
    struct exa_rdev_bh_struct *st;
};

static struct exa_rdev_bdev {
    /* spinlock to manage opened dev; */
    spinlock_t exa_rdev_bdev_open;
    struct bdev_entry {
	int major;
	int minor;
	int last_error;
	struct block_device *dev;
	int refcount; /** < refcount == 0 mean free */
	int max_sect_per_bio;
	long long size_in_kb;
	struct exa_rdev_bh_struct *first;
    } bdev_entries[EXA_RDEV_MAX_DEV_OPEN];
} bdev_cache;

static int exa_rdev_flush_bh(rdev_op_t op, struct bio *bio);

static struct exa_rdev_bh_struct *exa_rdev_bh_alloc(void)
{
    struct exa_rdev_bh_struct *st;
    int i;

    st = vmalloc(sizeof(struct exa_rdev_bh_struct));
    if (st == NULL)
        return NULL;

    memset(st, 0, sizeof(struct exa_rdev_bh_struct));

    for (i = 0; i < EXA_RDEV_BH - 1; i++)
	st->bh[i].next = i + 1;
    st->bh[EXA_RDEV_BH - 1].next = -1;

    for (i = 0; i < EXA_RDEV_BH; i++)
	st->bh[i].err = -RDEV_ERR_UNKNOWN;

    sema_init(&st->sem_bh, 0);

    st->first_completed = -1;
    st->last_completed = -1;
    spin_lock_init(&st->completed_io_lock);

    st->bh_first_free = 0;
    st->bh_free_nb = EXA_RDEV_BH;
    spin_lock_init(&st->free_bh_lock);

    st->bdev = NULL;

    st->pid_creator = current->pid;
    strlcpy(st->name_creator, current->comm, EXA_RDEV_NAME_SIZE);

    return st;
}

/**
 * @param new_chaine 1 it's the first bio/buffer head of a new request, 0 this
 * bio/buffer head
 * follow the previous and is in the same request
 * @param st
 * @return the new allocated bio/buffer head
 */
static struct bh_stuff *exa_rdev_bh_get_free(struct exa_rdev_bh_struct *st)
{
    int free;
    unsigned long flags;

    spin_lock_irqsave(&st->free_bh_lock, flags);

    if (st->bh_first_free == -1)
    {
        spin_unlock_irqrestore(&st->free_bh_lock, flags);
        return NULL;
    }

    free = st->bh_first_free;
    st->bh_first_free = st->bh[st->bh_first_free].next;

    st->bh[free].next = -1;
    st->bh_free_nb--;

    spin_unlock_irqrestore(&st->free_bh_lock, flags);

    return &st->bh[free];
}

/**
 * Free all bio, buffer head one the request and decrement usage of each
 * page used in this request
 * @param num index of the first bio/buffer head of the request
 * @param st
 */
static void exa_rdev_bh_put_free(struct bh_stuff *bh, struct exa_rdev_bh_struct *st)
{
    int i;
    struct page *page = NULL;
    unsigned long flags;
    int bh_idx = EXA_RDEV_BH_INDEX(&bh->bio, st);

    EXA_ASSERT(bh_idx < EXA_RDEV_BH);

    for (i = 0; i < bh->bio.bi_vcnt; i++)
    {
        struct page *next_page = bh->bio_vec[i].bv_page;
        bh->bio_vec[i].bv_page = NULL;
        if (page != next_page)
            put_page(next_page);
        page = next_page;
    }

    spin_lock_irqsave(&st->free_bh_lock, flags);

    bh->next    = st->bh_first_free;
    /* Kill bh->private content, this has to be correctly set when submitting
     * an IO. 0xDD is to make sure that the content is meaningless */
    memset(&bh->private, 0xDD, sizeof(bh->private));
    bh->err     = -RDEV_ERR_UNKNOWN;

    st->bh_first_free = bh_idx;
    st->bh_free_nb++;

    spin_unlock_irqrestore(&st->free_bh_lock, flags);
}

/*
 * Get the first (in front) finished IO in the finished list.
 * If there is no finished IO yet, this function return NULL.
 *
 * @param st   the rdev handle.
 *
 * return the fisrst completed IO in the completed list, or NULL.
 */
static struct bh_stuff *pop_bh_from_finished_list(struct exa_rdev_bh_struct *st)
{
    unsigned long flags;
    struct bh_stuff *bh = NULL;

    spin_lock_irqsave(&st->completed_io_lock, flags);

    if (st->first_completed != -1)
    {
        bh = &st->bh[st->first_completed];
        st->first_completed = bh->next;

        if (st->first_completed == -1)
            st->last_completed = -1;
    }

    spin_unlock_irqrestore(&st->completed_io_lock, flags);

    return bh;
}

/*
 * Put a finish IO in the finished list.
 * The request is put at the back of the list for fairness upon completion.
 *
 * @param index   index of the bh in the bh array
 *                FIXME this function would really deserve to take the bh
 *                itself, playing with indexes makes the caller know how bh are
 *                stored/handled, which is bad.
 * @param st      the rdev handle.
 */
static void pushback_bh_in_finished_list(int index, struct exa_rdev_bh_struct *st)
{
    unsigned long flags;

    spin_lock_irqsave(&st->completed_io_lock, flags);

    if (st->last_completed == -1)
        st->first_completed = index;
    else
        st->bh[st->last_completed].next = index;

    st->last_completed = index;
    st->bh[index].next = -1;

    spin_unlock_irqrestore(&st->completed_io_lock, flags);
}

/**
 * function called at the end of the process of one bio
 * @param bh ended bio
 * @param bytes_done byte effectivelly write or read
 * @param err 0 success otherwise errror
 * @return 0 : no problem
 */
static void __exa_rdev_end_io(struct bio *bio, int err)
{
    struct exa_rdev_bh_struct *st = bio->bi_private;
    int bh_idx;

    bh_idx = EXA_RDEV_BH_INDEX(bio, st);

    st->bh[bh_idx].err = err != 0 ? RDEV_REQUEST_END_ERROR : RDEV_REQUEST_END_OK;

    pushback_bh_in_finished_list(bh_idx, st);

    up(&st->sem_bh);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static int exa_rdev_end_io(struct bio *bio, unsigned int bytes_done, int err)
{
    /* XXX this test is legacy and is supposed to fix bug #3046 but I really
     * do not know why/how... I also ignore why this only apply for kernels
     * < 2.6.24 (IMHO for other kernels this is just buggy...) */
    if (bio->bi_size)
	return 1;

    __exa_rdev_end_io(bio, err);
    return 0;
}
#else
static void exa_rdev_end_io(struct bio *bio, int err)
{
    __exa_rdev_end_io(bio, err);
}
#endif

/**
 * find a device in a bdev cache
 * @param major major of device
 * @param minor minor of device
 * @return -1  : not finded
 *         >=0 : index in cache
 */
static struct bdev_entry *exa_rdev_find_bdev_in_cache(int major, int minor)
{
    int i;

    for (i = 0; i < EXA_RDEV_MAX_DEV_OPEN; i++)
    {
	struct bdev_entry *bdev = &bdev_cache.bdev_entries[i];
	if (bdev->refcount > 0 && bdev->major == major && bdev->minor == minor)
	    return bdev;
    }
    return NULL;
}

/**
 * find a free entry in a bdev cache
 * @return -1  : not found
 *         >=0 : index in cache
 */
static struct bdev_entry *exa_rdev_find_free_bdev(void)
{
    int i;
    for (i = 0; i < EXA_RDEV_MAX_DEV_OPEN; i++)
	if (bdev_cache.bdev_entries[i].refcount == 0)
	    return &bdev_cache.bdev_entries[i];

    return NULL;
}


/**
 * init bdev_cache
 * @return -ENOMEM if not enough memory
 */
static int exa_rdev_bdev_cache_init(void)
{
    int i;
    for (i = 0; i < EXA_RDEV_MAX_DEV_OPEN; i++)
    {
	bdev_cache.bdev_entries[i].refcount = 0;
	bdev_cache.bdev_entries[i].dev = NULL;
    }

    spin_lock_init(&bdev_cache.exa_rdev_bdev_open);

    return EXA_SUCCESS;
}

/**
 * free bdev_cache and check if some bdev havr already a refcount
 */
static void exa_rdev_bdev_cache_stop(void)
{
    int i;

    for (i = 0; i < EXA_RDEV_MAX_DEV_OPEN; i++)
    {
	struct bdev_entry *bdev = &bdev_cache.bdev_entries[i];
	if (bdev->refcount != 0)
	    printk("exa_rdev : %d:%d invalid refcount at stop (%d)\n",
		    bdev->major, bdev->minor, bdev->refcount);
    }

    spin_lock_init(&bdev_cache.exa_rdev_bdev_open);
}

/**
 * get the last error of a bdev in cache
 * if last error was EXA_RDEV_REQUEST_END_OK, the error status is cleaned
 * if last error was EXA_RDEV_REQUEST_END_ERROR, it's not clean
 * @param st current context
 * @return -RDEV_ERR_INVALID_DEVICE with refcount == 0
 *         -RDEV_ERR_UNKNOWN or unknown last error
 *          EXA_RDEV_REQUEST_END_OK
 *          EXA_RDEV_REQUEST_END_ERROR
 */
static int __get_last_error(struct exa_rdev_bh_struct *st)
{
    unsigned long flags;
    int last_error;

    EXA_ASSERT(st->bdev != NULL);

    spin_lock_irqsave(&bdev_cache.exa_rdev_bdev_open, flags);

    if (st->bdev->refcount == 0)
	last_error = -RDEV_ERR_INVALID_DEVICE;
    else
    {
	last_error = st->bdev->last_error;
	if (last_error != RDEV_REQUEST_END_ERROR)
	    st->bdev->last_error = -RDEV_ERR_UNKNOWN;
    }

    spin_unlock_irqrestore(&bdev_cache.exa_rdev_bdev_open, flags);

    return last_error;
}

/**
 * set the new last error, if last error is EXA_RDEV_REQUEST_END_ERROR, it's
 * not reset to reset EXA_RDEV_REQUEST_END_ERROR, we must use
 * EXA_RDEV_RELOAD_DEVICE
 * @param st the current context
 */
static void exa_rdev_set_last_error(struct exa_rdev_bh_struct *st, int error)
{
    unsigned long flags;

    if (st->bdev == NULL)
	return;

    spin_lock_irqsave(&bdev_cache.exa_rdev_bdev_open, flags);

    if (st->bdev->last_error != RDEV_REQUEST_END_ERROR
        || error == RDEV_RELOAD_DEVICE)
    {
	if (error == RDEV_RELOAD_DEVICE)
	    error = -RDEV_ERR_UNKNOWN;
	st->bdev->last_error = error;
    }

    spin_unlock_irqrestore(&bdev_cache.exa_rdev_bdev_open, flags);
}

/**
 * set last error based on the major minor, it like the function decribe up
 * @param major
 * @param minor
 * @return -RDEV_ERR_INVALID_DEVICE
 *         EXA_SUCCESS
 */
static int exa_rdev_set_last_error_by_devnum(int major, int minor, int error)
{
    unsigned long flags;
    int index;

    spin_lock_irqsave(&bdev_cache.exa_rdev_bdev_open, flags);

    for (index = 0; index < EXA_RDEV_MAX_DEV_OPEN; index++)
    {
	struct bdev_entry *bdev = &bdev_cache.bdev_entries[index];

	if (bdev->refcount > 0 && bdev->major == major && bdev->minor == minor)
	{
	    if (bdev->last_error == RDEV_REQUEST_END_ERROR
		&& error != RDEV_RELOAD_DEVICE)
		break;

	    if (error == RDEV_RELOAD_DEVICE)
		error = -RDEV_ERR_UNKNOWN;

	    bdev->last_error = error;
	    break;
	}
    }

    spin_unlock_irqrestore(&bdev_cache.exa_rdev_bdev_open, flags);

    if (index >= EXA_RDEV_MAX_DEV_OPEN)
	return -RDEV_ERR_INVALID_DEVICE;

    return EXA_SUCCESS;
}

/**
 * get the pid and name of creator of the Nieme refcount ("unknown" if changed
 * recentely)
 * @param index index in cache
 * @param n n ieme refcounter
 * @param pid_creator pid of creator of the fd that reference this bd,
 *                    -1 if unknown
 * @param name_creator name of creator of the fd that reference this bd
 *                     "unknown" if unknown.
 * @param last_op last operation, it was reset after this operation
 * @param last_user_pid pid of last user
 * @param name_last_user name of last user
 */
static void exa_rdev_bdev_get_refcount_info(struct bdev_entry *bdev, int n,
                                            int *pid_creator, char *name_creator,
                                            int *last_op, int *last_user_pid,
                                            char *name_last_user)
{
    unsigned long flags;
    struct exa_rdev_bh_struct *st;

    *pid_creator = -1;
    *last_op = EXA_RDEV_LAST_OP_NONE;
    *last_user_pid = -1;
    strlcpy(name_creator, "unknown", EXA_RDEV_NAME_SIZE);
    strlcpy(name_last_user, "unknown", EXA_RDEV_NAME_SIZE);

    spin_lock_irqsave(&bdev_cache.exa_rdev_bdev_open, flags);

    st = bdev->first;

    while (st != NULL)
    {
        if (n == 0)
        {
            *pid_creator = st->pid_creator;
            *last_op = st->last_op;
            st->last_op = EXA_RDEV_LAST_OP_NONE;
            *last_user_pid = st->last_user_pid;
            strlcpy(name_creator,   st->name_creator,   EXA_RDEV_NAME_SIZE);
            strlcpy(name_last_user, st->name_last_user, EXA_RDEV_NAME_SIZE);
            break;
        }
        st = st->nextbd;
        n--;
    }

    spin_unlock_irqrestore(&bdev_cache.exa_rdev_bdev_open, flags);
}

/**
 * show the state of the bdev cache, we use no spinlock because there are no
 * problem if we use some spinlock
 */
static void exa_rdev_bdev_show(void)
{
    int i, pid, last_op, last_user_pid;
    char name[EXA_RDEV_NAME_SIZE];
    char name_last_user[EXA_RDEV_NAME_SIZE];

    printk("Begin of bdev cache\n");

    for (i = 0; i < EXA_RDEV_MAX_DEV_OPEN; i++)
    {
	int cnt;
	char *error_str = "";
	struct bdev_entry *bdev = &bdev_cache.bdev_entries[i];

	cnt = bdev->refcount;
	if (cnt <= 0)
	    continue;

	if (bdev->last_error == -RDEV_ERR_UNKNOWN)
	    error_str = "NONE";

	if (bdev->last_error == RDEV_REQUEST_END_OK)
	    error_str = "OK";

	if (bdev->last_error == RDEV_REQUEST_END_ERROR)
	    error_str = "ERROR";

	printk("%3d:%4d refcount %3d last error %s\n",
		bdev->major, bdev->minor, bdev->refcount, error_str);

	/* FIXME this really looks like a for loop */
	while (cnt > 0)
	{
	    exa_rdev_bdev_get_refcount_info(bdev, cnt - 1, &pid, name, &last_op,
		    &last_user_pid, name_last_user);
	    switch (last_op)
	    {
		case EXA_RDEV_LAST_OP_NONE:
		    error_str = "NONE";
		    break;
		case EXA_RDEV_LAST_OP_READ:
		    error_str = "READ";
		    break;
		case EXA_RDEV_LAST_OP_WRITE:
		    error_str = "WRITE";
		    break;
		default:
		    error_str = "NONE";
	    }
	    printk("created by %16s (%6d) last user %16s (%6d) last op %s\n",
		    name, pid, name_last_user, last_user_pid, error_str);
	    cnt--;
	}
    }
    printk("End of bdev cache (unknown and pid==-1 is due to change in refcount"
	    " during this display)\n");
}

/**
 * put the device major:minor associted with st, do nothing if no device
 * associated
 * @param st
 */
static void exa_rdev_bdput(struct exa_rdev_bh_struct *st)
{
    unsigned long flags;
    struct block_device *dev = NULL;

    if (st->bdev == NULL)
        return;

    spin_lock_irqsave(&bdev_cache.exa_rdev_bdev_open, flags);

    if (st->nextbd != NULL)
        st->nextbd->prevbd = st->prevbd;

    if (st->prevbd != NULL)
        st->prevbd->nextbd = st->nextbd;
    else
        st->bdev->first = st->nextbd;

    st->bdev->refcount--;

    if (st->bdev->refcount == 0)
    {
        dev = st->bdev->dev;
        st->bdev->first = NULL;
    }

    st->bdev = NULL;

    spin_unlock_irqrestore(&bdev_cache.exa_rdev_bdev_open, flags);

    if (dev != NULL)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
        blkdev_put(dev);
#else
        blkdev_put(dev, FMODE_READ|FMODE_WRITE);
#endif
}

/**
 * get a bdev from the cache or create a new one
 * @param st
 * @return EXA_SUCCESS success
 *         -RDEV_ERR_INVALID_DEVICE  invalid major:minor
 */
static int exa_rdev_bdinit(struct exa_rdev_bh_struct *st, int major, int minor)
{
    unsigned long flags;
    struct bdev_entry *bdev;

    /* Init can only be called once. Actually, calling it twice is useless
     * and lead to some difficult error handling: what to do if we try to init
     * with another pair major/minor? swallowing the error and overwrite any
     * preexistent bdev seems really error prompt. */
    if (st->bdev != NULL)
	return -EEXIST;

    spin_lock_irqsave(&bdev_cache.exa_rdev_bdev_open, flags);

    bdev = exa_rdev_find_bdev_in_cache(major, minor);

    if (bdev != NULL)
    {
	st->nextbd = bdev->first;
	st->nextbd->prevbd = st;
	st->prevbd = NULL;
	bdev->first = st;
	bdev->refcount++;
    }
    else
    {
        long max_sect_per_bio;
        int ret;
        struct block_device *dev = bdget(MKDEV(major, minor));

	if (dev == NULL)
        {
            spin_unlock_irqrestore(&bdev_cache.exa_rdev_bdev_open, flags);
	    return -RDEV_ERR_INVALID_DEVICE;
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	ret = blkdev_get(dev, FMODE_READ | FMODE_WRITE, 0);
#else
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
	ret = blkdev_get(dev, FMODE_READ | FMODE_WRITE, NULL);
# else
	ret = blkdev_get(dev, FMODE_READ | FMODE_WRITE);
# endif
#endif
	if (ret < 0)
	{
	    bdput(dev);
            spin_unlock_irqrestore(&bdev_cache.exa_rdev_bdev_open, flags);
	    return -RDEV_ERR_INVALID_DEVICE;
	}

        bdev = exa_rdev_find_free_bdev();
        if (bdev == NULL)
        {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
	    blkdev_put(dev);
#else
	    blkdev_put(dev, FMODE_READ|FMODE_WRITE);
#endif
	    bdput(dev);
            spin_unlock_irqrestore(&bdev_cache.exa_rdev_bdev_open, flags);
            return -ENOSPC;
        }

        bdev->size_in_kb = get_capacity(dev->bd_disk) / 2;
        max_sect_per_bio = queue_max_sectors(dev->bd_disk->queue);

        if (max_sect_per_bio > EXA_RDEV_BVEC * SECT_PER_PAGE)
            max_sect_per_bio = EXA_RDEV_BVEC * SECT_PER_PAGE;

        if (max_sect_per_bio >
                queue_max_phys_segments(dev->bd_disk->queue) * SECT_PER_PAGE)
            max_sect_per_bio =
                queue_max_phys_segments(dev->bd_disk->queue) * SECT_PER_PAGE;

        if (max_sect_per_bio > queue_max_hw_sectors(dev->bd_disk->queue))
            max_sect_per_bio = queue_max_hw_sectors(dev->bd_disk->queue);

        if (max_sect_per_bio >
                queue_max_hw_segments(dev->bd_disk->queue) * SECT_PER_PAGE)
            max_sect_per_bio =
                queue_max_hw_segments(dev->bd_disk->queue) * SECT_PER_PAGE;

        bdev->max_sect_per_bio = max_sect_per_bio;
        bdev->refcount         = 1;
        bdev->dev              = dev;
        bdev->last_error       = -RDEV_ERR_UNKNOWN;
        bdev->major            = major;
        bdev->minor            = minor;

        /* dev == NULL -> no need to put him
         * (FIXME crappy side effect programming)*/
        dev = NULL;
        st->nextbd = NULL;
        st->prevbd = NULL;

        bdev->first = st;
    }

    spin_unlock_irqrestore(&bdev_cache.exa_rdev_bdev_open, flags);

    /* FIXME next is not thread safe. */
    st->bdev = bdev;

    return EXA_SUCCESS;
}

/**
 * get size in sectors of the targeted device
 * @param st
 * @return size of the device
 */
static long long __get_size_in_sectors(struct exa_rdev_bh_struct *st)
{
    if (st->bdev == NULL)
	return 0;
    return BYTES_TO_SECTORS(st->bdev->size_in_kb * 1024);
}

/**
 * Get a free bio/bvec/buffer head for a request, filled it with the current
 * fragment of the request.
 * Note : a fragment is a part of a request blocksized alligned and that fit
 * entirelly in one page.
 * @param page page that contain this fragment
 * @param size_in_sect number of sector to read/write in this bh/bvec
 * @param[inout] first -1 if it's the first fragment of an entire request, in
 *               this case it be set to	the index of the first bio/buffer head
 *               used, otherwise it won(t changed
 * @param st
 * @return EXA_SUCCESS success, error otherwise
 */
static int exa_rdev_add_bh(struct exa_rdev_bh_struct *st,
                           const struct exa_rdev_request_kernel *req,
                           struct bdev_entry *bdev,
                           struct page *page[], int page_count,
                           user_land_io_handle_t *tag)
{
    struct bio *bio;
    struct bh_stuff *bio_a;

    bio_a = exa_rdev_bh_get_free(st);

    if (bio_a == NULL)
        return RDEV_REQUEST_NOT_ENOUGH_FREE_REQ;

    bio_a->err = -RDEV_ERR_UNKNOWN;

    bio_a->private = *tag;

    bio = &bio_a->bio;
    memset(bio, 0, sizeof(*bio));

    bio->bi_sector  = req->sector;
    bio->bi_bdev    = bdev->dev;
    bio->bi_end_io  = exa_rdev_end_io;
    bio->bi_private = (void *)st;
    bio->bi_io_vec  = bio_a->bio_vec;

    atomic_set(&bio->bi_cnt, 1);

    bio->bi_flags = 1 << BIO_UPTODATE;

    for (bio->bi_vcnt = 0; bio->bi_vcnt < page_count; bio->bi_vcnt++)
    {
        struct bio_vec *bio_vec = &bio_a->bio_vec[bio->bi_vcnt];

        bio_vec->bv_page = page[bio->bi_vcnt];
        bio_vec->bv_len = SECTORS_TO_BYTES(MIN(SECT_PER_PAGE, req->sector_nb - bio->bi_vcnt * SECT_PER_PAGE));
        bio_vec->bv_offset = 0; /* offset inside page is 0 */
    }

    bio->bi_size = SECTORS_TO_BYTES(req->sector_nb);
    bio->bi_max_vecs = bio->bi_vcnt;

    return exa_rdev_flush_bh(req->op, &bio_a->bio);
}

/**
 * Used to finished bio not submited and to submit it, only used used in 2.6
 * @param st
 * @return EXA_SUCCESS success otherwise error
 */
static int exa_rdev_flush_bh(rdev_op_t op, struct bio *bio)
{
    EXA_ASSERT(RDEV_OP_VALID(op));

    /* one bio request pending */
    EXA_ASSERT(bio != NULL);

    switch (op)
    {
    case RDEV_OP_READ:
        submit_bio(READ, bio);
        break;

    case RDEV_OP_WRITE:
        submit_bio(WRITE, bio);
        break;

    case RDEV_OP_WRITE_BARRIER:
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        submit_bio(WRITE | (1 << BIO_RW_BARRIER), bio);
#else
	/* FIXME WRITE_FLUSH_FUA is not equivalent to having a barrier, so this
	 * may be wrong. Nevertheless, there seem to be some mix up between
	 * barrier an fua in the user land part, so all this needs to be
	 * reworked properly */
        submit_bio(WRITE_FLUSH_FUA, bio);
#endif
        break;

    case RDEV_OP_INVALID:
        EXA_ASSERT_VERBOSE(FALSE, "Invalid request operation: %d", op);
        break;
    }

    if (bio_flagged(bio, BIO_EOPNOTSUPP))
    {
        printk("exa_rdev: Severe Error BIO_EOPNOTSUPP\n");
        return -RDEV_ERR_BIG_ERROR;
    }

    return EXA_SUCCESS;
}

/**
 * Force all submited request (bio/buffer head) to start to be processed by
 * disk driver
 * @param st
 */
static void exa_rdev_start_bh(struct exa_rdev_bh_struct *st)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
    struct request_queue *q = NULL;

    if (st->bdev != NULL)
	q = bdev_get_queue(st->bdev->dev);

    if (q != NULL)
	blk_run_backing_dev(&q->backing_dev_info, NULL);
#else
    /* This is supposed to be automatically on newer kernels */
#endif
}

/**
 * the real make request, the info of the data to read/write is in req
 * note that this fonction can be used directly from kernel to hide difference
 * between 2.4 buffer_head and  2.6 bio
 * @param st
 * @return EXA_SUCCESS success other if error
 */
static int exa_rdev_make_one(struct exa_rdev_bh_struct *st,
                             const struct exa_rdev_request_kernel *req,
                             struct bdev_entry *bdev,
                             user_land_io_handle_t *tag)
{
    /* array of the user page of the user buffer we must have enough pointer to
     * store the pointer on each page of the max request size */
    struct page *page_array[EXA_RDEV_BVEC];
    unsigned int page_count;
    int i;

    if (req->sector + req->sector_nb > __get_size_in_sectors(st))
	return -RDEV_ERR_TOO_SMALL;

    page_count = quotient_ceil64(req->sector_nb, SECT_PER_PAGE);

    if (req->sector_nb > st->bdev->max_sect_per_bio)
    {
	printk("exa_rdev: request first sect %ld of %ld sector > %d max sector"
                " per request\n", req->sector, req->sector_nb,
		st->bdev->max_sect_per_bio);
	return -RDEV_ERR_REQUEST_TOO_BIG;
    }

    if ((((unsigned long) req->buffer) & (PAGE_SIZE - 1)) != 0)
    {
	printk("exa_rdev: %d:%d invalid request page request dev size is "
                "%lld sectors; request offset %ld size %ld buffer %p\n",
                st->bdev->major, st->bdev->minor, __get_size_in_sectors(st),
                req->sector, req->sector_nb, req->buffer);
	return -RDEV_ERR_INVALID_OFFSET;
    }

    down_read(&current->mm->mmap_sem);
    i = get_user_pages(current, current->mm, (unsigned long) req->buffer,
                       page_count, 1, 0, page_array, NULL);
    up_read(&current->mm->mmap_sem);

    if (page_count != i)
    {
        int j;
        printk("exa_rdev: Error: nbpage req %d, nbpage get %d  offset %ld "
                "size %ld  buffer %p\n", page_count, i, req->sector,
                req->sector_nb, req->buffer);
        for (j = 0; j < i; j++)
            put_page(page_array[j]);
        return -RDEV_ERR_INVALID_BUFFER;
    }

    {
        int err = exa_rdev_add_bh(st, req, bdev, page_array, page_count, tag);
        if (err != 0)
            for (i = 0; i < page_count; i++)
                put_page(page_array[i]);

        return err;
    }
}

/**
 * wait for one request ended
 * @param[in] wait        Does the caller want to wait for the completion of
 *                        a request if none were finished yet?
 * @param[out] private    if not NULL will contain the nbd_private data of the
 *                        request that is being acknoledged; if all request
 *                        were already completed, private is set to 0.
 * @param[in] st          the structure that contain all info about current
 *                        request pending and all free data for request
 *
 * @return  EXA_RDEV_REQUEST_END_ERROR  : one request was finded with one error
 *          EXA_RDEV_REQUEST_END_OK     : one request was find without error
 *          EXA_RDEV_REQUEST_NONE_ENDED : no request finded
 *          EXA_RDEV_REQUEST_ALL_ENDED  : all request was done, so none to wait for
 */
static int exa_rdev_wait_one(bool wait, user_land_io_handle_t *h,
                             struct exa_rdev_bh_struct *st)
{
    unsigned long flags;
    bool all_free, one_io_finished;
    struct bh_stuff *bh;
    int err;

    if (h != NULL)
        h->nbd_private = NULL;

    spin_lock_irqsave(&st->free_bh_lock, flags);

    all_free = st->bh_free_nb == EXA_RDEV_BH;

    spin_unlock_irqrestore(&st->free_bh_lock, flags);

    if (all_free)
        return RDEV_REQUEST_ALL_ENDED;

    spin_lock_irqsave(&st->completed_io_lock, flags);

    one_io_finished = st->first_completed != -1;

    spin_unlock_irqrestore(&st->completed_io_lock, flags);

    if (!one_io_finished && !wait)
        return RDEV_REQUEST_NONE_ENDED;

    if (!one_io_finished)
        exa_rdev_start_bh(st);

    do {
        down(&st->sem_bh);
        bh = pop_bh_from_finished_list(st);
    } while (bh == NULL);

    EXA_ASSERT(bh != NULL);

    if (h != NULL)
        *h = bh->private;

    err = bh->err;
    exa_rdev_bh_put_free(bh, st);

    exa_rdev_set_last_error(st, err);

    return err;
}


/**
 * Begin a new request
 * @param st the structure that contain all info about current request pending
 *           and all free data for request
 * @return  <0 if a new request cannot be start 0 if the request start and
 *          1 if the request start and another have ended
 */
static int _exa_rdev_make_request_new(struct exa_rdev_bh_struct *st,
                                      struct exa_rdev_request_kernel *req,
                                      struct bdev_entry *bdev)
{
    int err = __get_last_error(st);
    if (err == RDEV_REQUEST_END_ERROR)
	return err;

    st->last_op = req->op == 1 ?
	          EXA_RDEV_LAST_OP_WRITE : EXA_RDEV_LAST_OP_READ;

    if (st->last_user_pid != current->pid)
    {
	st->last_user_pid = current->pid;
	strlcpy(st->name_last_user, current->comm, EXA_RDEV_NAME_SIZE);
    }

    err = exa_rdev_make_one(st, req, bdev, &req->h);
    if (err != EXA_SUCCESS)
    {
	/* this is the only transitory error :
	 * after some call to exa_rdev_wait_one()
	 * it will work
	 */
	if (err != RDEV_REQUEST_NOT_ENOUGH_FREE_REQ)
	    exa_rdev_set_last_error(st, RDEV_REQUEST_END_ERROR);
	return err;
    }

    return EXA_SUCCESS;
}

/**
 * Wait for the end of all request.
 * @param st the structure that contain all info about current request pending
 * and all free data for request
 * @return EXA_RDEV_REQUEST_ALL_ENDED
 */
static void exa_rdev_make_request_end(struct exa_rdev_bh_struct *st)
{
    while (exa_rdev_wait_one(true, NULL, st) != RDEV_REQUEST_ALL_ENDED)
        ;
}

/*
 * EXA_RDEV file : file/ioctl interface to user, general structures
 */
static ssize_t exa_rdev_read(struct file *filp, char *buf, size_t count,
                             loff_t *l)
{
    return -1;
}

static ssize_t exa_rdev_write(struct file *file, const char *buf,
                              size_t size, loff_t *off)
{
    char cmd[EXA_RDEV_WRITE_MAX_CMD_LEN + 1];
    int major, minor, status;
    int err;
    int nba = 0;

    if (size > EXA_RDEV_WRITE_MAX_CMD_LEN)
        return -EINVAL;

    err = copy_from_user(cmd, buf, size);
    if (err < 0)
        return err;

    cmd[size] = '\0';

    /* Special case for display of debug info. */
    if (strcmp(cmd, "d\n") == 0)
    {
	exa_rdev_bdev_show();
	return size;
    }

    nba = sscanf(cmd, "%d %d %d\n", &major, &minor, &status);

    if (nba != 3)
        return -EINVAL;

    switch (status)
    {
    case 1:
        err = RDEV_REQUEST_END_OK;
        break;

    /* if the command == 2 then deactivate the device */
    case 2:
        err = RDEV_REQUEST_END_ERROR;
        break;

        /* if the command == 3 then reactivate the device */
    case 3:
        err = RDEV_RELOAD_DEVICE;
        break;

    default:
        return -EINVAL;
    }

    return exa_rdev_set_last_error_by_devnum(major, minor, err) == 0
                   ? size : -EINVAL;
}


#if HAVE_UNLOCKED_IOCTL
static long exa_rdev_ioctl(struct file *filp, unsigned int cmd, unsigned long __arg)
#else
static int exa_rdev_ioctl(struct inode *inode, struct file *filp,
                          unsigned int cmd, unsigned long __arg)
#endif

{
    int ret = -ENOTTY;
    int copy = 0;
    struct exa_rdev_file_struct *sfs =
	(struct exa_rdev_file_struct *) filp->private_data;
    void *arg = (void *)__arg;

    EXA_ASSERT(filp->private_data != NULL);

    /* If fd passed was not create with exa_rdev_handle_alloc() the
     * init was not done and thus bdev is NULL */
    if (sfs->st->bdev == NULL && cmd != EXA_RDEV_INIT)
        return -EINVAL;

    switch (cmd)
    {
	case EXA_RDEV_INIT:
            {
                struct exa_rdev_major_minor majmin;
                copy = copy_from_user(&majmin, arg, sizeof(struct exa_rdev_major_minor));
                ret = exa_rdev_bdinit(sfs->st, majmin.major, majmin.minor);
            }
            break;

	case EXA_RDEV_FLUSH:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
            ret = blkdev_issue_flush(sfs->st->bdev->dev, GFP_NOIO, NULL);
#else
            ret = blkdev_issue_flush(sfs->st->bdev->dev, NULL);
#endif
            break;

	case EXA_RDEV_MAKE_REQUEST_NEW:
            {
                user_land_io_handle_t h;
                struct exa_rdev_request_kernel *u_req = arg;
                struct exa_rdev_request_kernel req;
                copy = copy_from_user(&req, u_req, sizeof(req));

                ret = _exa_rdev_make_request_new(sfs->st, &req, sfs->st->bdev);
                if (ret != EXA_SUCCESS)
                    break;

                /* Calling make_request_new() has the (wonderful) side effect
                 * to signal any completed request. This is done by modifying
                 * the caller's buffer (setting nbd_private to the one of the
                 * completed request). If no request was completed, nbd_private
                 * is set to 0 and the return value is set correctly.
                 * Note: the flag "no_wait" is passed to exa_rdev_wait_one()
                 * so that the caller does not remain stuck here in case of
                 * nothing was (yet) completed. */
                ret = exa_rdev_wait_one(false, &h, sfs->st);

                /* private is the only field updated read by user */
                copy = copy_to_user(&u_req->h, &h, sizeof(u_req->h));
            }
	    break;

	case EXA_RDEV_GET_LAST_ERROR:
	    ret = __get_last_error(sfs->st);
	    break;

	case EXA_RDEV_WAIT_ONE_REQUEST:
            {
                user_land_io_handle_t h;

                ret = exa_rdev_wait_one(true, &h, sfs->st);

                /* private is the only field updated read by user */
                copy = copy_to_user(arg, &h, sizeof(h));
            }
	    break;

	case EXA_RDEV_ACTIVATE:
	    ret = exa_rdev_set_last_error_by_devnum(sfs->st->bdev->major,
                                                    sfs->st->bdev->minor,
						    RDEV_RELOAD_DEVICE);
	    break;

        case EXA_RDEV_DEACTIVATE:
	    ret = exa_rdev_set_last_error_by_devnum(sfs->st->bdev->major,
                                                    sfs->st->bdev->minor,
						    RDEV_REQUEST_END_ERROR);
	    break;

	default:
	    ret = -ENOTTY;
    }

    return ret;
}

static int exa_rdev_open(struct inode *inode, struct file *filp)
{
    struct exa_rdev_file_struct *sfs =
	vmalloc(sizeof(struct exa_rdev_file_struct));
    if (sfs == NULL)
    {
	/* allocated buffer_request independentelly */
	printk("exa_rdev: not enough memory for structure (%" PRIzu ")\n",
		sizeof(struct exa_rdev_file_struct));
	return -1;
    }
    filp->private_data = (void *) sfs;
    sema_init(&sfs->sem_fd, 1);

    sfs->st = exa_rdev_bh_alloc();
    if (sfs->st == NULL)
    {
        printk("exa_rdev: Cannot allocate memory for private data\n");
        return -RDEV_ERR_NOT_ENOUGH_RESOURCES;
    }

    return 0;
}

static int exa_rdev_release(struct inode *inode, struct file *filp)
{
    if (filp->private_data != NULL)
    {
	struct exa_rdev_file_struct *sfs =
	    (struct exa_rdev_file_struct *) filp->private_data;

	down(&sfs->sem_fd);

	if (sfs->st)
	{
	    exa_rdev_make_request_end(sfs->st);
	    exa_rdev_bdput(sfs->st);
	    vfree(sfs->st);
	}

	vfree(filp->private_data);
	filp->private_data = NULL;
    }
    return 0;
}

static struct file_operations exa_rdev_fops = {
    .read = exa_rdev_read,
    .write = exa_rdev_write,
#if HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl = exa_rdev_ioctl,
#else
    .ioctl = exa_rdev_ioctl,
#endif
    .open = exa_rdev_open,
    .release = exa_rdev_release
};

static int exa_rdev_major;

int init_module(void)
{
    int ret;
    ret = exa_rdev_bdev_cache_init();
    if (ret < 0)
	return ret;

    exa_rdev_fops.owner = THIS_MODULE;
    exa_rdev_major =
	register_chrdev(0, EXA_RDEV_MODULE_NAME, &exa_rdev_fops);
    if (exa_rdev_major < 0)
    {
	printk(KERN_ERR "Could not register exa_rdev char device\n");
	return exa_rdev_major;
    }

    return 0;
}

void cleanup_module(void)
{
    exa_rdev_bdev_cache_stop();
    unregister_chrdev(exa_rdev_major, EXA_RDEV_MODULE_NAME);
}

MODULE_AUTHOR("Seanodes <http://www.seanodes.com>");
MODULE_DESCRIPTION("Kernel API for Seanodes Exanodes modules");
MODULE_LICENSE("Proprietary");
