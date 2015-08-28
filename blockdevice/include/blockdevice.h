/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include "os/include/os_completion.h"
#include "os/include/os_inttypes.h"

/** Block device access modes */
typedef enum
{
    BLOCKDEVICE_ACCESS_READ,  /**< Read only */
    BLOCKDEVICE_ACCESS_WRITE, /**< Write only */
    BLOCKDEVICE_ACCESS_RW     /**< Read and write */
} blockdevice_access_t;

#define BLOCKDEVICE_ACCESS__FIRST  BLOCKDEVICE_ACCESS_READ
#define BLOCKDEVICE_ACCESS__LAST   BLOCKDEVICE_ACCESS_RW

#define BLOCKDEVICE_ACCESS_IS_VALID(a) \
    ((a) >= BLOCKDEVICE_ACCESS__FIRST && (a) <= BLOCKDEVICE_ACCESS__LAST)

/** Block device IO types */
typedef enum
{
    BLOCKDEVICE_IO_READ,
    BLOCKDEVICE_IO_WRITE
} blockdevice_io_type_t;

#define BLOCKDEVICE_IO_TYPE__FIRST  BLOCKDEVICE_IO_READ
#define BLOCKDEVICE_IO_TYPE__LAST   BLOCKDEVICE_IO_WRITE

#define BLOCKDEVICE_IO_TYPE_IS_VALID(t) \
    ((t) >= BLOCKDEVICE_IO_TYPE__FIRST && (t) <= BLOCKDEVICE_IO_TYPE__LAST)

/** Generic blockdevice */
typedef struct blockdevice blockdevice_t;

typedef struct __blockdevice_io blockdevice_io_t;

typedef void (*blockdevice_end_io_t)(blockdevice_io_t *io, int err);

struct __blockdevice_io
{
    blockdevice_t *bdev;
    blockdevice_end_io_t end_io;
    blockdevice_io_type_t type;
    uint64_t start_sector;
    void *buf;
    size_t size;
    /* FIXME flush cache should be a blockdevice operation. */
    bool flush_cache;
    /* FIXME this is ugly */
    bool bypass_lock;
    void *private_data;

#ifdef WITH_PERF
    uint64_t submit_date;
#endif
};

/** Block device operations */
typedef struct
{
    /* Mandatory operations */
    const char *(*get_name_op)(const void *context);

    uint64_t (*get_sector_count_op)(const void *context);
    int (*set_sector_count_op)(void *context, uint64_t count);

    /* Asynchronous operations. Either all are defined (non NULL) or none is. */
    int (*submit_io_op)(void *context, blockdevice_io_t *io);

    int (*close_op)(void *context);
} blockdevice_ops_t;

/**
 * Create a block device.
 *
 * @param[in,out] bdev       Block device created
 * @param[in]     context    Context describing the block device
 * @param[in]     ops        Operations on the context
 * @param[in]     access     Access mode
 *
 * @return 0 if successful, a negative error code otherwise
 */
/* XXX The open/close terminology doesn't fit well */
int blockdevice_open(blockdevice_t **bdev, void *context,
                     const blockdevice_ops_t *ops, blockdevice_access_t access);

/**
 * Close a block device.
 *
 * NOTE:
 *   - The context is *not* freed. It is left to close_op to do so, if needed.
 *
 * @param[in,out] bdev  Block device to close and free
 *
 * @return 0 if successful, a negative error code otherwise.
 */
int blockdevice_close(blockdevice_t *bdev);

/**
 * "Name" of a block device.
 *
 * @param[in] bdev  Block device
 *
 * @return name if successful, NULL otherwise
 */
const char *blockdevice_name(const blockdevice_t *bdev);

/**
 * Number of blocks of a block device.
 *
 * @param[in] bdev  Block device
 *
 * @return number of blocks; 0 is considered an error
 */
uint64_t blockdevice_get_sector_count(const blockdevice_t *bdev);

/**
 * Set the number of blocks of a block device.
 *
 * @param[in] bdev  Block device
 * @param[in] count New count
 *
 * @return 0 on success, a negative error code otherwise
 */
int blockdevice_set_sector_count(const blockdevice_t *bdev, uint64_t count);

/**
 * Size of a block device.
 *
 * @param[in] bdev  Block device
 *
 * @return size of the block device, in bytes; 0 is considered an error
 */
uint64_t blockdevice_size(const blockdevice_t *bdev);

/**
 * Read from a block device.
 *
 * @param      bdev          Block device
 * @param[out] buf           Buffer where to store the data read
 * @param[in]  size          Size of buffer in bytes
 * @param[in]  start_sector  Sector at which to start reading
 *
 * @return 0 if successful, a negative error code otherwise
 */
int blockdevice_read(blockdevice_t *bdev, void *buf, size_t size,
                     uint64_t start_sector);

/**
 * Write to a block device.
 *
 * @param     bdev          Block device
 * @param[in] buf           Buffer holding the data to write
 * @param[in] size          Size of buffer in bytes
 * @param[in] start_sector  Block at which to start writing
 *
 * @return 0 if successful, a negative error code otherwise
 */
int blockdevice_write(blockdevice_t *bdev, const void *buf, size_t size,
                      uint64_t start_sector);

/**
 * Submit an asynchronous IO.
 *
 * @param      bdev          Block device
 * @param[out] io            Handle on submitted IO (if successful)
 * @param[in]  type          Type of IO to perform
 * @param[in]  start_sector  Sector at which to start the IO
 * @param      buf           Buffer holding the data (write) or where to store the
 *                           data (read)
 * @param[in]  size          Size of buffer in bytes
 * @param[in]  flush_cache   Whether to flush cache after performing the IO
 * @param[in]  private_data  Caller's private data
 * @param[in]  end_io        Function to be called upon IO completion.
 *
 * @return 0 if successful, a negative error code otherwise
 */
int blockdevice_submit_io(blockdevice_t *bdev, blockdevice_io_t *io,
                          blockdevice_io_type_t type, uint64_t start_sector,
                          void *buf, size_t size, bool flush_cache,
                          void *private_data, blockdevice_end_io_t end_io);

/**
 * Submit an asynchronous IO, specifying whether to bypass the I/O lock.
 *
 * @param      bdev          Block device
 * @param[out] io            Handle on submitted IO (if successful)
 * @param[in]  type          Type of IO to perform
 * @param[in]  start_sector  Block at which to start the IO
 * @param      buf           Buffer holding the data (write) or where to store the
 *                           data (read)
 * @param[in]  size          Size of buffer in bytes
 * @param[in]  flush_cache   Whether to flush cache after performing the IO
 * @param[in]  bypass_lock   Whether to bypass the I/O lock
 * @param[in]  private_data  Caller's private data
 * @param[in]  end_io        Function to be called upon IO completion.
 *
 * @return 0 if successful, a negative error code otherwise
 */
int __blockdevice_submit_io(blockdevice_t *bdev, blockdevice_io_t *io,
                          blockdevice_io_type_t type, uint64_t start_sector,
                          void *buf, size_t size, bool flush_cache, bool bypass_lock,
                          void *private_data, blockdevice_end_io_t end_io);

/**
 * Finish an IO
 *
 * @param      io      The io to end
 * @param[in]  err     The result of the IO
 */

void blockdevice_end_io(blockdevice_io_t *io, int err);

/**
 * Flush all IOs.
 *
 * When this function returns, all the IOs that were in progress
 * (asynchronous or not) are guaranteed to have been performed.
 *
 * @param bdev  Block device
 *
 * @return 0 if successful, a negative error code otherwise.
 */
int blockdevice_flush(blockdevice_t *bdev);

/**
 * Get the access mode of a block device.
 *
 * @param[in] bdev  Block device
 *
 * @return access mode if the block device is valid, or an arbitrary
 *         invalid value otherwise (check with BLOCKDEVICE_ACCESS_IS_VALID()).
 */
blockdevice_access_t blockdevice_access(const blockdevice_t *bdev);

#endif /* BLOCKDEV_H */
