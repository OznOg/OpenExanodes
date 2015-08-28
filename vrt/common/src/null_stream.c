/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/null_stream.h"

#include "os/include/os_mem.h"

static int null_stream_write(void *v, const void *buf, size_t size)
{
    return size;
}

static int null_stream_flush(void *v)
{
    return 0;
}

/* Not sure supporting seek is right: we can't tell a meaningful position
   since seeking from the end of the stream has no sense?. */
static int null_stream_seek(void *v, int64_t offset, stream_seek_t seek)
{
    return 0;
}

static uint64_t null_stream_tell(void *v)
{
    return 0;
}

static stream_ops_t null_stream_ops =
{
    .read_op = NULL,
    .write_op = null_stream_write,
    .flush_op = null_stream_flush,
    .seek_op = null_stream_seek,
    .tell_op = null_stream_tell,
    .close_op = NULL
};

int null_stream_open(stream_t **stream)
{
    /* stream_open() doesn't accept a null context, so we pass in a dummy */
    static int dummy_context;
    return stream_open(stream, &dummy_context, &null_stream_ops, STREAM_ACCESS_WRITE);
}
