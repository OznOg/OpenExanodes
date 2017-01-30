/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "rdev/include/exa_rdev.h"
#include "rdev/src/rdev_perf.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "os/include/os_mem.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libaio.h"

struct exa_rdev_handle
{
    io_context_t ctx;
    int fd; /* file descriptor opened on the device */
    size_t io_count; /* number of outstanding IOs */
#ifdef WITH_PERF
    rdev_perfs_t rdev_perfs;
#endif
};

#define RDEV_LIBAIO_MAX_REQUEST 128

static rdev_static_op_t init_op = RDEV_STATIC_OP_INVALID;

int exa_rdev_static_init(rdev_static_op_t op)
{
    EXA_ASSERT_VERBOSE(init_op == RDEV_STATIC_OP_INVALID, "static data already initialized");

    EXA_ASSERT_VERBOSE(op == RDEV_STATIC_CREATE || op == RDEV_STATIC_GET,
                       "invalid static init op: %d", op);

    init_op = op;

    return 0;
}

void exa_rdev_static_clean(rdev_static_op_t op)
{
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

    init_op = RDEV_STATIC_OP_INVALID;
}

exa_rdev_handle_t *exa_rdev_handle_alloc(const char *path)
{
    exa_rdev_handle_t *handle;
    int err;

    handle = os_malloc(sizeof(exa_rdev_handle_t));
    if (handle == NULL)
	return NULL;

    handle->fd = open(path, O_RDWR | O_DIRECT);
    if (handle->fd < 0) {
	os_free(handle);
        return NULL;
    }
    handle->io_count = 0;

    /* Carful, libaio documentation requires ctx to be set to 0 prior calling
     * io_setup(). */
    memset(&handle->ctx, 0, sizeof(handle->ctx));

    err = io_setup(RDEV_LIBAIO_MAX_REQUEST, &handle->ctx);
    if (err < 0) {
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

    /* Ignore error here: what could we do? */
    io_destroy(handle->ctx);

    os_free(handle);
}

int exa_rdev_flush(exa_rdev_handle_t *handle)
{
  if (handle == NULL)
    return -1;

  while (exa_rdev_wait_one_request(NULL, handle) != RDEV_REQUEST_ALL_ENDED)
	  ;
  /* FIXME how to do that? */
  return 0;
}


int exa_rdev_make_request_new(rdev_op_t op, void **nbd_private,
			      unsigned long long sector, int sector_nb,
			      void *buffer, exa_rdev_handle_t *handle)
{
  struct iocb cb;
  struct iocb *cbs[1];
  int err;

  if (handle == NULL)
      return -1;

  memset(&cb, 0, sizeof(cb));
  cb.aio_fildes = handle->fd;

  switch (op) {
  case RDEV_OP_READ:
       cb.aio_lio_opcode = IO_CMD_PREAD;
       break;
  case RDEV_OP_WRITE:
  case RDEV_OP_WRITE_BARRIER:
       /* FIXME how to implement FUA (ok here it is barriere, but what should
	* be here is FUA)? There does not seem to be a FUA interface in libaio */
       cb.aio_lio_opcode = IO_CMD_PWRITE;
       break;
  case RDEV_OP_INVALID:
       EXA_ASSERT(RDEV_OP_VALID(op));
  }

  cb.data = *nbd_private;

  /* command-specific options */
  cb.u.c.buf = buffer;
  cb.u.c.offset = sector * SECTOR_SIZE;
  cb.u.c.nbytes = sector_nb * SECTOR_SIZE;

  cbs[0] = &cb;

  err = io_submit(handle->ctx, 1, cbs);
  switch (err) {
  case 1: /* expected behaviour */
      handle->io_count++;
      return RDEV_REQUEST_NONE_ENDED;

  case -EAGAIN:
      return RDEV_REQUEST_NOT_ENOUGH_FREE_REQ;

  default: /* FIXME print something indicating the error? */
      return RDEV_REQUEST_END_ERROR;
  }

}

int exa_rdev_wait_one_request(void **nbd_private, exa_rdev_handle_t *handle)
{
    struct io_event event;
    int ret;

    if (handle == NULL)
	return -RDEV_ERR_NOT_OPEN;

    if (!handle->io_count)
        return RDEV_REQUEST_ALL_ENDED;

    /* get the reply */
    do {
        ret = io_getevents(handle->ctx, 1, 1, &event, NULL /* wait forever */);
    } while (ret == -EINTR);

    if (ret != 1) /* an error occured */
        return RDEV_REQUEST_END_ERROR;

    /* A request ended successfully */
    handle->io_count--;

    if (nbd_private != NULL)
        *nbd_private = event.data;

    return RDEV_REQUEST_END_OK;
}

int exa_rdev_get_last_error(const exa_rdev_handle_t *handle)
{
    return -RDEV_ERR_UNKNOWN;
}

int exa_rdev_activate(exa_rdev_handle_t *handle)
{
    return 0;
}

int exa_rdev_deactivate(exa_rdev_handle_t *handle, char *path)
{
    return 0;
}

