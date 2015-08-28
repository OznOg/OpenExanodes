/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/narrowed_stream.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"

typedef struct
{
    stream_t *base_stream;
    uint64_t start;
    uint64_t end;
    uint64_t ofs;
} narrowed_stream_context_t;

static int __adjust_offset(narrowed_stream_context_t *nsc)
{
    if (stream_tell(nsc->base_stream) == nsc->ofs)
        return 0;

    return stream_seek(nsc->base_stream, nsc->ofs, STREAM_SEEK_FROM_BEGINNING);
}

static int narrowed_stream_read(void *v, void *buf, size_t size)
{
    narrowed_stream_context_t *nsc = v;
    size_t base_size;
    int r;
    int err;

    err = __adjust_offset(nsc);
    if (err != 0)
        return err;

    if (nsc->ofs + size - 1 > nsc->end)
        base_size = nsc->end - nsc->ofs + 1;
    else
        base_size = size;

    r = stream_read(nsc->base_stream, buf, base_size);
    if (r < 0)
        return r;

    nsc->ofs += r;

    return r;
}

static int narrowed_stream_write(void *v, const void *buf, size_t size)
{
    narrowed_stream_context_t *nsc = v;
    int w;
    int err;

    err = __adjust_offset(nsc);
    if (err != 0)
        return err;

    if (nsc->ofs + size - 1 > nsc->end)
        return -ENOSPC;

    w = stream_write(nsc->base_stream, buf, size);
    if (w < 0)
        return w;

    nsc->ofs += w;

    return w;
}

static int narrowed_stream_flush(void *v)
{
    narrowed_stream_context_t *nsc = v;
    return stream_flush(nsc->base_stream);
}

static int narrowed_stream_seek(void *v, int64_t offset, stream_seek_t seek)
{
    narrowed_stream_context_t *nsc = v;
    uint64_t new_ofs = 0;
    int err;

    switch (seek)
    {
    case STREAM_SEEK_FROM_BEGINNING:
        new_ofs = nsc->start + offset;
        break;

    case STREAM_SEEK_FROM_END:
        new_ofs = nsc->end + offset;
        break;

    case STREAM_SEEK_FROM_POS:
        new_ofs = stream_tell(nsc->base_stream) + offset;
        break;
    }

    if (new_ofs < nsc->start || new_ofs > nsc->end)
        return -EINVAL;

    err = stream_seek(nsc->base_stream, new_ofs, STREAM_SEEK_FROM_BEGINNING);
    if (err == 0)
        nsc->ofs = new_ofs;

    return err;
}

static uint64_t narrowed_stream_tell(void *v)
{
    narrowed_stream_context_t *nsc = v;
    return nsc->ofs - nsc->start;
}

static void narrowed_stream_close(void *v)
{
    os_free(v);
}

static stream_ops_t narrowed_stream_ops =
{
    .read_op = narrowed_stream_read,
    .write_op = narrowed_stream_write,
    .flush_op = narrowed_stream_flush,
    .seek_op = narrowed_stream_seek,
    .tell_op = narrowed_stream_tell,
    .close_op = narrowed_stream_close
};

int narrowed_stream_open(stream_t **narrow_stream, stream_t *base_stream,
                         uint64_t start, uint64_t end, stream_access_t access)
{
    narrowed_stream_context_t *nsc;
    int err;

    if (base_stream == NULL)
        return -EINVAL;

    nsc = os_malloc(sizeof(narrowed_stream_context_t));
    if (nsc == NULL)
        return -ENOMEM;

    nsc->base_stream = base_stream;
    nsc->start = start;
    nsc->end = end;
    nsc->ofs = start;

    err = stream_open(narrow_stream, nsc, &narrowed_stream_ops, access);
    if (err != 0)
    {
        os_free(nsc);
        return err;
    }

    return 0;
}
