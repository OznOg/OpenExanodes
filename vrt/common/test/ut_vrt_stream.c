/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/common/include/vrt_stream.h"

/* Necessary: we need an actual stream implementation, not just a hollow
   shell as the generic stream module provides. */
#include "vrt/common/include/memory_stream.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"

static char __buf[128];

UT_SECTION(stream_open)

static int dummy_read(void *unused, void *buf, size_t size)
{
    return 0;
}

static int dummy_write(void *unused, const void *buf, size_t size)
{
    return 0;
}

static int dummy_flush(void *unused)
{
    return 0;
}

static int dummy_seek(void *unused, int64_t offset, stream_seek_t seek)
{
    return 0;
}

static uint64_t dummy_tell(void *unused)
{
    return STREAM_TELL_ERROR;
}

static stream_ops_t dummy_ops =
{
    .read_op = dummy_read,
    .write_op = dummy_write,
    .flush_op = dummy_flush,
    .seek_op = dummy_seek,
    .tell_op = dummy_tell
};

ut_test(new_stream_on_null_context_returns_EINVAL)
{
    stream_t *stream;
    UT_ASSERT_EQUAL(-EINVAL, stream_open(&stream, NULL, &dummy_ops, STREAM_ACCESS_READ));
}

ut_test(new_stream_with_null_ops_returns_EINVAL)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;

    UT_ASSERT_EQUAL(-EINVAL, stream_open(&stream, dummy, NULL, STREAM_ACCESS_READ));
}

ut_test(new_stream_with_invalid_access_returns_EINVAL)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;

    UT_ASSERT_EQUAL(-EINVAL, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS__FIRST - 1));
    UT_ASSERT_EQUAL(-EINVAL, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS__LAST + 1));
}

ut_test(new_readonly_stream_with_no_read_op_returns_EINVAL)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_ops_t ops =
    {
        .read_op = NULL,
        .write_op = dummy_write,
        .seek_op = NULL,
        .tell_op = NULL,
        .close_op = NULL
    };
    stream_t *stream;

    UT_ASSERT_EQUAL(-EINVAL, stream_open(&stream, dummy, &ops, STREAM_ACCESS_READ));
}

ut_test(new_readonly_stream_with_read_op_succeeds)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_ops_t ops =
    {
        .read_op = dummy_read,
        .write_op = NULL,
        .seek_op = NULL,
        .tell_op = NULL,
        .close_op = NULL
    };
    stream_t *stream = NULL;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &ops, STREAM_ACCESS_READ));
    UT_ASSERT(stream != NULL);
    stream_close(stream);
}

ut_test(new_writeonly_stream_with_no_write_op_returns_EINVAL)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_ops_t ops =
    {
        .read_op = dummy_read,
        .write_op = NULL,
        .seek_op = NULL,
        .tell_op = NULL,
        .close_op = NULL
    };
    stream_t *stream;

    UT_ASSERT_EQUAL(-EINVAL, stream_open(&stream, dummy, &ops, STREAM_ACCESS_WRITE));
}

ut_test(new_writeonly_stream_with_write_op_succeeds)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_ops_t ops =
    {
        .read_op = NULL,
        .write_op = dummy_write,
        .seek_op = NULL,
        .tell_op = NULL,
        .close_op = NULL
    };
    stream_t *stream = NULL;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &ops, STREAM_ACCESS_WRITE));
    UT_ASSERT(stream != NULL);
    stream_close(stream);
}

ut_test(new_rw_stream_with_no_read_op_returns_EINVAL)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_ops_t ops =
    {
        .read_op = NULL,
        .write_op = dummy_write,
        .seek_op = NULL,
        .tell_op = NULL,
        .close_op = NULL
    };
    stream_t *stream;

    UT_ASSERT_EQUAL(-EINVAL, stream_open(&stream, dummy, &ops, STREAM_ACCESS_RW));
}

ut_test(new_rw_stream_with_no_write_op_returns_EINVAL)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_ops_t ops =
    {
        .read_op = dummy_read,
        .write_op = NULL,
        .seek_op = NULL,
        .tell_op = NULL,
        .close_op = NULL
    };
    stream_t *stream;

    UT_ASSERT_EQUAL(-EINVAL, stream_open(&stream, dummy, &ops, STREAM_ACCESS_RW));
}

ut_test(new_rw_stream_with_both_read_and_write_op_succeeds)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_ops_t ops =
    {
        .read_op = dummy_read,
        .write_op = dummy_write,
        .seek_op = NULL,
        .tell_op = NULL,
        .close_op = NULL
    };
    stream_t *stream = NULL;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &ops, STREAM_ACCESS_RW));
    UT_ASSERT(stream != NULL);
    stream_close(stream);
}

UT_SECTION(stream_free)

ut_test(freeing_sets_stream_to_null)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS_RW));

    stream_close(stream);
    UT_ASSERT(stream == NULL);
}

UT_SECTION(stream_read)

ut_test(reading_from_null_stream_returns_EINVAL)
{
    char buf[10];
    UT_ASSERT_EQUAL(-EINVAL, stream_read(NULL, buf, sizeof(buf)));
}

ut_test(reading_with_zero_size_does_nothing)
{
    stream_t *stream;
    char data[30];
    int i;

    /* Have to use a real stream implementation and set the underlying
       buffer to some known contents */
    memset(__buf, 'a', sizeof(__buf));
    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_READ));

    /* Set the data buffer to a contents different from the stream's
       underlying buffer */
    memset(data, 'z', sizeof(data));
    UT_ASSERT_EQUAL(0, stream_read(stream, data, 0));

    /* Check the data buffer was left untouched */
    for (i = 0; i < sizeof(data); i++)
        UT_ASSERT_EQUAL('z', data[i]);

    stream_close(stream);
}

ut_test(reading_with_null_buffer_and_zero_size_does_nothing)
{
    void *dummy = (void *)1;
    stream_t *stream;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS_READ));
    UT_ASSERT_EQUAL(0, stream_read(stream, NULL, 0));
    stream_close(stream);
}

ut_test(reading_with_null_buffer_and_non_zero_size_returns_EINVAL)
{
    void *dummy = (void *)1;
    stream_t *stream;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS_READ));
    UT_ASSERT_EQUAL(-EINVAL, stream_read(stream, NULL, 3));
    stream_close(stream);
}

ut_test(reading_from_writeonly_stream_returns_EOPNOTSUPP)
{
    void *dummy = (void *)1;
    stream_t *stream;
    char buf[5];

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS_WRITE));
    UT_ASSERT_EQUAL(-EOPNOTSUPP, stream_read(stream, buf, sizeof(buf)));
    stream_close(stream);
}

ut_test(reading_byte_per_byte_succeeds)
{
    stream_t *stream;
    size_t i;
    char c;

    /* Have to use a real stream implementation */
    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_READ));

    for (i = 0; i < sizeof(__buf); i++)
        __buf[i] = 'a';

    for (i = 0; i < sizeof(__buf); i++)
    {
        UT_ASSERT_EQUAL(1, stream_read(stream, &c, 1));
        UT_ASSERT_EQUAL('a', c);
    }

    stream_close(stream);
}

ut_test(reading_by_blobs_succeeds)
{
    stream_t *stream;
    size_t i;
    char c;
    size_t left;

    /* Write directly to the string */
    c = '0';
    for (i = 0; i < sizeof(__buf); i++)
    {
        __buf[i] = c++;
        if (c > '9')
            c = '0';
    }

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_READ));

    /* Read through the stream and check it's ok */
    left = sizeof(__buf);
    c = '0';
    while (left > 0)
    {
        char buf[3];
        size_t n = left < sizeof(buf) ? left : sizeof(buf);
        int j;

        UT_ASSERT_EQUAL(n, stream_read(stream, &buf, n));

        for (j = 0; j < n; j++)
        {
            UT_ASSERT_EQUAL(c++, buf[j]);
            if (c > '9')
                c = '0';
        }

        left -= n;
    }

    stream_close(stream);
}

ut_test(reading_passed_end_returns_zero_bytes)
{
    stream_t *stream;
    char c;
    size_t i;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_READ));

    for (i = 0; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL(1, stream_read(stream, &c, 1));

    UT_ASSERT_EQUAL(0, stream_read(stream, &c, 1));

    stream_close(stream);
}

ut_test(reading_more_than_available_returns_only_whats_available)
{
    stream_t *stream;
    size_t i;
    /* Buffer larger than the one accessed through the stream */
    char buf[sizeof(__buf) * 2];

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_READ));

    for (i = 0; i < sizeof(__buf); i++)
        __buf[i] = '0' + (i % 10);

    UT_ASSERT_EQUAL(sizeof(__buf), stream_read(stream, buf, sizeof(buf)));
    UT_ASSERT_EQUAL(0, memcmp(buf, __buf, sizeof(__buf)));

    stream_close(stream);
}

UT_SECTION(stream_write)

ut_test(writing_with_null_buffer_and_zero_size_does_nothing)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS_WRITE));
    UT_ASSERT_EQUAL(0, stream_write(stream, NULL, 0));
    stream_close(stream);
}

ut_test(writing_with_null_buffer_and_non_zero_size_returns_EINVAL)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS_WRITE));
    UT_ASSERT_EQUAL(-EINVAL, stream_write(stream, NULL, 3));
    stream_close(stream);
}

ut_test(writing_with_zero_size_does_nothing)
{
    stream_t *stream;
    char data[30];
    int i;

    /* Have to use a real stream implementation and set the underlying
       buffer to some known contents */
    memset(__buf, 'a', sizeof(__buf));
    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_READ));

    /* Set the data buffer to a contents different from the stream's
       underlying buffer */
    memset(data, 'z', sizeof(data));
    UT_ASSERT_EQUAL(0, stream_write(stream, data, 0));

    /* Check the stream's underlying buffer was left untouched */
    for (i = 0; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL('a', __buf[i]);

    stream_close(stream);
}

ut_test(writing_to_readonly_stream_returns_EOPNOTSUPP)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;
    char buf[5];

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS_READ));
    UT_ASSERT_EQUAL(-EOPNOTSUPP, stream_write(stream, buf, sizeof(buf)));
    stream_close(stream);
}

ut_test(writing_byte_per_byte_succeeds)
{
    stream_t *stream;
    size_t i;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_WRITE));

    for (i = 0; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL(1, stream_write(stream, "a", 1));

    for (i = 0; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL('a', __buf[i]);

    stream_close(stream);
}

ut_test(writing_by_blobs_succeeds)
{
    stream_t *stream;
    size_t i;
    char c;
    size_t left;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_WRITE));

    /* Write to the string using the stream */
    left = sizeof(__buf);
    c = '0';
    while (left > 0)
    {
        char buf[3];
        size_t n = left < sizeof(buf) ? left : sizeof(buf);
        int j;

        for (j = 0; j < n; j++)
        {
            buf[j] = c++;
            if (c > '9')
                c = '0';
        }

        UT_ASSERT_EQUAL(n, stream_write(stream, &buf, n));

        left -= n;
    }

    /* Read directly from the string and check it's ok */
    c = '0';
    for (i = 0; i < sizeof(__buf); i++)
    {
        UT_ASSERT_EQUAL(c++, __buf[i]);
        if (c > '9')
            c = '0';
    }

    stream_close(stream);
}

ut_test(writing_passed_end_returns_ENOSPC)
{
    stream_t *stream;
    size_t i;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_WRITE));

    for (i = 0; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL(1, stream_write(stream, "z", 1));

    UT_ASSERT_EQUAL(-ENOSPC, stream_write(stream, "z", 1));

    stream_close(stream);
}

UT_SECTION(stream_printf)

ut_test(printf_to_null_stream_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, stream_printf(NULL, "hello %s", "user"));
}

ut_test(printf_with_null_format_returns_EINVAL)
{
    stream_t *stream;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_WRITE));
    UT_ASSERT_EQUAL(-EINVAL, stream_printf(stream, NULL));
    stream_close(stream);
}

ut_test(printf_without_values)
{
    stream_t *stream;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_WRITE));
    UT_ASSERT_EQUAL(strlen("hello"), stream_printf(stream, "hello"));
    stream_close(stream);
}

ut_test(printf_with_values)
{
    stream_t *stream;
    char reference[128];
    int i, n;

    memset(__buf, '_', sizeof(__buf));

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_WRITE));

    n = os_snprintf(reference, sizeof(reference), "%s %s was born in %d", "Alan", "Turing", 1912);
    UT_ASSERT_EQUAL(n, stream_printf(stream, "%s %s was born in %d", "Alan", "Turing", 1912));

    for (i = 0; i < n; i++)
        UT_ASSERT_EQUAL(reference[i], __buf[i]);

    for (i = n; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL('_', __buf[i]);

    stream_close(stream);
}

UT_SECTION(stream_flush)

ut_test(flusing_null_stream_returns_EINVAL)
{
    UT_ASSERT(stream_flush(NULL) == -EINVAL);
}

ut_test(flushing_stream_with_no_flush_op_returns_EOPNOTSUPP)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;
    stream_ops_t ops =
    {
        .read_op = NULL,
        .write_op = dummy_write,
        .flush_op = NULL,
        .tell_op = NULL,
        .close_op = NULL
    };

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &ops, STREAM_ACCESS_WRITE));
    UT_ASSERT_EQUAL(-EOPNOTSUPP, stream_flush(stream));
    stream_close(stream);
}

ut_test(flushing_readonly_stream_returns_EOPNOTSUPP)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS_READ));
    UT_ASSERT_EQUAL(-EOPNOTSUPP, stream_flush(stream));
    stream_close(stream);
}

#define DELAYED_CONTENTS_SIZE  128

typedef struct
{
    char contents[DELAYED_CONTENTS_SIZE];
    uint64_t pos;
    char *latched_data;
    size_t latched_size;
} delayed_context_t;

static int __write_latched_data(delayed_context_t *ctx)
{
    if (ctx->latched_data != NULL)
    {
        memcpy(ctx->contents + ctx->pos, ctx->latched_data, ctx->latched_size);
        ctx->pos += ctx->latched_size;

        os_free(ctx->latched_data);
        ctx->latched_size = 0;
    }

    return 0;
}

static int delayed_write(void *v, const void *buf, size_t size)
{
    delayed_context_t *ctx = v;

    __write_latched_data(ctx);

    ctx->latched_data = os_malloc(size);
    memcpy(ctx->latched_data, buf, size);
    ctx->latched_size = size;

    return 0;
}

static int delayed_flush(void *v)
{
    return __write_latched_data((delayed_context_t *)v);
}

static uint64_t delayed_tell(void *v)
{
    return ((delayed_context_t *)v)->pos;
}

static const stream_ops_t delayed_ops =
{
    .read_op = NULL,
    .write_op = delayed_write,
    .flush_op = delayed_flush,
    .tell_op = delayed_tell,
    .close_op = NULL
};

ut_test(flush_does_write_pending_data)
{
    delayed_context_t delayed_context =
    {
        .contents = { 0 },
        .pos = 0,
        .latched_data = NULL,
        .latched_size = 0
    };
    stream_t *stream;
    int i;

    UT_ASSERT_EQUAL(0, stream_open(&stream, &delayed_context, &delayed_ops, STREAM_ACCESS_WRITE));

    UT_ASSERT_EQUAL(0, stream_write(stream, "first", strlen("first")));
    /* Check it's not written yet */
    UT_ASSERT(memcmp(delayed_context.contents, "first", strlen("first")) != 0);

    UT_ASSERT_EQUAL(0, stream_write(stream, "second", strlen("second")));
    /* Check the *first* data was written */
    UT_ASSERT_EQUAL(0, memcmp(delayed_context.contents, "first", strlen("first")));

    UT_ASSERT_EQUAL(0, stream_flush(stream));
    /* Check the *second* data was written too */
    UT_ASSERT_EQUAL(0, memcmp(delayed_context.contents, "firstsecond",
                              strlen("firstsecond")));

    stream_close(stream);

    /* Check nothing was written after the close since the stream was
       already flushed */
    UT_ASSERT_EQUAL(0, memcmp(delayed_context.contents, "firstsecond",
                              strlen("firstsecond")));
    for (i = strlen("firstsecond"); i < DELAYED_CONTENTS_SIZE; i++)
        UT_ASSERT_EQUAL(0, delayed_context.contents[i]);
}

ut_test(flush_is_idempotent)
{
    delayed_context_t delayed_context =
    {
        .contents = { 0 },
        .pos = 0,
        .latched_data = NULL,
        .latched_size = 0
    };
    stream_t *stream;
    uint64_t saved_pos;
    int i;

    UT_ASSERT_EQUAL(0, stream_open(&stream, &delayed_context, &delayed_ops, STREAM_ACCESS_WRITE));

    UT_ASSERT_EQUAL(0, stream_write(stream, "dummy", strlen("dummy")));
    /* Check it's not written yet */
    UT_ASSERT(memcmp(delayed_context.contents, "dummy", strlen("dummy")) != 0);

    UT_ASSERT_EQUAL(0, stream_flush(stream));
    /* Check it's now written */
    UT_ASSERT_EQUAL(0, memcmp(delayed_context.contents, "dummy", strlen("dummy")));
    saved_pos = stream_tell(stream);

    UT_ASSERT_EQUAL(0, stream_flush(stream));
    /* Check the contents of the stream hasn't changed */
    UT_ASSERT_EQUAL(saved_pos, stream_tell(stream));
    UT_ASSERT_EQUAL(0, memcmp(delayed_context.contents, "dummy", strlen("dummy")));
    for (i = strlen("dummy"); i < DELAYED_CONTENTS_SIZE; i++)
        UT_ASSERT_EQUAL(0, delayed_context.contents[i]);

    stream_close(stream);
}

ut_test(close_performs_flush)
{
    delayed_context_t delayed_context =
    {
        .contents = { 0 },
        .pos = 0,
        .latched_data = NULL,
        .latched_size = 0
    };
    stream_t *stream;

    UT_ASSERT_EQUAL(0, stream_open(&stream, &delayed_context, &delayed_ops, STREAM_ACCESS_WRITE));

    UT_ASSERT_EQUAL(0, stream_write(stream, "velvet", strlen("velvet")));
    /* Check it's not written yet */
    UT_ASSERT(memcmp(delayed_context.contents, "velvet", strlen("velvet")) != 0);

    stream_close(stream);

    /* Check the close did flush the pending data */
    UT_ASSERT_EQUAL(0, memcmp(delayed_context.contents, "velvet", strlen("velvet")));
}

UT_SECTION(stream_seek_rewind_and_tell)

ut_test(seeking_on_non_seekable_stream_returns_EOPNOTSUPP)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;
    stream_ops_t ops =
    {
        .read_op = dummy_read,
        .write_op = dummy_write,
        .seek_op = NULL,
        .tell_op = NULL,
        .close_op = NULL
    };

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &ops, STREAM_ACCESS_RW));

    UT_ASSERT_EQUAL(-EOPNOTSUPP, stream_seek(stream, 1, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(-EOPNOTSUPP, stream_rewind(stream));

    stream_close(stream);
}

ut_test(seeking_before_beginning_returns_EINVAL)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &dummy_ops, STREAM_ACCESS_RW));
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(stream, -1, STREAM_SEEK_FROM_BEGINNING));
    stream_close(stream);
}

ut_test(seeking_at_valid_position_succeeds)
{
    stream_t *stream;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_RW));

    UT_ASSERT_EQUAL(0, stream_seek(stream, sizeof(__buf) / 2, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(sizeof(__buf) / 2, stream_tell(stream));

    stream_close(stream);
}

ut_test(seeking_at_beginning_succeeds)
{
    stream_t *stream;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_RW));

    stream_seek(stream, 5, STREAM_SEEK_FROM_BEGINNING);

    UT_ASSERT_EQUAL(0, stream_seek(stream, 0, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(0, stream_tell(stream));

    stream_close(stream);
}

ut_test(rewind_succeeds)
{
    stream_t *stream;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_RW));

    stream_seek(stream, 5, STREAM_SEEK_FROM_BEGINNING);

    UT_ASSERT_EQUAL(0, stream_rewind(stream));
    UT_ASSERT_EQUAL(0, stream_tell(stream));

    stream_close(stream);
}

ut_test(seeking_after_end_returns_EINVAL)
{
    stream_t *stream;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_RW));
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(stream, +1, STREAM_SEEK_FROM_END));
    stream_close(stream);
}

ut_test(seeking_at_end_succeeds)
{
    stream_t *stream;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_RW));

    UT_ASSERT_EQUAL(0, stream_seek(stream, 0, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(sizeof(__buf), stream_tell(stream));

    stream_close(stream);
}

ut_test(relative_seek_out_of_bounds_returns_EINVAL)
{
    stream_t *stream;

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, __buf, sizeof(__buf), STREAM_ACCESS_RW));

    UT_ASSERT_EQUAL(-EINVAL, stream_seek(stream, -sizeof(__buf) - 1, STREAM_SEEK_FROM_POS));
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(stream, +sizeof(__buf) + 1, STREAM_SEEK_FROM_POS));

    stream_close(stream);
}

ut_test(tell_on_non_tellable_stream_returns_error)
{
    /* No need to have an actually valid context here */
    void *dummy = (void *)1;
    stream_t *stream;
    stream_ops_t ops =
    {
        .read_op = dummy_read,
        .write_op = dummy_write,
        .seek_op = dummy_seek,
        .tell_op = NULL,
        .close_op = NULL
    };

    UT_ASSERT_EQUAL(0, stream_open(&stream, dummy, &ops, STREAM_ACCESS_RW));
    UT_ASSERT_EQUAL(STREAM_TELL_ERROR, stream_tell(stream));
    stream_close(stream);
}
