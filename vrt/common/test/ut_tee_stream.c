/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/common/include/tee_stream.h"
#include "vrt/common/include/memory_stream.h"

#define BUFFER_SIZE  256

typedef char buffer_t[BUFFER_SIZE];

static buffer_t bufs[2];
static stream_t *ms[2];
static stream_t *ts;

ut_test(building_tee_stream_on_non_writable_substream_returns_EPERM)
{
    int err;

    err = memory_stream_open(&ms[0], bufs[0], sizeof(buffer_t), STREAM_ACCESS_READ);
    UT_ASSERT_EQUAL(0, err);

    err = memory_stream_open(&ms[1], bufs[1], sizeof(buffer_t), STREAM_ACCESS_WRITE);
    UT_ASSERT_EQUAL(0, err);

    err = tee_stream_open(&ts, ms[0], ms[1]);
    UT_ASSERT_EQUAL(-EPERM, err);
}

ut_test(building_tee_stream_on_write_only_substreams_succeeds)
{
    stream_access_t a0, a1;
    int err;

    for (a0 = STREAM_ACCESS__FIRST; a0 <= STREAM_ACCESS__LAST; a0++)
    {
        if (a0 == STREAM_ACCESS_READ)
            continue;

        for (a1 = STREAM_ACCESS__FIRST; a1 <= STREAM_ACCESS__LAST; a1++)
        {
            if (a1 == STREAM_ACCESS_READ)
                continue;

            err = memory_stream_open(&ms[0], bufs[0], sizeof(buffer_t), a0);
            UT_ASSERT_EQUAL(0, err);

            err = memory_stream_open(&ms[1], bufs[1], sizeof(buffer_t), a1);
            UT_ASSERT_EQUAL(0, err);

            err = tee_stream_open(&ts, ms[0], ms[1]);
            UT_ASSERT_EQUAL(0, err);

            stream_close(ts);

            stream_close(ms[0]);
            stream_close(ms[1]);
        }
    }
}

ut_test(writing_to_tee_stream_yields_identical_data_on_substreams)
{
    static char *words[] = {
        "tiger", "tiger", "burning", "bright",
        "in", "the", "forests", "of", "the", "night",
        NULL
    };
    uint64_t end_ts_ofs;
    int i, k;
    int err;

    for (i = 0; i < 2; i++)
    {
        err = memory_stream_open(&ms[i], bufs[i], sizeof(buffer_t), STREAM_ACCESS_RW);
        UT_ASSERT_EQUAL(0, err);
    }

    err = tee_stream_open(&ts, ms[0], ms[1]);
    UT_ASSERT_EQUAL(0, err);

    /* Write to the tee stream */
    for (k = 0; words[k] != NULL; k++)
    {
        size_t len = strlen(words[k]);
        UT_ASSERT_EQUAL(len, stream_write(ts, words[k], len));
    }
    end_ts_ofs = stream_tell(ts);

    stream_close(ts);

    /* Check each substream */
    for (i = 0; i < 2; i++)
    {
        UT_ASSERT_EQUAL(end_ts_ofs, stream_tell(ms[i]));

        stream_rewind(ms[i]);
        for (k = 0; words[k] != NULL; k++)
        {
            size_t len = strlen(words[k]);
            buffer_t tmp;

            UT_ASSERT_EQUAL(len, stream_read(ms[i], tmp, len));
            UT_ASSERT_EQUAL(0, memcmp(tmp, words[k], len));
        }
    }

    for (i = 0; i < 2; i++)
        stream_close(ms[i]);
}

ut_test(seeking_on_tee_stream_yields_identical_offsets_on_substreams)
{
    typedef struct
    {
        int64_t ofs;
        stream_seek_t seek;
        uint64_t expected;
    } blurb_t;
    blurb_t blurbs[] = {
        { .ofs =  5, .seek = STREAM_SEEK_FROM_BEGINNING, .expected =  5 },
        { .ofs = 20, .seek = STREAM_SEEK_FROM_BEGINNING, .expected = 20 },
        { .ofs = -3, .seek = STREAM_SEEK_FROM_POS,       .expected = 17 },
        { .ofs = -7, .seek = STREAM_SEEK_FROM_END,       .expected = sizeof(buffer_t) - 7 }
    };
#define NUM_BLURBS  (sizeof(blurbs) / sizeof(blurb_t))
    int i, k;
    int err;

    for (i = 0; i < 2; i++)
    {
        err = memory_stream_open(&ms[i], bufs[i], sizeof(buffer_t), STREAM_ACCESS_RW);
        UT_ASSERT_EQUAL(0, err);
    }

    err = tee_stream_open(&ts, ms[0], ms[1]);
    UT_ASSERT_EQUAL(0, err);

    for (k = 0; k < NUM_BLURBS; k++)
    {
        UT_ASSERT_EQUAL(0, stream_seek(ts, blurbs[k].ofs, blurbs[k].seek));
        UT_ASSERT_EQUAL(blurbs[k].expected, stream_tell(ts));
    }

    stream_close(ts);

    for (i = 0; i < 2; i++)
        stream_close(ms[i]);
}

