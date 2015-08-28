/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "blockdevice/include/blockdevice_stream.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"

#include <string.h> /* for memcpy */

/** Sector cache */
typedef struct
{
    char     *buffer;       /**< Cached sectors' data */
    size_t   size;          /**< Cache size, in bytes */
    uint64_t first_sector;  /**< First sector in the cache */
    bool     valid;         /**< Whether the cache is valid */
    bool     dirty;         /**< Whether the cache is dirty */
} cache_t;

/** Context for streaming on a block device */
typedef struct {
    blockdevice_t *bdev;  /**< Block device streamed on */
    uint64_t offset;      /**< Current offset, in bytes */
    cache_t cache;        /**< Sector cache */
} blockdev_stream_context_t;

#define __cache_read(cache, bdev, sector)                               \
    blockdevice_read((bdev), (cache)->buffer, (cache)->size, (sector))

#define __cache_write(cache, bdev)                                      \
    blockdevice_write((bdev), (cache)->buffer, (cache)->size, (cache)->first_sector)

static bool sector_is_cached(const blockdev_stream_context_t *ctx, uint64_t sector)
{
    return sector >= ctx->cache.first_sector
        && sector < ctx->cache.first_sector + BYTES_TO_SECTORS(ctx->cache.size);
}

static int blockdev_stream_flush(void *v)
{
    blockdev_stream_context_t *ctx = v;
    int err;

    /* If cache is invalid or clean, there's nothing to flush */
    if (!ctx->cache.valid || !ctx->cache.dirty)
        return 0;

    err = __cache_write(&ctx->cache, ctx->bdev);
    if (err != 0)
        return err;

    ctx->cache.dirty = false;

    blockdevice_flush(ctx->bdev);

    return 0;
}

static int blockdev_stream_read(void *v, void *buf, size_t size)
{
    blockdev_stream_context_t *ctx = v;
    uint64_t bytes_read;
    uint64_t real_size_in_bytes = blockdevice_size(ctx->bdev);

    if (ctx->offset + size > real_size_in_bytes)
        size = real_size_in_bytes - ctx->offset;
    if (size == 0)
        return 0;

    bytes_read = 0;
    while (bytes_read < size)
    {
        uint64_t sector_offset = BYTES_TO_SECTORS(ctx->offset);
        uint32_t offset_inside_cache;
        uint32_t bytes_to_read;

        /* About to read a non cached area */
        if (ctx->cache.valid && !sector_is_cached(ctx, sector_offset))
        {
            /* If cache is dirty, we need to flush it */
            if (ctx->cache.dirty)
            {
                int err = blockdev_stream_flush(ctx);
                if (err != 0)
                    return err;
            }

            /* invalidate cache */
            ctx->cache.valid = false;
        }

        if (!ctx->cache.valid)
        {
            /* Read a full buffer starting at the first sector */
            int err = __cache_read(&ctx->cache, ctx->bdev, sector_offset);
            if (err != 0)
                return err;

            ctx->cache.dirty = false;
            ctx->cache.valid = true;
            ctx->cache.first_sector = sector_offset;
        }

        offset_inside_cache = ctx->offset - SECTORS_TO_BYTES(ctx->cache.first_sector);
        EXA_ASSERT(offset_inside_cache < ctx->cache.size);

        /* Now we're only interested in part of the buffer:
         * starting at the offset inside the first sector,
         * and until the size we want or the buffer size
         */
        bytes_to_read = MIN(size - bytes_read,
                            ctx->cache.size - offset_inside_cache);

        memcpy((char *)buf + bytes_read,
               ctx->cache.buffer + offset_inside_cache,
               bytes_to_read);

        ctx->offset += bytes_to_read;
        bytes_read += bytes_to_read;
    }

    return bytes_read;
}

static int blockdev_stream_write(void *v, const void *buf, size_t size)
{
    blockdev_stream_context_t *ctx = v;
    uint64_t bytes_written = 0;

    if (size == 0)
        return 0;

    /* Check if IO goes beyond the end of device */
    if (ctx->offset + size > blockdevice_size(ctx->bdev))
        return -ENOSPC;

    while (bytes_written < size)
    {
        uint64_t sector_offset = BYTES_TO_SECTORS(ctx->offset);
        uint32_t offset_inside_cache;
        uint32_t bytes_to_write;

        /* If the first sector of IO is not in cache, flush the cache and
         * invalidate it */
        /* About to read a non cached area */
        if (ctx->cache.valid && !sector_is_cached(ctx, sector_offset))
        {
            /* If cache is dirty, we need to flush it */
            if (ctx->cache.dirty)
            {
                int err = blockdev_stream_flush(ctx);
                if (err != 0)
                    return err;
            }

            /* invalidate cache */
            ctx->cache.valid = false;
        }

        if (!ctx->cache.valid)
        {
            /* Read a full buffer starting at the first sector */
            int err = __cache_read(&ctx->cache, ctx->bdev, sector_offset);
            if (err != 0)
                return err;

            ctx->cache.first_sector = sector_offset;
            ctx->cache.valid = true;
        }

        offset_inside_cache = ctx->offset - SECTORS_TO_BYTES(ctx->cache.first_sector);
        EXA_ASSERT(offset_inside_cache < ctx->cache.size);

        bytes_to_write = MIN(size - bytes_written,
                             ctx->cache.size - offset_inside_cache);

        memcpy(ctx->cache.buffer + offset_inside_cache,
               (char *)buf + bytes_written, bytes_to_write);

        ctx->cache.dirty = true;
        bytes_written += bytes_to_write;
        ctx->offset += bytes_to_write;
    }

    return bytes_written;
}

static int blockdev_stream_seek(void *v, int64_t offset, stream_seek_t seek)
{
    blockdev_stream_context_t *ctx = v;
    uint64_t real_size_in_bytes = blockdevice_size(ctx->bdev);

    switch (seek)
    {
    case STREAM_SEEK_FROM_BEGINNING:
        if (offset < 0 || offset > real_size_in_bytes)
            return -EINVAL;
        ctx->offset = offset;
        return 0;

    case STREAM_SEEK_FROM_POS:
        if ((int64_t)ctx->offset + offset < 0
            || ctx->offset + offset > real_size_in_bytes)
            return -EINVAL;
        ctx->offset += offset;
        return 0;

    case STREAM_SEEK_FROM_END:
        if (real_size_in_bytes + offset > real_size_in_bytes
            || (int64_t)real_size_in_bytes + offset < 0)
            return -EINVAL;
        ctx->offset = real_size_in_bytes + offset;
        return 0;
    }

    EXA_ASSERT(false);
    return -EINVAL;
}

static uint64_t blockdev_stream_tell(void *v)
{
    blockdev_stream_context_t *ctx = v;

    return ctx->offset;
}

static void blockdev_stream_close(void *v)
{
    blockdev_stream_context_t *ctx = v;

    /* No need to flush here, since the vrt_stream API guarantees the flush
       operation is called before the close operation. */

    os_free(ctx->cache.buffer);
    os_free(ctx);
}

static const stream_ops_t blockdev_stream_ops =
{
    .read_op = blockdev_stream_read,
    .write_op = blockdev_stream_write,
    .seek_op = blockdev_stream_seek,
    .tell_op = blockdev_stream_tell,
    .flush_op = blockdev_stream_flush,
    .close_op = blockdev_stream_close
};

int blockdevice_stream_on(stream_t **stream, blockdevice_t *bdev,
                          size_t cache_size, stream_access_t access)
{
    blockdev_stream_context_t *ctx;
    int err;

    if (bdev == NULL)
        return -EINVAL;

    if (!STREAM_ACCESS_IS_VALID(access))
        return -EINVAL;

    if (access == STREAM_ACCESS_WRITE || access == STREAM_ACCESS_RW)
        if (blockdevice_access(bdev) == BLOCKDEVICE_ACCESS_READ)
            return -EPERM;

    ctx = os_malloc(sizeof(blockdev_stream_context_t));
    if (ctx == NULL)
        return -ENOMEM;

    ctx->bdev = bdev;
    ctx->offset = 0;

    ctx->cache.buffer = os_malloc(cache_size);
    if (ctx->cache.buffer == NULL)
    {
        os_free(ctx);
        return -ENOMEM;
    }
    ctx->cache.size = cache_size;
    ctx->cache.first_sector = 0;
    ctx->cache.valid = false;
    ctx->cache.dirty = false;

    err = stream_open(stream, ctx, &blockdev_stream_ops, access);
    if (err != 0)
    {
        os_free(ctx->cache.buffer);
        os_free(ctx);
        return err;
    }

    return 0;
}
