/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/common/include/narrowed_stream.h"
#include "vrt/common/include/memory_stream.h"

static char __buf[64];
static stream_t *mem_stream;
static stream_t *narrow_stream;

#define NARROW_STREAM_START  5
#define NARROW_STREAM_END    29

static void __dump_buf(void)
{
    int i;

    /* REMOVE_ME */
    for (i = 0; i < sizeof(__buf); i++)
        if (i % 10 == 0)
            printf("%d", i / 10);
        else
            printf(" ");
    printf("\n");
    for (i = 0; i < sizeof(__buf); i++)
        printf("%d", i % 10);
    printf("\n");
    for (i = 0; i < sizeof(__buf); i++)
        printf("%c", __buf[i]);
    printf("\n");
}

static void __common_setup(void)
{
    int i;
    int err;

    for (i = 0; i < sizeof(__buf); i++)
        if (i >= NARROW_STREAM_START && i <= NARROW_STREAM_END)
            __buf[i] = 'n'; /* narrow */
        else
            __buf[i] = 'b'; /* base */

    __dump_buf();

    err = memory_stream_open(&mem_stream, __buf, sizeof(__buf), STREAM_ACCESS_RW);
    UT_ASSERT_EQUAL(0, err);

    err = narrowed_stream_open(&narrow_stream, mem_stream,
                               NARROW_STREAM_START, NARROW_STREAM_END,
                               stream_access(mem_stream));
    UT_ASSERT_EQUAL(0, err);

    ut_printf("mem_stream ofs   : %"PRIu64, stream_tell(mem_stream));
    ut_printf("narrow_stream ofs: %"PRIu64, stream_tell(narrow_stream));
}

static void __common_cleanup(void)
{
    stream_close(narrow_stream);
    stream_close(mem_stream);
}

UT_SECTION(narrowed_stream_open)

ut_setup()
{
    __common_setup();
}

ut_cleanup()
{
    __common_cleanup();
}

ut_test(narrowed_stream_on_null_stream_returns_EINVAL)
{
    stream_t *stream;
    UT_ASSERT_EQUAL(-EINVAL, narrowed_stream_open(&stream, NULL, 0, 128,
                                                  STREAM_ACCESS_RW));
}

ut_test(narrowed_stream_with_end_beyond_substream_end_returns_EINVAL)
{
    uint64_t end = stream_tell(mem_stream);
    stream_t *stream;

    UT_ASSERT_EQUAL(-EINVAL, narrowed_stream_open(&stream, NULL, 0, end + 1,
                                                  STREAM_ACCESS_RW));
}

UT_SECTION(narrowed_stream_seek)

ut_setup()
{
    __common_setup();
}

ut_cleanup()
{
    __common_cleanup();
}

ut_test(seeking_seeks_to_proper_position_on_substream)
{
    uint64_t ofs;

    /* Start */
    UT_ASSERT_EQUAL(0, stream_rewind(narrow_stream));
    UT_ASSERT_EQUAL(NARROW_STREAM_START, stream_tell(mem_stream));

    /* Middle */
    ofs = (NARROW_STREAM_END - NARROW_STREAM_START + 1) / 2;
    UT_ASSERT_EQUAL(0, stream_seek(narrow_stream, ofs, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(NARROW_STREAM_START + ofs, stream_tell(mem_stream));

    /* End */
    UT_ASSERT_EQUAL(0, stream_seek(narrow_stream, 0, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(NARROW_STREAM_END, stream_tell(mem_stream));
}

ut_test(seeking_before_start_leaves_substream_position_unchanged)
{
    uint64_t saved_ofs;

    saved_ofs = stream_tell(mem_stream);
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(narrow_stream, -1, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(saved_ofs, stream_tell(mem_stream));

    UT_ASSERT_EQUAL(0, stream_seek(narrow_stream, 5, STREAM_SEEK_FROM_BEGINNING));
    saved_ofs = stream_tell(mem_stream);
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(narrow_stream, -6, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(saved_ofs, stream_tell(mem_stream));
}

ut_test(seeking_after_end_leaves_substream_position_unchanged)
{
    uint64_t saved_ofs;

    saved_ofs = stream_tell(mem_stream);
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(narrow_stream, +1, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(saved_ofs, stream_tell(mem_stream));

    UT_ASSERT_EQUAL(0, stream_seek(narrow_stream, -5, STREAM_SEEK_FROM_END));
    saved_ofs = stream_tell(mem_stream);
    UT_ASSERT_EQUAL(-EINVAL, stream_seek(narrow_stream, +6, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(saved_ofs, stream_tell(mem_stream));
}

UT_SECTION(narrowed_stream_read)

ut_setup()
{
    __common_setup();
}

ut_cleanup()
{
    __common_cleanup();
}

ut_test(reading_passed_end_returns_zero_bytes)
{
    char c;
    size_t i, n;

    ut_printf("narrow stream offset: %"PRIu64, stream_tell(narrow_stream));
    ut_printf("mem stream offset: %"PRIu64, stream_tell(mem_stream));

    n = NARROW_STREAM_END - NARROW_STREAM_START + 1;
    for (i = 0; i < n; i++)
    {
        UT_ASSERT_EQUAL(1, stream_read(narrow_stream, &c, 1));
        UT_ASSERT_EQUAL('n', c);
    }

    UT_ASSERT_EQUAL(0, stream_read(narrow_stream, &c, 1));
}

ut_test(reading_beyond_end_returns_only_whats_available)
{
    char buf[5];
    int i;

    UT_ASSERT_EQUAL(0, stream_seek(narrow_stream, -2, STREAM_SEEK_FROM_END));
    ut_printf("narrow stream ofs after seek: %"PRIu64, stream_tell(narrow_stream));
    ut_printf("mem stream ofs after seek: %"PRIu64, stream_tell(mem_stream));

    UT_ASSERT_EQUAL(3, stream_read(narrow_stream, buf, sizeof(buf)));
    for (i = 0; i < 3; i++)
        UT_ASSERT_EQUAL('n', buf[i]);
}

UT_SECTION(narrowed_stream_write)

ut_setup()
{
    __common_setup();
}

ut_cleanup()
{
    __common_cleanup();
}

ut_test(writing_after_end_returns_EIO)
{
    UT_ASSERT_EQUAL(0, stream_seek(mem_stream, sizeof(__buf), STREAM_SEEK_FROM_BEGINNING));
}

ut_test(writing_beyond_end_of_valid_range_returns_ENOSPC)
{
    char c;

    UT_ASSERT_EQUAL(0, stream_seek(narrow_stream, 0, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(1, stream_write(narrow_stream, &c, 1));

    UT_ASSERT_EQUAL(-ENOSPC, stream_write(narrow_stream, &c, 1));

    UT_ASSERT_EQUAL(0, stream_seek(narrow_stream, -3, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(-ENOSPC, stream_write(narrow_stream, "longer_than_3_bytes",
                                          strlen("longer_than_3_bytes")));
}

ut_test(writing_whole_narrowed_stream_only_writes_defined_range)
{
    char c = '!';
    int w;
    int i;

    do
        w = stream_write(narrow_stream, &c, 1);
    while (w == 1);

    UT_ASSERT_EQUAL(-ENOSPC, w);

    __dump_buf();

    for (i = 0; i < sizeof(__buf); i++)
        if (i >= NARROW_STREAM_START && i <= NARROW_STREAM_END)
            UT_ASSERT_EQUAL(c, __buf[i]);
        else
            UT_ASSERT_EQUAL('b', __buf[i]);
}

UT_SECTION(FIXME)

ut_setup()
{
    __common_setup();
}

ut_cleanup()
{
    __common_cleanup();
}

ut_test(non_narrowed_stream_acts_like_base_stream)
{
    stream_t *narrow_stream;
    uint64_t start, end;
    char *word;
    size_t n;

    start = stream_tell(mem_stream);
    stream_seek(mem_stream, 0, STREAM_SEEK_FROM_END);
    end = stream_tell(mem_stream);

    UT_ASSERT_EQUAL(0, narrowed_stream_open(&narrow_stream, mem_stream, start, end,
                                            stream_access(mem_stream)));

    word = "end";
    n = strlen(word);

    UT_ASSERT_EQUAL(0, stream_seek(narrow_stream, -n, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(n, stream_write(narrow_stream, word, n));

    stream_close(narrow_stream);
}
