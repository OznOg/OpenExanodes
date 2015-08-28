/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/memory_stream.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_string.h"  /* for memcpy() */

/* Context for streaming on a string */
typedef struct
{
    char *bytes;
    size_t size;
    uint64_t ofs;
} memory_stream_context_t;

static int memory_stream_read(void *v, void *buf, size_t size)
{
    memory_stream_context_t *msc = v;
    size_t r;

    r = size;
    if (msc->ofs + r > msc->size)
        r = msc->size - msc->ofs;

    memcpy(buf, msc->bytes + msc->ofs, r);

    msc->ofs += r;

    return r;
}

static int memory_stream_write(void *v, const void *buf, size_t size)
{
    memory_stream_context_t *msc = v;

    if (msc->ofs + size > msc->size)
        return -ENOSPC;

    memcpy(msc->bytes + msc->ofs, buf, size);

    msc->ofs += size;

    return size;
}

static int memory_stream_flush(void *v)
{
    return 0;
}

static int memory_stream_seek(void *v, int64_t offset, stream_seek_t seek)
{
    memory_stream_context_t *msc = v;
    uint64_t new_ofs = 0; /* Gcc complains without the init */

    switch (seek)
    {
    case STREAM_SEEK_FROM_BEGINNING:
        new_ofs = offset;
        break;

    case STREAM_SEEK_FROM_END:
        new_ofs = msc->size + offset;
        break;

    case STREAM_SEEK_FROM_POS:
        new_ofs = msc->ofs + offset;
        break;
    }

    if (new_ofs > msc->size)
        return -EINVAL;

    msc->ofs = new_ofs;

    return 0;
}

static uint64_t memory_stream_tell(void *v)
{
    return ((memory_stream_context_t *)v)->ofs;
}


static void memory_stream_close(void *msc)
{
    os_free(msc);
}

static stream_ops_t memory_stream_ops =
{
    .read_op  = memory_stream_read,
    .write_op = memory_stream_write,
    .flush_op = memory_stream_flush,
    .seek_op  = memory_stream_seek,
    .tell_op  = memory_stream_tell,
    .close_op  = memory_stream_close
};

int memory_stream_open(stream_t **stream, char *bytes, size_t size,
                       stream_access_t access)
{
    memory_stream_context_t *msc;
    int err;

    if (bytes == NULL)
        return -EINVAL;

    msc = os_malloc(sizeof(memory_stream_context_t));
    if (msc == NULL)
        return -ENOMEM;

    msc->bytes = bytes;
    msc->size = size;
    msc->ofs = 0;

    err = stream_open(stream, msc, &memory_stream_ops, access);
    if (err != 0)
    {
        os_free(msc);
        return err;
    }

    return 0;
}
