/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
# ifndef major
#   include <sys/sysmacros.h>
# endif

#include "common/include/exa_error.h"
#include "os/include/os_mem.h"
#include "common/include/exa_names.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"

#include "os/include/os_dir.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_kmod.h"
#include "os/include/os_thread.h"
#include "os/include/strlcpy.h"

#include "rdev/include/exa_rdev.h"
#include "rdev/src/rdev_kmodule.h"
#include "rdev/src/rdev_perf.h"

#include "log/include/log.h"

#define EXA_RDEV_MODULE_PATH "/dev/" EXA_RDEV_MODULE_NAME

struct exa_rdev_handle
{
    int fd;
#ifdef WITH_PERF
    rdev_perfs_t rdev_perfs;
#endif
};

static rdev_static_op_t init_op = RDEV_STATIC_OP_INVALID;

static int __ioctl_nointr(int fd, int op, void *param)
{
    int err;
    do {
        err = ioctl(fd, op, param);
        /* While loop is mandatory to make sure that the operation is really
         * done and not just interrupted. */
    } while (err == -1 && errno == EINTR);

    return err < 0 ? -errno : err;
}

int exa_rdev_static_init(rdev_static_op_t op)
{
    int err;
    EXA_ASSERT_VERBOSE(init_op == RDEV_STATIC_OP_INVALID, "static data already initialized");

    EXA_ASSERT_VERBOSE(op == RDEV_STATIC_CREATE || op == RDEV_STATIC_GET,
                       "invalid static init op: %d", op);

    err = os_kmod_load(EXA_RDEV_MODULE_NAME);
    if (err != 0)
    {
        exalog_error("Failed to load kernel module '" EXA_RDEV_MODULE_NAME "': %s (%d)",
                     os_strerror(-err), err);
        return err;
    }

    init_op = op;

    return 0;
}

void exa_rdev_static_clean(rdev_static_op_t op)
{
    int err;
    /* Initialization not performed, nothing to clean */
    if (init_op == RDEV_STATIC_OP_INVALID)
        return;

    EXA_ASSERT_VERBOSE(op == RDEV_STATIC_RELEASE || op == RDEV_STATIC_DELETE,
               "invalid static clean op: %d", op);

    if (op == RDEV_STATIC_DELETE)
    {
        EXA_ASSERT_VERBOSE(init_op == RDEV_STATIC_CREATE,
                           "deletion of static data by non-owner");
    }
    else /* RDEV_STATIC_RELEASE */
    {
        EXA_ASSERT_VERBOSE(init_op == RDEV_STATIC_GET,
                           "release of static data by owner");
    }

    err = os_kmod_unload("exa_rdev");
    if (err != 0)
        exalog_warning("Failed to unload kernel module '" EXA_RDEV_MODULE_NAME "': %s (%d)",
                       os_strerror(-err), err);

    init_op = RDEV_STATIC_OP_INVALID;
}

int exa_rdev_init(void)
{
    int fd;

    fd = open(EXA_RDEV_MODULE_PATH, O_RDWR);

    return fd;
}

exa_rdev_handle_t *exa_rdev_handle_alloc(const char *path)
{
    struct exa_rdev_major_minor majmin;
    exa_rdev_handle_t *handle;
    struct stat sstat;

    handle = os_malloc(sizeof(exa_rdev_handle_t));
    if (handle == NULL)
	return handle;

    if (stat(path, &sstat) < 0)
    {
	os_free(handle);
	return NULL;
    }

    if (S_ISDIR(sstat.st_mode))
    {
	os_free(handle);
	return NULL;
    }

    if (sstat.st_rdev == sstat.st_dev)
    {
	os_free(handle);
	/* refusing to directly access to / */
	return NULL;
    }

    handle->fd = open(EXA_RDEV_MODULE_PATH, O_RDWR);
    if (handle->fd <= 0)
    {
	os_free(handle);
	return NULL;
    }

    majmin.major = major(sstat.st_rdev);
    majmin.minor = minor(sstat.st_rdev);

    if (__ioctl_nointr(handle->fd, EXA_RDEV_INIT, &majmin) < 0)
    {
	close(handle->fd);
	os_free(handle);
	return NULL;
    }

    rdev_perf_init(&handle->rdev_perfs, path);

    return handle;
}

void __exa_rdev_handle_free(exa_rdev_handle_t *handle)
{
    if (handle == NULL)
        return;

    close(handle->fd);

    os_free(handle);
}

int exa_rdev_flush(exa_rdev_handle_t *handle)
{
  if (handle == NULL)
    return -1;

  return __ioctl_nointr(handle->fd, EXA_RDEV_FLUSH, NULL);
}


int exa_rdev_make_request_new(rdev_op_t op, void **nbd_private,
			      unsigned long long sector, int sector_nb,
			      void *buffer, exa_rdev_handle_t *handle)
{
  struct exa_rdev_request_kernel req;
  int err;

  EXA_ASSERT(RDEV_OP_VALID(op));

  if (handle == NULL)
    return -1;

  req.sector_nb     = sector_nb;
  req.buffer        = buffer;
  req.sector        = sector;
  req.op            = op;

  /* Careful: h is kept by value in kernel space, that's why the req
   * can be destroyed when leaving this function */
  req.h.nbd_private = *nbd_private;

  COMPILE_TIME_ASSERT(sizeof(rdev_req_perf_t) == sizeof(req.h.private_perf_data));

  rdev_perf_make_request(&handle->rdev_perfs, op == RDEV_OP_READ,
                         (rdev_req_perf_t *)&req.h.private_perf_data);

  err = __ioctl_nointr(handle->fd, EXA_RDEV_MAKE_REQUEST_NEW, &req);

  *nbd_private = req.h.nbd_private;

  if (req.h.nbd_private != NULL)
    rdev_perf_end_request(&handle->rdev_perfs,
                          (rdev_req_perf_t *)&req.h.private_perf_data);

  return err;
}

int exa_rdev_wait_one_request(void **nbd_private,
                              exa_rdev_handle_t *handle)
{
    user_land_io_handle_t h;
    int err;

    if (handle == NULL)
	return -RDEV_ERR_NOT_OPEN;

    if (nbd_private == NULL)
        return -EINVAL;

    *nbd_private = NULL;

    err = __ioctl_nointr(handle->fd, EXA_RDEV_WAIT_ONE_REQUEST, &h);

    *nbd_private = h.nbd_private;

    if (h.nbd_private != NULL)
        rdev_perf_end_request(&handle->rdev_perfs,
                              (rdev_req_perf_t *)&h.private_perf_data);

    return err;
}

int exa_rdev_get_last_error(const exa_rdev_handle_t *handle)
{
    int state = __ioctl_nointr(handle->fd, EXA_RDEV_GET_LAST_ERROR, NULL);

    /* for compatibility */
    if (state == RDEV_REQUEST_END_OK || state == RDEV_REQUEST_END_ERROR)
	return state;

    return -RDEV_ERR_UNKNOWN;
}

int exa_rdev_activate(exa_rdev_handle_t *handle)
{
    return __ioctl_nointr(handle->fd, EXA_RDEV_ACTIVATE, NULL);
}

int exa_rdev_deactivate(exa_rdev_handle_t *handle, char *path)
{
    return __ioctl_nointr(handle->fd, EXA_RDEV_DEACTIVATE, NULL);
}

