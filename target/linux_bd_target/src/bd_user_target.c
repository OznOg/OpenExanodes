/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "target/include/target_adapter.h"
#include "lum/export/include/executive_export.h"
#include "target/linux_bd_target/include/bd_user_perf.h"

#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/threadonize.h"

#include "log/include/log.h"

#include "os/include/os_dir.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_kmod.h"
#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "os/include/os_thread.h"

#include <sys/mman.h> /* mmap */
#include <linux/fs.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>

static volatile bool bd_target_run;

struct tab_session
{
    /* User structure pointer get by kernel mmap This structure must be read only */
    struct bd_kernel_queue *bd_kernel_queue;
    /* read/write */
    struct bd_user_queue *bd_user_queue;
    int bd_fd;
    int bd_buffer_size;
    int bd_page_size;
    int bd_max_queue;

    /* FIXME What the hell does this comment mean?? -> "used to don't have a
             device that be suspended/down/removed/resumed
       during a __bdget_new_request() or a other function it" */
    os_thread_rwlock_t change_state;

    struct bd_barrillet_queue bd_ack_request;
    struct bd_barrillet_queue bd_new_request;
};

static struct
{
    struct tab_session *session;
    int major;
    os_thread_mutex_t lock;
} private_data;

/* we use a false mutex to force a memory barrier */
static os_thread_mutex_t fake_mutex = PTHREAD_MUTEX_INITIALIZER;
#define MB() os_thread_mutex_trylock(&fake_mutex)

#define LAST_MINOR  (MAX_BD_MINOR - 1)

static lum_export_t *minors2export[LAST_MINOR + 1];

/* TODO this function is duplicated from exa_rdev and would deserve to be
 * merged. Actually, the whole exa_bd and exa_rdev should probably be merged... */
static int __ioctl_nointr(int fd, int op, uint64_t param)
{
    int err;
    do {
        err = ioctl(fd, op, param);
        /* While loop is mandatory to make sure that the operation is really
         * done and not just interrupted. */
    } while (err == -1 && errno == EINTR);

    return err < 0 ? -errno : err;
}

static int export_bdev_register(lum_export_t *export)
{
    int minor;

    for (minor = 0; minor <= LAST_MINOR; minor++)
        if (minors2export[minor] == NULL)
        {
            minors2export[minor] = export;
            return minor;
        }

    return -1;
}

static int export_bdev_find_minor(const lum_export_t *export)
{
    int minor;

    for (minor = 0; minor <= LAST_MINOR; minor++)
        if (minors2export[minor] == export)
            return minor;

    return -1;
}

static void export_bdev_release_minor(int minor)
{
    EXA_ASSERT(minors2export[minor] != NULL);

    minors2export[minor] = NULL;
}

/**
 * Create an UNIX block device path under /dev associated to a group:volume.
 *
 * @param[in]   path    Path of the device in /dev
 * @param[in]   major
 * @param[in]   minor
 *
 * @return EXA_SUCCESS if successful, negative error otherwise.
 */
static int export_bdev_create(const char *path, int major, int minor)
{
    char _path[EXA_MAXSIZE_LINE + 1];
    char *dir;
    mode_t mode = S_IFBLK | S_IRUSR | S_IWUSR;
    struct stat st;
    int err;

    strlcpy(_path, path, sizeof(_path));

    dir = os_dirname(_path);
    err = os_dir_create_recursive(dir);
    if (err)
        return err;

    /* An old device may be left by Exanodes when it crashes.
       As the major/minor may change we delete and recreate it.
       FIXME: we should also test if another volume has the same maj/min
     */
    if (stat(path, &st) == 0)
        unlink(path);

    if (mknod(path, mode, makedev(major, minor)) == -1)
    {
        exalog_error("Cannot create device '%s' with major,minor %d,%d: %s (%d)",
                     path, major, minor, os_strerror(errno), -errno);
        return -errno;
    }

    return EXA_SUCCESS;
}

/** Remove an UNIX block device path under /dev in the form of group/volume.
 *
 * @param[in] path  dev entry name
 *
 * @return          EXA_SUCCESS if successful, negative error otherwise.
 */
static int export_bdev_delete(const char *path)
{
    int err;
    char _path[EXA_MAXSIZE_LINE + 1];
    char *dir;

    if (unlink(path) == -1 && errno != ENOENT)
    {
        exalog_error("Cannot remove device '%s': %s (%d)", path,
                     os_strerror(errno), -errno);
        return -errno;
    }

    strlcpy(_path, path, sizeof(_path));

    dir = os_dirname(_path);

    /* If the group directory is empty we also remove it */
    err = os_dir_remove(dir);
    if(err != 0 && err != -ENOTEMPTY)
    {
        exalog_error("Cannot remove directory '%s': %s (%d)", dir,
                     os_strerror(errno), -errno);
        return -errno;
    }

    return EXA_SUCCESS;
}

static struct bd_init init;
static os_thread_t linux_blockdevice_io_thread;

static bool checknum(struct tab_session *session, int num)
{
    /* Just some sanity check, but these errors can't be checked correctly */
    /* if this function is called from several threads at the same time */
    if (num <= 0 /* FIXME 0 stands for BD_FREE_QUEUE but only
                  * defined in kernel space */
        || num >= session->bd_max_queue)
        return false;

    if (session->bd_kernel_queue[num].bd_use == BDUSE_FREE)
        return false;

    return true;
}

/**
 * Free all data needed by a session
 * @param session
 * @return
 */
static int __bdend(struct tab_session *session)
{
    bd_target_run = false;
    /* Wake __bdget_new_request */
    __ioctl_nointr(session->bd_fd, BD_IOCTL_CLEANUP, 0);
    os_thread_join(linux_blockdevice_io_thread);

    munmap(session->bd_kernel_queue, 2 * session->bd_page_size);
    munmap(session->bd_user_queue,   3 * session->bd_page_size);

    close(session->bd_fd);

    os_aligned_free(init.buffer);
    os_free(session);

    return 0;
}

/**
 * Create a session
 * @param[out] Major major number of all the device that will be manage in
 * this session
 * @return addresse of a session, NULL if error
 */
static struct tab_session *__bdinit(int *major, int bd_buffer_size,
                                    int bd_max_queue, int bd_barrier_enable)
{
    struct bd_barrillet_queue *ack_rq, *new_rq;

    struct tab_session *session;
    int file = open("/dev/exa_bd", O_RDWR);

    EXA_ASSERT(major);

    if (file < 0)
        return NULL;

    session = (struct tab_session *) os_malloc(sizeof(struct tab_session));
    if (session == NULL)
    {
        close(file);
        return NULL;
    }

    session->bd_buffer_size = bd_buffer_size;
    session->bd_max_queue   = bd_max_queue;

    init.buffer = os_aligned_malloc(
        session->bd_buffer_size * session->bd_max_queue,
        4096 /* FIXME hardcoded */,
	NULL);
    if (init.buffer == NULL)
    {
        os_free(session);
        close(file);
        return NULL;
    }

    init.bd_buffer_size    = bd_buffer_size;
    init.bd_max_queue      = bd_max_queue;
    init.bd_barrier_enable = bd_barrier_enable;

    *major = __ioctl_nointr(file, BD_IOCTL_INIT, (uint64_t)&init);
    if (*major <= 0)
    {
        close(file);
        os_aligned_free(init.buffer);
        os_free(session);
        return NULL;
    }

    session->bd_page_size = init.bd_page_size;
    session->bd_fd = file;
    session->bd_kernel_queue = mmap(NULL, 2 * session->bd_page_size,
                                    PROT_READ, MAP_SHARED, file, 0);
    session->bd_user_queue   = mmap(NULL, 3 * session->bd_page_size,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED, file, 0);
    if (session->bd_user_queue == MAP_FAILED
        || session->bd_kernel_queue == MAP_FAILED)
    {
        exalog_debug("mmap of bd_user structures failed\n");
        perror("ExaBDInit: Memory mapping failed\n"); /* This is really an interesting error to be print... */
        close(file);    /* It also kill the session */
        os_free(session);
        return NULL;
    }

    new_rq = &session->bd_new_request;
    ack_rq = &session->bd_ack_request;

    /* FIXME the next cast seems really really strange... */
    new_rq->last_index_add = (int *) &(session->bd_user_queue[bd_max_queue]);
    new_rq->last_index_read_plus_one = new_rq->last_index_add + 1;
    new_rq->next_elt = new_rq->last_index_add + 2;

    ack_rq->last_index_add = new_rq->last_index_add + 2 + bd_max_queue;
    ack_rq->last_index_read_plus_one = new_rq->last_index_add + 2 +
                                       bd_max_queue + 1;
    ack_rq->next_elt = new_rq->last_index_add + 2 + bd_max_queue + 2;

    os_thread_rwlock_init(&session->change_state);

    return session;
}

/**
 * Create a new device and left it to suspend mode
 * @param session target session
 * @param minor minor of this new device
 * @param size_in_sector size of this new device devide byt 512 bytes
 * @param name of this new device (can be see in /sys/block with Linux 2.6)
 * @return 0 if success
 */
static int __bdnew_minor(struct tab_session *session,
                           int minor, long size_in_sector,
                           const char *name, bool readonly)
{
    struct bd_new_minor bd_minor;

    bd_minor.bd_minor         = minor;
    bd_minor.size_in512_bytes = size_in_sector;
    bd_minor.readonly         = readonly;

    exalog_debug("NewMinor %d size %ld\n", minor, size_in_sector);

    return __ioctl_nointr(session->bd_fd, BD_IOCTL_NEWMINOR, (uint64_t)&bd_minor);
}

/**
 * Set size of an already created device
 * @param session target session
 * @param minor minor of this new device
 * @param size_in_sector new size of this device devide byt 512 bytes
 * @return 0 if success
 */
static int __bdminor_set_size(struct tab_session *session, int minor,
                                long size_in_sector)
{
    /* FIXME Use a specific structure in order not to squat the
     * bd_new_minor struct... */
    struct bd_new_minor bd_minor;

    bd_minor.bd_minor         = minor;
    bd_minor.size_in512_bytes = size_in_sector;
    bd_minor.readonly         = true; /* FIXME Seems useless */

    exalog_debug("Setsize %ld:%d\n", size_in_sector, minor);

    return __ioctl_nointr(session->bd_fd, BD_IOCTL_SETSIZE, (uint64_t)&bd_minor);
}

/**
 * Remove a device
 * @param session target session
 * @param minor minor of the device to remove
 * @return -1 if error otherwise status:
 *            BDMINOR_DOWN, BDMINOR_UP or BDMINOR_SUSPEND
 */
static int __bdremove_minor(struct tab_session *session, int minor)
{
    int ret;

    os_thread_rwlock_wrlock(&session->change_state);
    ret = __ioctl_nointr(session->bd_fd, BD_IOCTL_DELMINOR, minor);
    os_thread_rwlock_unlock(&session->change_state);

    /* XXX I cannot figure out what this function is supposed to return, so I
     * add this assert here.... */
    EXA_ASSERT(ret == -1 || ret == 0);
    return ret;
}

/**
 * Get a new request sent to this session by the kernel
 * @param session target session
 * @param[out] queue containing info on the request (number, size of data, ...)
 * @param[out] Buffer buffer to store data
 * @return 0 success, -1 if cannot find a new request.
 */
static bool __bdget_new_request(struct tab_session *session, long *queue_index)
{
    /* Li state for last i ! */
    int li = -1;

    while (li == -1 && bd_target_run)
    {
	os_thread_rwlock_rdlock(&session->change_state);

	li = session->bd_new_request.next_elt[*session->bd_new_request.
					      last_index_read_plus_one];
	if (li == -1)
	{
	    os_thread_rwlock_unlock(&session->change_state);
	    if (__ioctl_nointr(session->bd_fd, BD_IOCTL_SEM_WAIT, 0) != 0)
		return false;
	}
    }

    if (!bd_target_run)
    {
	os_thread_rwlock_unlock(&session->change_state);
	return false;
    }

    session->bd_new_request.next_elt[*session->bd_new_request.
                                     last_index_read_plus_one] = -1;
    MB();
    *session->bd_new_request.last_index_read_plus_one =
        (*session->bd_new_request.last_index_read_plus_one +
         1) % session->bd_max_queue;

    *queue_index = li;
    os_thread_rwlock_unlock(&session->change_state);

    return true;
}

/**
 * ending a request
 * @param session session
 * @param num request number, -1 mean we don't end any request
 * @param status status of the ended request
 * @return 0 ok
 *         -1 error request number unknown or already ended
 */
static inline void __bdend_request(struct tab_session *session,
                                   int num, int status)
{
    os_thread_rwlock_rdlock(&session->change_state);

    EXA_ASSERT(checknum(session, num));

    session->bd_user_queue[num].bd_result = status;
    BDEV_TARGET_PERF_END_REQUEST(session->bd_kernel_queue[num].bd_op,
                                 &session->bd_user_queue[num]);
    MB();
    session->bd_ack_request.next_elt[*session->bd_ack_request.last_index_add] = num;
    *session->bd_ack_request.last_index_add =
        (*session->bd_ack_request.last_index_add + 1) % session->bd_max_queue;

    os_thread_rwlock_unlock(&session->change_state);

    exalog_debug("bd_user : will end %d request with status %d\n", num, status);

    __ioctl_nointr(session->bd_fd, BD_IOCTL_SEM_POST, 0); /* 1 or 2 */
}

static int adapter_static_init(exa_nodeid_t node_id /* unused */)
{
    int err;

    BDEV_TARGET_PERF_INIT();

    err = os_kmod_load("exa_bd");
    if (err != 0)
        exalog_error("Failed to load kernel module 'exa_bd'");

    return err;
}

static int adapter_static_cleanup(void)
{
    BDEV_TARGET_PERF_CLEANUP();

    return os_kmod_unload("exa_bd");
}

static void thread_submit_io(void *arg);

static bool bdev_init_done = false;

static int adapter_init(const lum_init_params_t *params)
{
    struct tab_session *session;

    bdev_init_done = false;

    os_thread_mutex_init(&private_data.lock);

    session = __bdinit(&private_data.major, 128 * 1024L, params->bdev_queue_depth, 1);
    if (session == NULL)
        return -EXA_ERR_DEFAULT;

    private_data.session = session;

    bd_target_run = true;

    exathread_create(&linux_blockdevice_io_thread, 8192, thread_submit_io, NULL);

    bdev_init_done = true;

    return EXA_SUCCESS;
}

static int adapter_cleanup(void)
{
    int ret;

    if (!bdev_init_done)
        return 0;

    os_thread_mutex_lock(&private_data.lock);
    ret = __bdend(private_data.session);
    os_thread_mutex_unlock(&private_data.lock);

    bdev_init_done = false;

    return ret;
}

/* functions used by exa_bd_server */
static int adapter_signal_new_export(lum_export_t *lum_export, uint64_t size)
{
    export_t *export;
    bool readonly;
    int minor, err;

    EXA_ASSERT(lum_export != NULL);

    export = lum_export_get_desc(lum_export);
    EXA_ASSERT(export_get_type(export) == EXPORT_BDEV);

    minor = export_bdev_register(lum_export);

    if (minor == -1)
        return -NBD_ERR_CANT_GET_MINOR;

    readonly = export_is_readonly(export);

    /* FIXME Should take private_data.lock(?) (There was no locking here
             since the origin) */
    __bdnew_minor(private_data.session, minor, 0, "Not Used", readonly);
    __bdminor_set_size(private_data.session, minor, size);

    err = export_bdev_create(export_bdev_get_path(export),
                             private_data.major, minor);
    if (err != EXA_SUCCESS)
    {
        /* FIXME There is no callback to export plugin to tell initialization
         * failed...
         * I postpone the fix as the original code was like that. */
        export_bdev_release_minor(minor);
        __bdremove_minor(private_data.session, minor);
    }

    return err;
}

static int adapter_signal_export_update_iqn_filters(const lum_export_t *lum_export)
{
    EXA_ASSERT_VERBOSE(false, "The IQN filters are irrelevant in bdev.");

    return EXA_SUCCESS;
}

static void adapter_export_set_size(lum_export_t *export, uint64_t size_in_sector)
{
    int minor = export_bdev_find_minor(export);

    EXA_ASSERT(MINOR_IS_VALID(minor));

    __bdminor_set_size(private_data.session, minor, size_in_sector);
}

/**
 * Test if an export is in use
 * @param export:   the export to test
 * return EXPORT_IN_USE if in use EXPORT_NOT_IN_USE else.
 */
static lum_export_inuse_t adapter_export_get_inuse(const lum_export_t *export)
{
    int minor = export_bdev_find_minor(export);

    EXA_ASSERT(MINOR_IS_VALID(minor));

    /* FIXME there is a real mixup between __ioctl_nointr return status and the return value
     * of the operation... fe, if 0 is returned, what does it mean ?*/
    return __ioctl_nointr(private_data.session->bd_fd, BD_IOCTL_IS_INUSE, minor) == 1 ?
	      EXPORT_IN_USE : EXPORT_NOT_IN_USE;
}

static int adapter_signal_remove_export(const lum_export_t *export)
{
    export_t *desc;
    int minor;
    int ret;

    EXA_ASSERT(export != NULL);

    /* FIXME there is a race here between the moment we check for in use
     * and the moment when we start removing the export.
     * IOs should be refused from the moment it is decided to stop. (by some
     * kind of suspend mechanism or else...) */
    if (adapter_export_get_inuse(export) == EXPORT_IN_USE)
        return -VRT_ERR_VOLUME_IS_IN_USE;

    minor = export_bdev_find_minor(export);
    if (minor == -1)
        return -ENOENT;

    /* FIXME I do not know what is the 'SUCCESS' return value of this
     * function...*/
    ret = __bdremove_minor(private_data.session, minor);
    if (ret != 0)
    {
        exalog_error("Cannot unregister device in exa_bdev");
        return -VRT_ERR_VOLUME_IS_IN_USE;
    }

    export_bdev_release_minor(minor);

    desc = lum_export_get_desc(export);

    return export_bdev_delete(export_bdev_get_path(desc));
}

static void disk_end_io(int err, void *data)
{
    long req_num = (long) data; /* FIXME bad cast... */
    struct tab_session *session = private_data.session;

    __bdend_request(session, req_num, err);
}

static void thread_submit_io(void *arg)
{
    while (bd_target_run)
    {
        struct bd_kernel_queue *kernel_q;
        struct bd_user_queue *user_q;
        long queue_index;

        blockdevice_io_type_t bio_type;
        bool flush_cache;

        if (!__bdget_new_request(private_data.session, &queue_index))
	    continue;

        /* FIXME the lock is taken after the while loop... quite strange... */
        os_thread_mutex_lock(&private_data.lock);

        user_q   = &private_data.session->bd_user_queue[queue_index];
        kernel_q = &private_data.session->bd_kernel_queue[queue_index];

        os_thread_mutex_unlock(&private_data.lock);

        if (kernel_q->bd_op == 0) /* FIXME: We should use a define (maybe READ) instead of 0 */
            bio_type = BLOCKDEVICE_IO_READ;
        else
            bio_type = BLOCKDEVICE_IO_WRITE;

        flush_cache = (user_q->bd_info & BD_INFO_BARRIER) != 0;

        /* FIXME The lock is relesaed but private data is still accessed
         *        Is this really what is expected ? */
        lum_export_submit_io(minors2export[kernel_q->bd_minor],
                             bio_type, flush_cache,
                             kernel_q->bd_blk_num,
                             kernel_q->bd_size_in_sector << 9,
                             kernel_q->bd_buf_user,
                             (void *)queue_index, disk_end_io);

        BDEV_TARGET_PERF_MAKE_REQUEST(kernel_q->bd_op, user_q,
                                      (kernel_q->bd_size_in_sector << 9) / 1024);
    }
}

static int __check_device(const char *devpath)
{
    struct stat info;

    if (stat(devpath, &info) < 0)
        return -errno;

    if (!S_ISBLK(info.st_mode))
        return -EXA_ERR_NOT_BLOCK_DEV;

    return EXA_SUCCESS;
}

static int adapter_set_readahead(const lum_export_t *lum_export, uint32_t readahead)
{
    export_t *desc = lum_export_get_desc(lum_export);
    const char *path;
    unsigned long readahead_sectors;
    int fd;
    int ret;

    if (readahead > UINT32_MAX / 2)
        return -ERANGE;

    readahead_sectors = readahead * 2;

    path = export_bdev_get_path(desc);

    ret = __check_device(path);
    if (ret != EXA_SUCCESS)
        return ret;

    if ((fd = open(path, O_RDONLY)) == -1)
        return -errno;

    if (__ioctl_nointr(fd, BLKRASET, readahead_sectors) == -1)
    {
        close(fd);
        return -errno;
    }

    close(fd);

    return EXA_SUCCESS;
}

static void adapter_suspend(void)
{
    /* Nothing to do here, for now */
}

static void adapter_resume(void)
{
    /* Nothing to do here, for now */
}

static target_adapter_t target_adapter =
{
    .static_init                      = adapter_static_init,
    .static_cleanup                   = adapter_static_cleanup,
    .init                             = adapter_init,
    .cleanup                          = adapter_cleanup,
    .signal_new_export                = adapter_signal_new_export,
    .signal_remove_export             = adapter_signal_remove_export,
    .signal_export_update_iqn_filters = adapter_signal_export_update_iqn_filters,
    .export_set_size                  = adapter_export_set_size,
    .export_get_inuse                 = adapter_export_get_inuse,
    .set_readahead                    = adapter_set_readahead,
    .suspend                          = adapter_suspend,
    .resume                           = adapter_resume,
    .set_mship                        = NULL,
    .set_peers                        = NULL,
    .set_addresses                    = NULL,
    .start_target                     = NULL,
    .stop_target                      = NULL
};

const target_adapter_t * get_bdev_adapter(void)
{
    return &target_adapter;
}
