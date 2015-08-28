/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "blockdevice/include/blockdevice.h"

#include "os/include/os_atomic.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"

#include <stdlib.h>

struct blockdevice
{
    void *context;                /**< Block device context */
    blockdevice_ops_t ops;        /**< Operations on the context */
    blockdevice_access_t access;  /**< Access mode */
    bool buffering;               /**< Whether system buffering mode is enabled*/
    os_atomic_t pending_io_count; /**< Number of IOs being processed */
};

int blockdevice_open(blockdevice_t **bdev, void *context,
                     const blockdevice_ops_t *ops, blockdevice_access_t access)
{
    if (context == NULL || ops == NULL)
        return -EINVAL;

    if (!BLOCKDEVICE_ACCESS_IS_VALID(access))
        return -EINVAL;

    if (ops->get_name_op == NULL
        || ops->get_sector_count_op == NULL
        || ops->set_sector_count_op == NULL
        || ops->submit_io_op == NULL)
        return -EINVAL;

    *bdev = os_malloc(sizeof(blockdevice_t));
    if (*bdev == NULL)
        return -ENOMEM;

    (*bdev)->context = context;
    (*bdev)->ops = *ops;
    (*bdev)->access = access;

    os_atomic_set(&(*bdev)->pending_io_count, 0);

    return 0;
}

int blockdevice_close(blockdevice_t *bdev)
{
    if (bdev == NULL)
        return 0;

    /* FIXME obviously there is a race here if caller keeps sumitting IOs...
     * For now, it is not really harmful as we know that there is a correct use
     * of this API, but this is not clean, anyway. */
    if (os_atomic_read(&bdev->pending_io_count) != 0)
        return -EBUSY;

    if (bdev->ops.close_op != NULL)
    {
        int err = bdev->ops.close_op(bdev->context);

        if (err != 0)
            return err;
    }
    os_free(bdev);

    return 0;
}

const char *blockdevice_name(const blockdevice_t *bdev)
{
    return bdev->ops.get_name_op(bdev->context);
}

uint64_t blockdevice_get_sector_count(const blockdevice_t *bdev)
{
    return bdev->ops.get_sector_count_op(bdev->context);
}

int blockdevice_set_sector_count(const blockdevice_t *bdev, uint64_t count)
{
    return bdev->ops.set_sector_count_op(bdev->context, count);
}

uint64_t blockdevice_size(const blockdevice_t *bdev)
{
    return SECTORS_TO_BYTES(blockdevice_get_sector_count(bdev));
}

static void blockdevice_io_complete(blockdevice_io_t *io, int error)
{
    complete((completion_t *)io->private_data, error);
}

int blockdevice_read(blockdevice_t *bdev, void *buf, size_t size,
                     uint64_t start_sector)
{
    blockdevice_io_t bio;
    completion_t io_completion;
    int err;

    init_completion(&io_completion);

    /* FIXME The first sector is int64_t, gets passed uint64_t (start_sector) */
    err = blockdevice_submit_io(bdev, &bio, BLOCKDEVICE_IO_READ, start_sector,
                                buf, size, false, &io_completion,
                                blockdevice_io_complete);

    return err != 0 ? err : wait_for_completion(&io_completion);
}

int blockdevice_write(blockdevice_t *bdev, const void *buf, size_t size,
                      uint64_t start_sector)
{
    blockdevice_io_t bio;
    completion_t io_completion;
    int err;

    if (bdev->access == BLOCKDEVICE_ACCESS_READ)
        return -EINVAL;

    init_completion(&io_completion);

    /* FIXME The first sector is int64_t, gets passed uint64_t (start_sector) */
    err = blockdevice_submit_io(bdev, &bio, BLOCKDEVICE_IO_WRITE, start_sector,
                                (void *)buf, size, false, &io_completion,
                                blockdevice_io_complete);

    return err != 0 ? err : wait_for_completion(&io_completion);
}

int blockdevice_flush(blockdevice_t *bdev)
{
    blockdevice_io_t bio;
    completion_t io_completion;
    int err;

    init_completion(&io_completion);

    err = blockdevice_submit_io(bdev, &bio, BLOCKDEVICE_IO_WRITE, 0, NULL, 0,
                        true, &io_completion, blockdevice_io_complete);

    return err != 0 ? err : wait_for_completion(&io_completion);
}

/**
 * Initialize a blockdevice_io_t so that it is ready to be sumitted to
 * blockdevice.
 *
 * @param bdev          Block device
 * @param type          io type
 * @param start_sector  the sector where the IO starts
 * @param buf           buffer holding data (W) or where to put data (R)
 * @param size          Size of buffer in bytes
 * @param flush_cache   Whether to flush cache after performing the IO
 * @param bypass_lock   Whether to bypass the nbd I/O lock
 * @param private_data  Caller's private data
 * @param end_io        Function to be called upon IO completion.
 */
static void blockdevice_io_init(blockdevice_io_t *io, blockdevice_t *bdev,
                                blockdevice_io_type_t type, uint64_t start_sector,
                                void *buf, size_t size, bool flush_cache,
                                bool bypass_lock, void *private_data,
                                blockdevice_end_io_t end_io)
{
    io->bdev         = bdev;
    io->type         = type;
    io->start_sector = start_sector;
    io->buf          = buf;
    io->size         = size;
    io->flush_cache  = flush_cache;
    io->bypass_lock  = bypass_lock;
    io->private_data = private_data;
    io->end_io       = end_io;
}

int __blockdevice_submit_io(blockdevice_t *bdev, blockdevice_io_t *io,
                            blockdevice_io_type_t type, uint64_t start_sector,
                            void *buf, size_t size, bool flush_cache,
                            bool bypass_lock, void *private_data,
                            blockdevice_end_io_t end_io)
{
    if (io == NULL)
        return -EINVAL;

    if (!BLOCKDEVICE_IO_TYPE_IS_VALID(type))
        return -EINVAL;

    if (buf == NULL && size != 0)
        return -EINVAL;

    os_atomic_inc(&bdev->pending_io_count);

    blockdevice_io_init(io, bdev, type, start_sector, buf, size, flush_cache,
                        bypass_lock, private_data, end_io);

    return bdev->ops.submit_io_op(bdev->context, io);
}

int blockdevice_submit_io(blockdevice_t *bdev, blockdevice_io_t *io,
                          blockdevice_io_type_t type, uint64_t start_sector,
                          void *buf, size_t size, bool flush_cache,
                          void *private_data, blockdevice_end_io_t end_io)
{
    return __blockdevice_submit_io(bdev, io, type, start_sector, buf, size,
                                   flush_cache, false, private_data, end_io);
}

blockdevice_access_t blockdevice_access(const blockdevice_t *bdev)
{
    if (bdev == NULL)
        /* Arbitrary value outside the range of valid access modes */
        return BLOCKDEVICE_ACCESS__LAST + 5;

    return bdev->access;
}

void blockdevice_end_io(blockdevice_io_t *io, int err)
{
    blockdevice_t *bdev = io->bdev;

    EXA_ASSERT(bdev != NULL);

    os_atomic_dec(&bdev->pending_io_count);

    io->end_io(io, err);
}
