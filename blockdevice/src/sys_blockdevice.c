/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "blockdevice/include/sys_blockdevice.h"

#include "os/include/os_dir.h"
#include "os/include/os_disk.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_string.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"

#include <stdio.h>  /* for FILE */

/* 'align' is assumed to be a power of 2 */
#define __ALIGNED_ON(ptr, align)  (((size_t)(ptr) & ((align) - 1)) == 0)

typedef struct
{
    char *path;
    /* XXX Probably a HANDLE on Windows */
    int fd;
} sys_blockdevice_context_t;

static sys_blockdevice_context_t *__blockdevice_context_alloc(const char *path,
                                                              int fd)
{
    sys_blockdevice_context_t *ctx = os_malloc(sizeof(sys_blockdevice_context_t));

    if (ctx == NULL)
        return NULL;

    ctx->path = os_strdup(path);
    if (ctx->path == NULL)
    {
        os_free(ctx);
        return NULL;
    }

    ctx->fd = fd;

    return ctx;
}

static void __blockdevice_context_free(sys_blockdevice_context_t *ctx)
{
    if (ctx == NULL)
        return;

    close(ctx->fd);

    os_free(ctx->path);
    os_free(ctx);
}

static const char *sys_blockdevice_name(const void *v)
{
    return ((sys_blockdevice_context_t *)v)->path;
}

static size_t sys_blockdevice_block_size(const void *v)
{
    /* XXX Not nice */
#ifdef WIN32
    return 4096;
#else
    return 512;
#endif
}

static uint64_t sys_blockdevice_get_sector_count(const void *v)
{
    const sys_blockdevice_context_t *ctx = v;
    uint64_t size;

    if (os_disk_get_size(ctx->fd, &size) != 0)
        return 0;

    return BYTES_TO_SECTORS(size);
}

static int sys_blockdevice_set_sector_count(void *v, uint64_t count)
{
    return -EPERM;
}

static int sys_blockdevice_do_read(sys_blockdevice_context_t *ctx, void *buf,
                                   size_t size, uint64_t start_sector)
{
    size_t block_size = sys_blockdevice_block_size(ctx);
    off_t ofs;
    ssize_t r;

    if (!__ALIGNED_ON(buf, block_size))
        return -EINVAL;

    ofs = start_sector * SECTOR_SIZE;
    if (lseek(ctx->fd, ofs, SEEK_SET) == (off_t)-1)
        return -errno;

    r = read(ctx->fd, buf, size);
    if (r < 0)
        return -errno;

    if (r != size)
        return -EIO;

    return 0;
}

static int sys_blockdevice_do_write(sys_blockdevice_context_t *ctx,
                                    const void *buf, size_t size, uint64_t block)
{
    size_t block_size = sys_blockdevice_block_size(ctx);

    if (!__ALIGNED_ON(buf, block_size))
        return -EINVAL;

    /* FIXME */
    return -EIO;
}

int sys_blockdevice_close(void *v)
{
    __blockdevice_context_free((sys_blockdevice_context_t *)v);

    return 0;
}

static int sys_blockdevice_submit_io(void *v, blockdevice_io_t *io)
{
    sys_blockdevice_context_t *ctx = v;
    size_t block_size = sys_blockdevice_block_size(ctx);
    size_t aligned_size;
    void *buf;
    int err;

    if (io->size % block_size == 0)
        aligned_size = io->size;
    else
        aligned_size = io->size + block_size - io->size % block_size;

    EXA_ASSERT(aligned_size >= io->size);

    buf = os_aligned_malloc(aligned_size, block_size, &err);

    if (buf == NULL)
    {
        blockdevice_end_io(io, -EINVAL);
        return -EINVAL;
    }

    err = -EOPNOTSUPP;

    switch (io->type)
    {
        case BLOCKDEVICE_IO_READ:
            err = sys_blockdevice_do_read(v, buf, aligned_size, io->start_sector);
            memcpy(io->buf, buf, io->size);
            break;

        case BLOCKDEVICE_IO_WRITE:
            memcpy(buf, io->buf, io->size);
            err = sys_blockdevice_do_write(v, buf, aligned_size, io->start_sector);
            break;
    }

    os_aligned_free(buf);

    blockdevice_end_io(io, err);
    return 0;
}

/* System blockdevice with no asynchronous IO support */
static blockdevice_ops_t sys_blockdevice_ops =
{
    .get_name_op = sys_blockdevice_name,

    .get_sector_count_op = sys_blockdevice_get_sector_count,
    .set_sector_count_op = sys_blockdevice_set_sector_count,

    .submit_io_op = sys_blockdevice_submit_io,

    .close_op = sys_blockdevice_close
};

int sys_blockdevice_open(blockdevice_t **bdev, const char *path,
                         blockdevice_access_t access)
{
    sys_blockdevice_context_t *ctx;
    int fd = -1;
    int err;

    /* XXX Should check that it *is* indeed a blockdevice rather than
           do some path-based guessing game... */
    if (!os_disk_path_is_valid(path))
        return -ENODEV;

    switch (access)
    {
    case BLOCKDEVICE_ACCESS_READ:
        fd = os_disk_open_raw(path, OS_DISK_READ | OS_DISK_DIRECT);
        break;

    case BLOCKDEVICE_ACCESS_WRITE:
        fd = os_disk_open_raw(path, OS_DISK_WRITE | OS_DISK_DIRECT);
        break;

    case BLOCKDEVICE_ACCESS_RW:
        fd = os_disk_open_raw(path, OS_DISK_RDWR | OS_DISK_DIRECT);
        break;
    }

    if (fd < 0)
        return fd;

    ctx = __blockdevice_context_alloc(path, fd);
    if (ctx == NULL)
    {
        close(fd);
        return -ENOMEM;
    }

    err = blockdevice_open(bdev, ctx, &sys_blockdevice_ops, access);
    if (err != 0)
    {
        __blockdevice_context_free(ctx);
        return err;
    }

    return 0;
}
