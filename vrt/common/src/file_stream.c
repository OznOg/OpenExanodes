/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/file_stream.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"

#include "log/include/log.h"

#include "os/include/os_disk.h"
#include "os/include/os_error.h"
#include "os/include/os_dir.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"

/* The context for streaming on a file is a FILE * */
typedef struct
{
    FILE *file;
    bool close_all;
    /* The following two fields are necessary to ensure we return -ENOSPC in
       case we write beyond the end of file, when the file is a block device. */
    bool is_bdev;
    uint64_t bdev_size;
} file_stream_context_t;

static int file_stream_read(void *v, void *buf, size_t size)
{
    file_stream_context_t *fsc = v;
    size_t r;

    r = fread(buf, 1, size, fsc->file);
    if (r < size)
    {
        int err = ferror(fsc->file);
        if (err != 0)
            return -err;
    }

    return r;
}

static int file_stream_write(void *v, const void *buf, size_t size)
{
    file_stream_context_t *fsc = v;
    size_t w;

    if (fsc->is_bdev)
    {
        /* Conform to the stream API by returning -ENOSPC when attempting to
           write beyond the end of the block device. */
        uint64_t ofs = ftell(fsc->file);
        if (ofs + size > fsc->bdev_size)
            return -ENOSPC;
    }

    w = fwrite(buf, 1, size, fsc->file);
    if (w < size)
    {
        int err = ferror(fsc->file);
        if (err != 0)
            return -err;
    }

    return w;
}

static int file_stream_flush(void *v)
{
    file_stream_context_t *fsc = v;

    /* XXX Should we flush the kernel buffers too? (sync, fsync) */
    if (fflush(fsc->file) == EOF)
        return errno;

    return 0;
}

static int file_stream_seek(void *v, int64_t offset, stream_seek_t seek)
{
    file_stream_context_t *fsc = v;
    int whence = 0;

    switch (seek)
    {
    case STREAM_SEEK_FROM_BEGINNING:
        whence = SEEK_SET;
        break;

    case STREAM_SEEK_FROM_END:
        whence = SEEK_END;
        break;

    case STREAM_SEEK_FROM_POS:
        whence = SEEK_CUR;
        break;
    }

    if (fseek(fsc->file, offset, whence) != 0)
        return -errno;

    return 0;
}

static uint64_t file_stream_tell(void *v)
{
    file_stream_context_t *fsc = v;

    long pos = ftell(fsc->file);
    if (pos < 0)
        return STREAM_TELL_ERROR;

    return pos;
}

static void file_stream_close(void *v)
{
    file_stream_context_t *fsc = v;

    if (fsc->close_all)
        fclose(fsc->file);

    free(fsc);
}

static stream_ops_t file_stream_ops =
{
    .read_op = file_stream_read,
    .write_op = file_stream_write,
    .flush_op = file_stream_flush,
    .seek_op = file_stream_seek,
    .tell_op = file_stream_tell,
    .close_op = file_stream_close
};

int __file_stream_on(stream_t **stream, FILE *file, stream_access_t access,
                     bool close_all)
{
    file_stream_context_t *fsc;
    struct stat stat;
    int err;

    if (fstat(fileno(file), &stat) < 0)
        return -errno;

    fsc = os_malloc(sizeof(file_stream_context_t));
    if (fsc == NULL)
        return -ENOMEM;

    fsc->file = file;
    fsc->close_all = close_all;
    fsc->is_bdev = S_ISBLK(stat.st_mode) ? true : false;
    if (fsc->is_bdev)
    {
        int err = os_disk_get_size(fileno(file), &fsc->bdev_size);
        if (err != 0)
        {
            os_free(fsc);
            return err;
        }
    }
    else
        fsc->bdev_size = 0;

    err = stream_open(stream, fsc, &file_stream_ops, access);
    if (err != 0)
    {
        os_free(fsc);
        return err;
    }

    return 0;
}

int file_stream_on(stream_t **stream, FILE *file, stream_access_t access)
{
    return __file_stream_on(stream, file, access, false);
}

int file_stream_open(stream_t **stream, const char *filename, stream_access_t access)
{
    FILE *file = NULL;
    int err;

    /* XXX Return -EINVAL instead, like the other streams do */
    EXA_ASSERT(filename != NULL);
    EXA_ASSERT(STREAM_ACCESS_IS_VALID(access));

    switch (access)
    {
    case STREAM_ACCESS_READ:
        file = fopen(filename, "rb");
        break;

    case STREAM_ACCESS_WRITE:
        file = fopen(filename, "wb");
        break;

    case STREAM_ACCESS_RW:
        file = fopen(filename, "rb+");
        break;
    }

    if (file == NULL)
        return -errno;

    err = __file_stream_on(stream, file, access, true);
    if (err != 0)
    {
        fclose(file);
        return err;
    }

    return 0;
}
