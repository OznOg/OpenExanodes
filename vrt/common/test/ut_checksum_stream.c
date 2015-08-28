/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/checksum_stream.h"
#include "vrt/common/include/memory_stream.h"

#include "common/include/checksum.h"

#include <unit_testing.h>

#define BUF_SIZE 100
#define STRING_IN "nous partimes 500 et par un prompt renfort nous nous vimes 3000 en arrivant au port"
#define STRING_OUT "bla bla bla bla bla bla bla bla bla bla bla bla bla bla bla bla bla bla bla bla"

static char __buf[BUF_SIZE];
static stream_t *rw_stream;

UT_SECTION(checksum_stream_open)

ut_setup()
{
    UT_ASSERT_EQUAL(0, memory_stream_open(&rw_stream, __buf, sizeof(__buf),
                                          STREAM_ACCESS_RW));
}

ut_cleanup()
{
    stream_close(rw_stream);
}

ut_test(open_on_null_base_stream_returns_null)
{
    stream_t *checksum_stream;
    UT_ASSERT_EQUAL(-EINVAL, checksum_stream_open(&checksum_stream, NULL));
}


ut_test(open_with_valid_args_returns_valid_stream)
{
    stream_t *checksum_stream;

    UT_ASSERT_EQUAL(0, checksum_stream_open(&checksum_stream, rw_stream));
    stream_close(checksum_stream);
}

UT_SECTION(checksum_stream_read_write_seek)

ut_setup()
{
    UT_ASSERT_EQUAL(0, memory_stream_open(&rw_stream, __buf, sizeof(__buf), STREAM_ACCESS_RW));
}

ut_cleanup()
{
    stream_close(rw_stream);
}

ut_test(read_write_redirection)
{
    stream_t *checksum_stream;
    char buf_in[BUF_SIZE] = STRING_IN;
    char buf_out[BUF_SIZE] = STRING_OUT;

    checksum_stream_open(&checksum_stream, rw_stream);
    stream_write(checksum_stream, buf_in, BUF_SIZE);
    stream_close(checksum_stream);

    UT_ASSERT_EQUAL(0, memcmp(__buf, buf_in, BUF_SIZE));

    stream_seek(rw_stream, 0, STREAM_SEEK_FROM_BEGINNING);

    checksum_stream_open(&checksum_stream, rw_stream);
    stream_read(checksum_stream, buf_out, BUF_SIZE);
    stream_close(checksum_stream);

    UT_ASSERT_EQUAL(0, memcmp(buf_out, buf_in, BUF_SIZE));
}

ut_test(checksum_is_correct_with_read_write_at_once)
{
    stream_t *checksum_stream;
    char buf_in[BUF_SIZE] = STRING_IN;
    char buf_out[BUF_SIZE] = STRING_OUT;
    checksum_context_t ref_ctx;

    checksum_reset(&ref_ctx);
    checksum_feed(&ref_ctx, buf_in, BUF_SIZE);

    checksum_stream_open(&checksum_stream, rw_stream);

    stream_write(checksum_stream, buf_in, BUF_SIZE);

    UT_ASSERT_EQUAL(checksum_get_value(&ref_ctx),
                    checksum_stream_get_value(checksum_stream));

    stream_rewind(checksum_stream);

    UT_ASSERT_EQUAL(0, checksum_stream_get_value(checksum_stream));

    stream_read(checksum_stream, buf_out, BUF_SIZE);

    UT_ASSERT_EQUAL(checksum_get_value(&ref_ctx),
                    checksum_stream_get_value(checksum_stream));

    stream_close(checksum_stream);
}

ut_test(checksum_is_correct_with_read_write_by_char)
{
    stream_t *checksum_stream;
    char buf_in[BUF_SIZE] = STRING_IN;
    char buf_out[BUF_SIZE] = STRING_OUT;
    unsigned int i;
    checksum_context_t ref_ctx;

    checksum_reset(&ref_ctx);
    checksum_feed(&ref_ctx, buf_in, BUF_SIZE);

    checksum_stream_open(&checksum_stream, rw_stream);
    stream_write(checksum_stream, buf_in, BUF_SIZE);

    UT_ASSERT_EQUAL(checksum_get_value(&ref_ctx),
                    checksum_stream_get_value(checksum_stream));

    stream_rewind(checksum_stream);

    UT_ASSERT_EQUAL(0, checksum_stream_get_value(checksum_stream));

    for (i = 0; i < BUF_SIZE; i++)
        stream_read(checksum_stream, &buf_out[i], 1);

    UT_ASSERT_EQUAL(checksum_get_value(&ref_ctx),
                    checksum_stream_get_value(checksum_stream));

    stream_close(checksum_stream);
}

ut_test(checksum_is_correct_when_trying_to_read_more_than_written)
{
    stream_t *checksum_stream;
    char buf_in[BUF_SIZE] = STRING_IN;
    char buf_out[BUF_SIZE * 2] = STRING_OUT;
    checksum_context_t ref_ctx;
    int ret;

    checksum_reset(&ref_ctx);
    checksum_feed(&ref_ctx, buf_in, BUF_SIZE);

    checksum_stream_open(&checksum_stream, rw_stream);

    stream_write(checksum_stream, buf_in, BUF_SIZE);

    UT_ASSERT_EQUAL(checksum_get_value(&ref_ctx),
                    checksum_stream_get_value(checksum_stream));

    stream_rewind(checksum_stream);

    ret = stream_read(checksum_stream, buf_out, BUF_SIZE * 2);

    UT_ASSERT_EQUAL(ret, BUF_SIZE);
    UT_ASSERT_EQUAL(checksum_get_value(&ref_ctx),
                    checksum_stream_get_value(checksum_stream));

    stream_close(checksum_stream);
}

ut_test(checksum_is_zero_when_trying_to_read_empty_stream)
{
    stream_t *checksum_stream;
    char buf_out[BUF_SIZE] = STRING_OUT;
    int ret;

    /* pseudo empty string */
    stream_seek(rw_stream, 0, STREAM_SEEK_FROM_END);

    checksum_stream_open(&checksum_stream, rw_stream);

    ret = stream_read(checksum_stream, buf_out, BUF_SIZE);
    UT_ASSERT_EQUAL(ret, 0);

    UT_ASSERT_EQUAL(0, checksum_stream_get_value(checksum_stream));

    stream_close(checksum_stream);
}

ut_test(checksum_is_reset_by_rewind)
{
    stream_t *checksum_stream;
    char buf_in[BUF_SIZE] = STRING_IN;
    char buf_out[BUF_SIZE] = STRING_OUT;

    checksum_stream_open(&checksum_stream, rw_stream);

    stream_write(checksum_stream, buf_in, BUF_SIZE);
    UT_ASSERT(checksum_stream_get_value(checksum_stream) != 0);

    stream_rewind(checksum_stream);
    UT_ASSERT_EQUAL(0, checksum_stream_get_value(checksum_stream));

    stream_read(checksum_stream, buf_out, BUF_SIZE);
    UT_ASSERT(checksum_stream_get_value(checksum_stream) != 0);

    stream_rewind(checksum_stream);
    UT_ASSERT_EQUAL(0, checksum_stream_get_value(checksum_stream));

    stream_close(checksum_stream);
}

ut_test(seek_is_not_supported_except_rewind)
{
    stream_t *checksum_stream;

    checksum_stream_open(&checksum_stream, rw_stream);

    UT_ASSERT_EQUAL(-EINVAL, stream_seek(checksum_stream, 1, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(checksum_stream, 1, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(checksum_stream, 1, STREAM_SEEK_FROM_POS));
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(checksum_stream, 0, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(checksum_stream, 0, STREAM_SEEK_FROM_POS));

    stream_close(checksum_stream);
}

ut_test(size_is_correctly_computed)
{
    stream_t *checksum_stream;
    char buf_in[BUF_SIZE] = STRING_IN;
    char buf_out[BUF_SIZE] = STRING_OUT;
    int ret;

    checksum_stream_open(&checksum_stream, rw_stream);

    ret = stream_write(checksum_stream, buf_in, BUF_SIZE / 2);
    UT_ASSERT_EQUAL(BUF_SIZE / 2, checksum_stream_get_size(checksum_stream));
    UT_ASSERT_EQUAL(ret, BUF_SIZE / 2);

    ret = stream_write(checksum_stream, buf_in, BUF_SIZE / 2);
    UT_ASSERT_EQUAL(BUF_SIZE, checksum_stream_get_size(checksum_stream));
    UT_ASSERT_EQUAL(ret, BUF_SIZE / 2);

    stream_rewind(checksum_stream);
    UT_ASSERT_EQUAL(0, checksum_stream_get_value(checksum_stream));

    ret = stream_read(checksum_stream, buf_out, BUF_SIZE / 2);
    UT_ASSERT_EQUAL(BUF_SIZE / 2, checksum_stream_get_size(checksum_stream));
    UT_ASSERT_EQUAL(ret, BUF_SIZE / 2);

    ret = stream_read(checksum_stream, buf_out, BUF_SIZE * 2);
    UT_ASSERT_EQUAL(BUF_SIZE, checksum_stream_get_size(checksum_stream));
    UT_ASSERT_EQUAL(BUF_SIZE / 2, ret);

    stream_rewind(checksum_stream);
    UT_ASSERT_EQUAL(0, checksum_stream_get_value(checksum_stream));

    stream_close(checksum_stream);
}
