/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/checksum_stream.h"

#include "common/include/checksum.h"
#include "common/include/exa_assert.h"

#include "os/include/os_mem.h"
#include "os/include/os_error.h"

typedef enum { CHECKSUM_STREAM_MAGIC = 0xAA770011 }  checksum_stream_magic_t;

typedef struct
{
    checksum_stream_magic_t magic;
    stream_t *base_stream;
    checksum_context_t checksum_ctx;
} checksum_stream_context_t;

static int checksum_stream_read(void *v, void *buf, size_t size)
{
    checksum_stream_context_t *csc = v;
    int ret;

    ret = stream_read(csc->base_stream, buf, size);

    if (ret > 0)
        checksum_feed(&csc->checksum_ctx, buf, ret);

    return ret;
}

static int checksum_stream_write(void *v, const void *buf, size_t size)
{
    checksum_stream_context_t *csc = v;
    int ret;

    ret = stream_write(csc->base_stream, buf, size);

    if (ret > 0)
        checksum_feed(&csc->checksum_ctx, buf, ret);

    return ret;
}

static int checksum_stream_flush(void *v)
{
    checksum_stream_context_t *csc = v;

    return stream_flush(csc->base_stream);
}

static uint64_t checksum_stream_tell(void *v)
{
    checksum_stream_context_t *csc = v;

    return stream_tell(csc->base_stream);
}

static int checksum_stream_seek(void *v, int64_t offset, stream_seek_t seek)
{
    checksum_stream_context_t *csc = v;

    if (offset != 0 || seek != STREAM_SEEK_FROM_BEGINNING)
        return -EINVAL;

    checksum_reset(&csc->checksum_ctx);

    return stream_seek(csc->base_stream, offset, seek);
}

static void checksum_stream_close(void *v)
{
    os_free(v);
}

static stream_ops_t checksum_stream_ops =
{
    .read_op = checksum_stream_read,
    .write_op = checksum_stream_write,
    .flush_op = checksum_stream_flush,
    .seek_op = checksum_stream_seek,
    .tell_op = checksum_stream_tell,
    .close_op = checksum_stream_close
};

int checksum_stream_open(stream_t **stream, stream_t *base_stream)
{
    checksum_stream_context_t *csc;
    int err;

    if (base_stream == NULL)
        return -EINVAL;

    csc = os_malloc(sizeof(checksum_stream_context_t));
    if (csc == NULL)
        return -ENOMEM;

    csc->magic = CHECKSUM_STREAM_MAGIC;
    csc->base_stream = base_stream;

    checksum_reset(&csc->checksum_ctx);

    err = stream_open(stream, csc, &checksum_stream_ops,
                      stream_access(csc->base_stream));
    if (err != 0)
    {
        os_free(csc);
        return err;
    }

    return 0;
}

checksum_t checksum_stream_get_value(const stream_t *stream)
{
    const checksum_stream_context_t *csc = __stream_context(stream);

    EXA_ASSERT(csc->magic == CHECKSUM_STREAM_MAGIC);

    return checksum_get_value(&csc->checksum_ctx);
}

size_t checksum_stream_get_size(const stream_t *stream)
{
    const checksum_stream_context_t *csc = __stream_context(stream);

    EXA_ASSERT(csc->magic == CHECKSUM_STREAM_MAGIC);

    return checksum_get_size(&csc->checksum_ctx);
}

void checksum_stream_reset(stream_t *stream)
{
    checksum_stream_context_t *csc = __stream_context(stream);

    EXA_ASSERT(csc->magic == CHECKSUM_STREAM_MAGIC);

    checksum_reset(&csc->checksum_ctx);
}
