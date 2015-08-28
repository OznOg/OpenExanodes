/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/tee_stream.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"

typedef struct
{
    stream_t *streams[2];
} tee_stream_context_t;

static int tee_stream_write(void *tsc, const void *buf, size_t size)
{
    tee_stream_context_t *_tsc = tsc;
    int i;

    for (i = 0; i < 2; i++)
    {
        int w = stream_write(_tsc->streams[i], buf, size);
        if (w < 0)
            return w;
        else if (w != size)
            return -EIO;
    }

    return size;
}

static int tee_stream_flush(void *tsc)
{
    tee_stream_context_t *_tsc = tsc;
    int errs[2];
    int i;

    /* Best effort: try to flush both streams */
    for (i = 0; i < 2; i++)
        errs[i] = stream_flush(_tsc->streams[i]);

    for (i = 0; i < 2; i++)
        if (errs[i] != 0)
            return errs[i];

    return 0;
}

static int tee_stream_seek(void *tsc, int64_t offset, stream_seek_t seek)
{
    tee_stream_context_t *_tsc = tsc;
    int i;

    for (i = 0; i < 2; i++)
    {
        int err = stream_seek(_tsc->streams[i], offset, seek);
        if (err != 0)
            return err;
    }

    return 0;
}

static uint64_t tee_stream_tell(void *tsc)
{
    tee_stream_context_t *_tsc = tsc;
    /* Arbitrarily return the offset of the first stream */
    return stream_tell(_tsc->streams[0]);
}

static void tee_stream_close(void *tsc)
{
    os_free(tsc);
}

static stream_ops_t tee_stream_ops =
{
    .read_op  = NULL,
    .write_op = tee_stream_write,
    .flush_op = tee_stream_flush,
    .seek_op  = tee_stream_seek,
    .tell_op  = tee_stream_tell,
    .close_op = tee_stream_close,
};

int tee_stream_open(stream_t **tee_stream, stream_t *stream1, stream_t *stream2)
{
    stream_access_t a1, a2;
    tee_stream_context_t *tsc;
    int err;

    if (stream1 == NULL || stream2 == NULL)
        return -EINVAL;

    a1 = stream_access(stream1);
    a2 = stream_access(stream2);
    if (!STREAM_ACCESS_IS_VALID(a1) || !STREAM_ACCESS_IS_VALID(a1))
        return -EINVAL;

    /* Both streams must be writable, ie not readonly */
    if (a1 == STREAM_ACCESS_READ || a2 == STREAM_ACCESS_READ)
        return -EPERM;

    tsc = os_malloc(sizeof(tee_stream_context_t));
    if (tsc == NULL)
        return -ENOMEM;

    tsc->streams[0] = stream1;
    tsc->streams[1] = stream2;

    err = stream_open(tee_stream, tsc, &tee_stream_ops, STREAM_ACCESS_WRITE);
    if (err != 0)
    {
        os_free(tsc);
        return err;
    }

    return 0;
}
