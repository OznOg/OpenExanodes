/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/common/include/memory_stream.h"

#include "os/include/os_error.h"

static char __buf[20];
static stream_t *r_stream;
static stream_t *w_stream;
static stream_t *rw_stream;

UT_SECTION(memory_stream_open)

ut_test(new_memory_stream_on_null_buffer_returns_EINVAL)
{
    stream_t *stream;
    UT_ASSERT_EQUAL(-EINVAL, memory_stream_open(&stream, NULL, 23, STREAM_ACCESS_READ));
}

ut_test(new_stream_with_invalid_access_returns_EINVAL)
{
    stream_t *stream;
    char buf[16];

    UT_ASSERT_EQUAL(-EINVAL, memory_stream_open(&stream, buf, sizeof(buf), STREAM_ACCESS_READ - 1));
    UT_ASSERT_EQUAL(-EINVAL, memory_stream_open(&stream, buf, sizeof(buf), STREAM_ACCESS_RW + 1));
}

UT_SECTION(stream_read)

ut_setup()
{
    memory_stream_open(&r_stream, __buf, sizeof(__buf), STREAM_ACCESS_READ);
    memory_stream_open(&w_stream, __buf, sizeof(__buf), STREAM_ACCESS_WRITE);
    memory_stream_open(&rw_stream, __buf, sizeof(__buf), STREAM_ACCESS_RW);
}

ut_cleanup()
{
    stream_close(r_stream);
    stream_close(w_stream);
    stream_close(rw_stream);
}

ut_test(reading_with_null_buffer_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, stream_read(r_stream, NULL, 3));
}

ut_test(reading_from_writeonly_stream_returns_EOPNOTSUPP)
{
    char buf[5];

    UT_ASSERT_EQUAL(-EOPNOTSUPP, stream_read(w_stream, buf, sizeof(buf)));
}

ut_test(reading_byte_per_byte_succeeds)
{
    size_t i;
    char c;

    for (i = 0; i < sizeof(__buf); i++)
        __buf[i] = 'a';

    for (i = 0; i < sizeof(__buf); i++)
    {
        UT_ASSERT_EQUAL(1, stream_read(r_stream, &c, 1));
        UT_ASSERT_EQUAL('a', c);
    }
}

ut_test(reading_by_blobs_succeeds)
{
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

    /* Read through the stream and check it's ok */
    left = sizeof(__buf);
    c = '0';
    while (left > 0)
    {
        char buf[3];
        size_t n = left < sizeof(buf) ? left : sizeof(buf);
        int j;

        UT_ASSERT_EQUAL(n, stream_read(r_stream, &buf, n));

        for (j = 0; j < n; j++)
        {
            UT_ASSERT_EQUAL(c++, buf[j]);
            if (c > '9')
                c = '0';
        }

        left -= n;
    }
}

ut_test(reading_passed_end_returns_zero_bytes)
{
    char c;
    size_t i;

    for (i = 0; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL(1, stream_read(r_stream, &c, 1));

    UT_ASSERT_EQUAL(0, stream_read(r_stream, &c, 1));
}

ut_test(reading_more_than_available_returns_only_whats_available)
{
    /* Buffer larger than the one accessed through the stream */
    char buf[sizeof(__buf) * 2];
    size_t i;

    for (i = 0; i < sizeof(__buf); i++)
        __buf[i] = '0' + (i % 10);

    UT_ASSERT_EQUAL(sizeof(__buf), stream_read(r_stream, buf, sizeof(buf)));
    UT_ASSERT_EQUAL(0, memcmp(buf, __buf, sizeof(__buf)));
}

UT_SECTION(stream_write)

ut_setup()
{
    memory_stream_open(&r_stream, __buf, sizeof(__buf), STREAM_ACCESS_READ);
    memory_stream_open(&w_stream, __buf, sizeof(__buf), STREAM_ACCESS_WRITE);
}

ut_cleanup()
{
    stream_close(r_stream);
    stream_close(w_stream);
}

ut_test(writing_with_null_buffer_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, stream_write(w_stream, NULL, 3));
}

ut_test(writing_to_readonly_stream_returns_EOPNOTSUPP)
{
    char buf[5] = { 0 };

    UT_ASSERT_EQUAL(-EOPNOTSUPP, stream_write(r_stream, buf, sizeof(buf)));
}

ut_test(writing_byte_per_byte_succeeds)
{
    size_t i;

    for (i = 0; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL(1, stream_write(w_stream, "a", 1));

    for (i = 0; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL('a', __buf[i]);
}

ut_test(writing_by_blobs_succeeds)
{
    size_t i;
    char c;
    size_t left;

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

        UT_ASSERT_EQUAL(n, stream_write(w_stream, &buf, n));

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
}

ut_test(writing_passed_end_returns_ENOSPC)
{
    size_t i;

    for (i = 0; i < sizeof(__buf); i++)
        UT_ASSERT_EQUAL(1, stream_write(w_stream, "z", 1));

    UT_ASSERT_EQUAL(-ENOSPC, stream_write(w_stream, "z", 1));
}

UT_SECTION(stream_seek_and_tell)

ut_setup()
{
    memory_stream_open(&r_stream, __buf, sizeof(__buf), STREAM_ACCESS_READ);
}

ut_cleanup()
{
    stream_close(r_stream);
}

ut_test(seeking_before_beginning_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(r_stream, -1, STREAM_SEEK_FROM_BEGINNING));
}

ut_test(seeking_at_beginning_succeeds)
{
    UT_ASSERT_EQUAL(0, stream_rewind(r_stream));
    UT_ASSERT_EQUAL(0, stream_tell(r_stream));
}

ut_test(seeking_after_end_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(r_stream, +1, STREAM_SEEK_FROM_END));
}

ut_test(seeking_at_end_succeeds)
{
    UT_ASSERT_EQUAL(0, stream_seek(r_stream, 0, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(sizeof(__buf), stream_tell(r_stream));
}

ut_test(seeking_from_end_succeeds)
{
    UT_ASSERT_EQUAL(0, stream_seek(r_stream, -5, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(sizeof(__buf) - 5, stream_tell(r_stream));
}

ut_test(relative_seek_out_of_bounds_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(r_stream, -sizeof(__buf) - 1, STREAM_SEEK_FROM_POS));
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(r_stream, +sizeof(__buf) + 1, STREAM_SEEK_FROM_POS));
}

UT_SECTION(multiple_streams_on_single_object)

ut_setup()
{
    memory_stream_open(&r_stream, __buf, sizeof(__buf), STREAM_ACCESS_READ);
    memory_stream_open(&w_stream, __buf, sizeof(__buf), STREAM_ACCESS_WRITE);
    memory_stream_open(&rw_stream, __buf, sizeof(__buf), STREAM_ACCESS_RW);
}

ut_cleanup()
{
    stream_close(r_stream);
    stream_close(w_stream);
    stream_close(rw_stream);
}

ut_test(write_with_one_stream_and_read_with_another)
{
    char buf[8];

    UT_ASSERT_EQUAL(8, stream_write(w_stream, "abcdefgh", 8));

    UT_ASSERT_EQUAL(3, stream_read(r_stream, buf, 3));
    UT_ASSERT_EQUAL(0, memcmp(buf, "abc", 3));

    UT_ASSERT_EQUAL(1, stream_read(r_stream, buf, 1));
    UT_ASSERT_EQUAL('d', buf[0]);

    UT_ASSERT_EQUAL(4, stream_read(r_stream, buf, 4));
    UT_ASSERT_EQUAL(0, memcmp(buf, "efgh", 4));
}

ut_test(multiple_streams_writing_to_same_object)
{
    char buf[10];

    UT_ASSERT_EQUAL(7, stream_write(w_stream, "bonjour", 7));
    UT_ASSERT_EQUAL(5, stream_write(rw_stream, "HELLO", 5));

    UT_ASSERT_EQUAL(0, memcmp(__buf, "HELLOur", 7));

    UT_ASSERT_EQUAL(0, stream_seek(w_stream, 2, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(3, stream_write(w_stream, "!!!", 3));

    UT_ASSERT_EQUAL(0, stream_rewind(rw_stream));
    UT_ASSERT_EQUAL(7, stream_read(rw_stream, buf, 7));

    UT_ASSERT_EQUAL(0, memcmp(__buf, "HE!!!ur", 7));
}
