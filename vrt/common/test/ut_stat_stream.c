/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/common/include/stat_stream.h"
#include "vrt/common/include/memory_stream.h"

static char __buf[20];
static stream_t *rw_stream;

static stream_stats_t stats;
static stream_t *stat_stream;

UT_SECTION(stat_stream_open)

ut_setup()
{
    UT_ASSERT_EQUAL(0, memory_stream_open(&rw_stream, __buf, sizeof(__buf),
                                          STREAM_ACCESS_RW));
}

ut_cleanup()
{
    stream_close(rw_stream);
}

ut_test(stat_stream_on_null_base_stream_returns_EINVAL)
{
    stream_stats_t s;
    UT_ASSERT_EQUAL(-EINVAL, stat_stream_open(&stat_stream, NULL, &s));
}


ut_test(stat_stream_with_null_stats_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, stat_stream_open(&stat_stream, rw_stream, NULL));
}

UT_SECTION(stat_stream_read_write_seek_tell)

ut_setup()
{
    UT_ASSERT_EQUAL(0, memory_stream_open(&rw_stream, __buf, sizeof(__buf),
                                          STREAM_ACCESS_RW));
}

ut_cleanup()
{
    stream_close(rw_stream);
}

ut_test(stats_are_zero_when_no_operation_performed)
{
    UT_ASSERT_EQUAL(0, stat_stream_open(&stat_stream, rw_stream, &stats));
    UT_ASSERT(stat_stream != NULL);

    stream_close(stat_stream);

    UT_ASSERT_EQUAL(0, stats.read_stats.op_count);
    UT_ASSERT_EQUAL(0, stats.read_stats.error_count);
    UT_ASSERT_EQUAL(0, stats.read_stats.total_bytes);

    UT_ASSERT_EQUAL(0, stats.write_stats.op_count);
    UT_ASSERT_EQUAL(0, stats.write_stats.error_count);
    UT_ASSERT_EQUAL(0, stats.write_stats.total_bytes);

    /* The flush count is ONE, not zero, because the closing of a
       flushable stream calls flush at close. */
    UT_ASSERT_EQUAL(1, stats.flush_stats.op_count);
    UT_ASSERT_EQUAL(0, stats.flush_stats.error_count);
    UT_ASSERT_EQUAL(0, stats.flush_stats.total_bytes);

    UT_ASSERT_EQUAL(0, stats.seek_stats.op_count);
    UT_ASSERT_EQUAL(0, stats.seek_stats.error_count);
    UT_ASSERT_EQUAL(0, stats.seek_stats.total_bytes);

    UT_ASSERT_EQUAL(0, stats.tell_stats.op_count);
    UT_ASSERT_EQUAL(0, stats.tell_stats.error_count);
    UT_ASSERT_EQUAL(0, stats.tell_stats.total_bytes);
}

ut_test(stats_match_operations_performed_even_after_stream_closed)
{
    uint64_t pos;
    char buf[8];

    UT_ASSERT_EQUAL(0, stat_stream_open(&stat_stream, rw_stream, &stats));
    UT_ASSERT(stat_stream != NULL);

    stream_write(stat_stream, "just", strlen("just"));
    stream_write(stat_stream, "a", strlen("a"));
    stream_write(stat_stream, "lot", strlen("lot"));

    stream_seek(stat_stream, -3, STREAM_SEEK_FROM_POS);
    stream_write(stat_stream, "few", strlen("few"));

    stream_write(stat_stream, "words", strlen("words"));

    UT_ASSERT_EQUAL(0, stream_seek(stat_stream, 5, STREAM_SEEK_FROM_BEGINNING));
    pos = stream_tell(stat_stream);
    UT_ASSERT_EQUAL(5, pos);

    UT_ASSERT_EQUAL(8, stream_read(stat_stream, buf, 8));
    UT_ASSERT_EQUAL(0, memcmp(buf, "fewwords", 8));

    UT_ASSERT_EQUAL(0, stream_seek(stat_stream, 0, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(-ENOSPC, stream_write(stat_stream, "not_written", strlen("not_written")));

    UT_ASSERT_EQUAL(0, stream_rewind(stat_stream));
    UT_ASSERT_EQUAL(4, stream_read(stat_stream, buf, 4));
    UT_ASSERT_EQUAL(0, memcmp(buf, "just", 4));

    stream_close(stat_stream);

    UT_ASSERT_EQUAL(1 + 1, stats.read_stats.op_count);
    UT_ASSERT_EQUAL(0, stats.read_stats.error_count);
    UT_ASSERT_EQUAL(8 + 4, stats.read_stats.total_bytes);

    UT_ASSERT_EQUAL(6, stats.write_stats.op_count);
    UT_ASSERT_EQUAL(1, stats.write_stats.error_count);
    UT_ASSERT_EQUAL(strlen("just") + strlen("a") + strlen("lot") + strlen("few")
                    + strlen("words"), stats.write_stats.total_bytes);

    UT_ASSERT_EQUAL(4, stats.seek_stats.op_count);
    UT_ASSERT_EQUAL(0, stats.seek_stats.error_count);
    UT_ASSERT_EQUAL(-(-3) + 5 + 0, stats.seek_stats.total_bytes);

    UT_ASSERT_EQUAL(1, stats.tell_stats.op_count);
    UT_ASSERT_EQUAL(0, stats.tell_stats.error_count);
}
