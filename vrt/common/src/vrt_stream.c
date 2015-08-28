/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/vrt_stream.h"

#include "os/include/os_assert.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"

#include <stdarg.h>

struct stream
{
    void *context;           /**< Stream context */
    stream_ops_t ops;        /**< Operations on the context */
    stream_access_t access;  /**< Access mode */
};

int stream_open(stream_t **stream, void *context, const stream_ops_t *ops,
                stream_access_t access)
{
    if (context == NULL || ops == NULL)
        return -EINVAL;

    if (!STREAM_ACCESS_IS_VALID(access))
        return -EINVAL;

    if (access == STREAM_ACCESS_READ || access == STREAM_ACCESS_RW)
        if (ops->read_op == NULL)
            return -EINVAL;

    if (access == STREAM_ACCESS_WRITE || access == STREAM_ACCESS_RW)
        if (ops->write_op == NULL)
            return -EINVAL;

    /* No check on ops->seek nor ops-tell since some contexts may not support
       these operations */

    *stream = os_malloc(sizeof(stream_t));
    if (*stream == NULL)
        return -ENOMEM;

    (*stream)->context = context;
    (*stream)->ops = *ops;
    (*stream)->access = access;

    return 0;
}

void __stream_close(stream_t *stream)
{
    if (stream == NULL)
        return;

    stream_flush(stream);

    if (stream->ops.close_op != NULL)
        stream->ops.close_op(stream->context);

    os_free(stream);
}

int stream_read(stream_t *stream, void *buf, size_t size)
{
    if (stream == NULL)
        return -EINVAL;

    if (size == 0)
        return 0;

    if (buf == NULL)
        return -EINVAL;

    if (stream->access != STREAM_ACCESS_READ && stream->access != STREAM_ACCESS_RW)
        return -EOPNOTSUPP;

    return stream->ops.read_op(stream->context, buf, size);
}

int stream_write(stream_t *stream, const void *buf, size_t size)
{
    if (stream == NULL)
        return -EINVAL;

    if (size == 0)
        return 0;

    if (buf == NULL)
        return -EINVAL;

    if (stream->access != STREAM_ACCESS_WRITE && stream->access != STREAM_ACCESS_RW)
        return -EOPNOTSUPP;

    return stream->ops.write_op(stream->context, buf, size);
}

int stream_vprintf(stream_t *stream, const char *fmt, va_list al)
{
    va_list al2;
    int size, size2;
    char *buf;
    int w;

    if (stream == NULL || fmt == NULL)
        return -EINVAL;

    va_copy(al2, al);
    size = vsnprintf(NULL, 0, fmt, al2);
    va_end(al2);

    if (size < 0)
        return size;

    /* +1 for terminal '\0' */
    buf = os_malloc(size + 1);
    if (buf == NULL)
        return -ENOMEM;

    va_copy(al2, al);
    size2 = vsnprintf(buf, size + 1, fmt, al2);
    va_end(al2);
    OS_ASSERT(size2 == size);

    w = stream_write(stream, buf, size);

    os_free(buf);

    return w;
}

int stream_printf(stream_t *stream, const char *fmt, ...)
{
    va_list al;
    int w;

    va_start(al, fmt);
    w = stream_vprintf(stream, fmt, al);
    va_end(al);

    return w;
}

int stream_flush(stream_t *stream)
{
    if (stream == NULL)
        return -EINVAL;

    if (stream->access != STREAM_ACCESS_WRITE && stream->access != STREAM_ACCESS_RW)
        return -EOPNOTSUPP;

    if (stream->ops.flush_op == NULL)
        return -EOPNOTSUPP;

    return stream->ops.flush_op(stream->context);
}

int stream_seek(stream_t *stream, int64_t offset, stream_seek_t seek)
{
    if (stream == NULL || !STREAM_SEEK_IS_VALID(seek))
        return -EINVAL;

    if (stream->ops.seek_op == NULL)
        return -EOPNOTSUPP;

    switch (seek)
    {
    case STREAM_SEEK_FROM_BEGINNING:
        if (offset < 0)
            return -EINVAL;
        break;

    case STREAM_SEEK_FROM_END:
        if (offset > 0)
            return -EINVAL;
        break;

    case STREAM_SEEK_FROM_POS:
        break;
    }

    return stream->ops.seek_op(stream->context, offset, seek);
}

int stream_rewind(stream_t *stream)
{
    return stream_seek(stream, 0, STREAM_SEEK_FROM_BEGINNING);
}

uint64_t stream_tell(const stream_t *stream)
{
    if (stream == NULL)
        return STREAM_TELL_ERROR;

    if (stream->ops.tell_op == NULL)
        return STREAM_TELL_ERROR;

    return stream->ops.tell_op(stream->context);
}

stream_access_t stream_access(const stream_t *stream)
{
    if (stream == NULL)
        /* Arbitrary value outside the range of valid access modes */
        return STREAM_ACCESS__LAST + 5;

    return stream->access;
}

void *__stream_context(const stream_t *stream)
{
    return stream->context;
}
