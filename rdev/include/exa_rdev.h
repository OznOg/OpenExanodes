/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXA_RDEV_H
#define EXA_RDEV_H

#include "common/include/exa_constants.h"
#include "os/include/os_inttypes.h"

#define EXA_RDEV_STATUS_OK   1
#define EXA_RDEV_STATUS_FAIL 2

typedef enum rdev_op {
    RDEV_OP_INVALID,
    RDEV_OP_READ,
    RDEV_OP_WRITE,
    RDEV_OP_WRITE_BARRIER
} rdev_op_t;

#define RDEV_OP_VALID(op) ((op) >= RDEV_OP_READ && (op) <= RDEV_OP_WRITE_BARRIER)

/* XXX RELOAD_DEVICE sounds like a command, not a status (?) */
typedef enum
{
#define RDEV_REQ_STATUS__FIRST  RDEV_REQUEST_NONE_ENDED
    RDEV_REQUEST_NONE_ENDED = 1,
    RDEV_REQUEST_ALL_ENDED,
    RDEV_REQUEST_END_ERROR,
    RDEV_REQUEST_END_OK,
    RDEV_REQUEST_NOT_ENOUGH_FREE_REQ,
    RDEV_RELOAD_DEVICE
#define RDEV_REQ_STATUS__LAST   RDEV_RELOAD_DEVICE
} rdev_req_status_t;
#define RDEV_REQ_STATUS_IS_VALID(status) \
    ((status) >= RDEV_REQ_STATUS__FIRST && (status) <= RDEV_REQ_STATUS__LAST)


#define EXA_RDEV_READ_WRITE_FRAGMENT 0x40000 /* 262144 */

/* max device opended */
#define EXA_RDEV_MAX_DEV_OPEN (NBMAX_DISKS_PER_NODE)

/** Rdev handler */
typedef struct exa_rdev_handle exa_rdev_handle_t;

/** Static data operation */
typedef enum
{
    RDEV_STATIC_CREATE,   /**< Create the static data */
    RDEV_STATIC_GET,      /**< Get a reference to (existing) static data */
    RDEV_STATIC_RELEASE,  /**< Release a reference to static data */
    RDEV_STATIC_DELETE    /**< Delete the static data */
} rdev_static_op_t;

#define RDEV_STATIC_OP_INVALID ((rdev_static_op_t)(RDEV_STATIC_DELETE + 1))

/** Check whether a static data operation is valid */
#define RDEV_STATIC_OP_VALID(op) \
    ((op) >= RDEV_STATIC_CREATE && (op) <= RDEV_STATIC_DELETE)

/**
 * Initialize the static rdev data.
 * *Must* be called prior to using any function of this module.
 *
 * @param[in] op  Operation to perform, either RDEV_STATIC_CREATE or
 *                RDEV_STATIC_GET.
 *
 * @return 0 if successful, -ENOMEM otherwise
 */
int exa_rdev_static_init(rdev_static_op_t op);

/**
 * Clean up the static rdev data.
 * *Must* be called when done with this module.
 *
 * @param[in] op  Operation to perform, either RDEV_STATIC_DELETE or
 *                RDEV_STATIC_RELEASE.
 *
 * @note RDEV_STATIC_DELETE (resp. RDEV_STATIC_RELEASE) can be used
 *       only if static data was initialized with RDEV_STATIC_CREATE
 *       (resp. RDEV_STATIC_GET).
 */
void exa_rdev_static_clean(rdev_static_op_t op);

#ifdef WIN32
#ifdef RDEV_TEST
/**
 * Get a "user friendly" name of an rdev status.
 *
 * @param[in] status  Rdev status to get the name of
 *
 * @return Name if status is valid, NULL otherwise
 */
const char *exa_rdev_status_name(rdev_req_status_t status);

/**
 * Get information about the RDEV at the specified index.
 *
 * @note Meant to be used in iterations, but the collated information
 * is *not* a snapshot of the internal RDEV table: the table may change
 * between calls to this function.
 *
 * @param[in]  index      Index of the RDEV to get information about
 * FIXME: a proper handle (exa_rdev_handle_t *) would be more suitable
 * @param[out] path       RDEV path
 * @param[in]  path_size  Size of the path buffer
 * @param[out] status     RDEV status
 * @param[out] use_count  RDEV use count
 *
 * @return 0 if successful, -ENOENT when the index is out of range,
 *         and -EINVAL if a parameter is invalid
 */
int exa_rdev_at_index(int index, char *path, size_t path_size,
                      rdev_req_status_t *status, int *use_count);

/**
 * Perform an operation on the status of an RDEV.
 *
 * @param[in]     path       Path of the RDEV
 * @param[in]     op         Operation: 'g' to get the status, 's' to set it
 * @param[in,out] status     Status read ('g' operation) or to set ('s' operation)
 * @param[out]    use_count  Use count read ('g' operation only, may be NULL
 *                           for the 's' operation)
 *
 * @return 0 if successful, a negative error code otherwise
 */
int exa_rdev_under_the_hood_do(const char *path, char op,
                               rdev_req_status_t *status,
                               int *use_count);
#endif  /* RDEV_TEST */
#endif  /* WIN32 */

/**
 * Initialise exa_rdev
 *
 * @return A file descriptor associated to exa_rdev if successfull, -1 otherwise
 */
int exa_rdev_init(void);

/**
 * Allocate and initialize a real device handler
 *
 * @param path  The path of the disk
 *
 * @return An exa_rdev handle describing the disk if successfull,
 *         NULL otherwise
 */
exa_rdev_handle_t *exa_rdev_handle_alloc(const char *path);

/**
 * Free a real device handler
 *
 * @param req  request structure
 */
void __exa_rdev_handle_free(exa_rdev_handle_t *handle);
#define exa_rdev_handle_free(handle) \
        (__exa_rdev_handle_free(handle), handle = NULL)

/**
 * Add a new request to exa_rdev processing queue
 *
 * @param op                   Operation to carry on data (read, write, ...)
 * @param[in,out] nbd_private  (IN) info that we will receive when this request will
 *                                  end
 *		               (OUT) if one request have ended, info we have sent
 *                                   with the request
 * @param sector                Offset on disk to read/write
 * @param sector_nb             Number of sectors to read/write
 * @param buffer                Buffer containing data to write or where to get data
 *                              upon read
 * @param req                   The exa_rdev handle describing the disk
 *
 * @return
 * EXA_RDEV_REQUEST_END_ERROR : the new processed request has been sent,
 *                              another request ended with error (private has
 *                              been updated with the info of this failed
 *                              request)
 * EXA_RDEV_REQUEST_END_OK : the new processed request has been sent, another
 *                           request ended with success (private has been
 *                           updated with the info of this successfull request)
 * EXA_RDEV_REQUEST_NONE_ENDED : the new processed request has been sent
 *
 * The new request has not been sent due to :
 *  -RDEV_ERR_NOT_ENOUGH_FREE_REQ : not enough free requests, retry after an
 *                                  exa_rdev_wait_one_request()
 *  EXA_RDEV_GENERIC_ERROR
 *  -RDEV_ERR_INVALID_DEVICE
 *  -RDEV_ERR_INVALID_BUFFER
 *  -RDEV_ERR_INVALID_OFFSET
 *  -RDEV_ERR_INVALID_SIZE
 *  -RDEV_ERR_BIG_ERROR
 *  -RDEV_ERR_NOT_SOCKET
 *  -RDEV_ERR_NOT_ENOUGH_RESOURCES
 *  -RDEV_ERR_UNKNOWN
 *  -RDEV_ERR_TOO_SMALL
 *  -RDEV_ERR_NOT_INITED
 *  -RDEV_ERR_INVALID_IOCTL
 */
int exa_rdev_make_request_new(rdev_op_t op, void **nbd_private,
			      unsigned long long sector,
			      int sector_nb, void *buffer,
			      exa_rdev_handle_t *handle);

/**
 * Returns the last error on the device associate to the handle.
 * This may return -RDEV_ERR_UNKNOWN if no IO was done recently.
 *
 * @param handle     The exa_rdev handle associated to the disk
 *
 * return -RDEV_ERR_UNKNOWN or RDEV_REQUEST_END_ERROR or RDEV_REQUEST_END_OK
 * FIXME the return value is really messed up.
 */
int exa_rdev_get_last_error(const exa_rdev_handle_t *req);

/**
 * Ask the drive associated to the handle to flush all its buffers (cache).
 * CAREFUL: the call is asynchronous, this means that upon return, the drive
 * may still be performing IOs. The caller MUST check after this call that
 * all IOs are actually completed.
 *
 * @param req     The exa_rdev handle describing the disk
 *
 * @return EXA_SUCCESS, error otherwise
 */
int exa_rdev_flush(exa_rdev_handle_t *handle);

/**
 * Synchronous read/write with exa_rdev.
 * Each big request is divided into blocks of EXA_RDEV_READ_WRITE_FRAGMENT size
 *
 * @param op      Operation to carry on data (read, write, ...)
 * @param start   Offset on disk (must be aligned with 4096)
 * @param size    Size to read/write (must be aligned with 4096)
 * @param buffer  Source/destination buffer
 * @param req     The exa_rdev handle describing the disk
 *
 * @return EXA_SUCCESS on success, error otherwise
 */
int exa_rdev_make_request(rdev_op_t op, unsigned long long start , int size,
			  void *buffer, exa_rdev_handle_t *handle);

/**
 * Wait the end of one request
 *
 * @param[in,out] nbd_private  (OUT) info associted with this request (sent with it with
 *                             exa_rdev_make_request_new)
 *                             (IN) != 0 mean only the request with private
 * @param start                Offset on the disk
 * @param size                 Size of data to read/write for the current request
 * @param buffer               Buffer that will be used for the current request
 * @param req                  The exa_rdev handle describing the disk
 *
 * @return EXA_RDEV_REQUEST_END_OK    : one request ended successfully *private
 *                                      is the info associted to this request
 *	   EXA_RDEV_REQUEST_END_ERROR : one request ended with error  *private
 *	                                is the info associted to this request
 *	   EXA_RDEV_REQUEST_ALL_ENDED : no more request "in progress"
 *	   EXA_RDEV_REQUEST_NONE_ENDED: no request find, retry ?
 */
int exa_rdev_wait_one_request(void **nbd_private,
			      exa_rdev_handle_t *handle);

/**
 * Test if a disk is still usable or not
 *
 * @param req     The exa_rdev handle describing the disk
 * @param buffer  Buffer used by the request (only used on Windows)
 * @param size    Size of the buffer (only used on Windows)
 *
 * @return EXA_SUCCESS : ok
 */
int exa_rdev_test(exa_rdev_handle_t *handle, void *buffer, int size);

/**
 * Activate a broken disk
 *
 * @param req  The exa_rdev handle describing the disk
 *
 * @return
 */
int exa_rdev_activate(exa_rdev_handle_t *handle);

/**
 * Dectivate a disk
 *
 * @param req   The exa_rdev handle describing the disk
 * @param path  The path of the disk to deactivate
 * FIXME: 'path' is dupplicate 'req' identify the disk clearly enough
 *
 * @return
 */
int exa_rdev_deactivate(exa_rdev_handle_t *handle, char *path);

#endif /* EXA_RDEV_H */
