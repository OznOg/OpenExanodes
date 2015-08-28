/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/common/include/file_stream.h"

#include "os/include/os_file.h"
#include "os/include/os_disk.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_string.h"
#include "os/include/os_user.h"
#include "os/include/os_stdio.h"

#define BYTE_FILE       "__bytes__"
#define BYTE_FILE_COPY  BYTE_FILE "copy__"

#define FILE_SIZE  235

ut_setup()
{
    FILE *f;
    size_t i;

    unlink(BYTE_FILE);

    f = fopen(BYTE_FILE, "wb");
    UT_ASSERT(f != NULL);

    for (i = 0; i < FILE_SIZE; i++)
        fputc('a' + (i % ('z' - 'a' + 1)), f);

    fclose(f);
}

ut_cleanup()
{
    unlink(BYTE_FILE);
    unlink(BYTE_FILE_COPY);
}

ut_test(opening_stream_on_non_existent_file_returns_ENOENT)
{
    stream_t *stream;

    /* Ensure the file does not exist */
    unlink("hahaha");

    UT_ASSERT_EQUAL(-ENOENT, file_stream_open(&stream, "hahaha", STREAM_ACCESS_READ));
}

ut_test(read_file_through_stream)
{
    FILE *f1;
    stream_t *stream;
    char buf1[10], buf2[10];

    f1 = fopen(BYTE_FILE, "rb");
    UT_ASSERT(f1 != NULL);

    UT_ASSERT_EQUAL(0, file_stream_open(&stream, BYTE_FILE, STREAM_ACCESS_READ));
    UT_ASSERT(stream != NULL);

    while (true)
    {
        int r1 = fread(buf1, 1, sizeof(buf1), f1);
        int r2 = stream_read(stream, buf2, sizeof(buf2));

        UT_ASSERT_EQUAL(r1, r2);
        if (r1 == 0)
            break;

        UT_ASSERT_EQUAL(0, memcmp(buf1, buf2, r1));
    }

    stream_close(stream);

    fclose(f1);
}

ut_test(copy_and_compare_file_through_streams)
{
    stream_t *stream1, *stream2;
    char buf[16];
    int r;

    UT_ASSERT_EQUAL(0, file_stream_open(&stream1, BYTE_FILE, STREAM_ACCESS_READ));
    UT_ASSERT(stream1 != NULL);

    UT_ASSERT_EQUAL(0, file_stream_open(&stream2, BYTE_FILE, STREAM_ACCESS_RW));
    UT_ASSERT(stream2 != NULL);

    /*
     * Copy the file
     */
    while ((r = stream_read(stream1, buf, sizeof(buf))) != 0)
    {
        int w = stream_write(stream2, buf, r);
        UT_ASSERT(w == r);
    }

    /*
     * Compare copy with original
     */
    UT_ASSERT_EQUAL(0, stream_rewind(stream1));
    UT_ASSERT_EQUAL(0, stream_rewind(stream2));

    while (true)
    {
        char buf1[9];
        char buf2[9];

        int r1 = stream_read(stream1, buf1, sizeof(buf1));
        int r2 = stream_read(stream2, buf2, sizeof(buf2));

        UT_ASSERT_EQUAL(r1, r2);

        if (r1 == 0)
            break;

        UT_ASSERT_EQUAL(0, memcmp(buf1, buf2, r1));
    }

    stream_close(stream1);
    stream_close(stream2);
}

ut_test(seeking_and_reading_file)
{
    FILE *f;
    stream_t *stream;
#define BUF_SIZE  3
    char buf[BUF_SIZE];
    int i, r;

    typedef struct
    {
        stream_seek_t seek;
        int64_t ofs;
        uint64_t expected_ofs;
        char expected_bytes[BUF_SIZE];
    } blurb_t;
    blurb_t blurbs[4] =
    {
        { .seek = STREAM_SEEK_FROM_BEGINNING, .ofs =  5, .expected_bytes = "fgh" },
        { .seek = STREAM_SEEK_FROM_POS,       .ofs =  2, .expected_bytes = "klm" },
        { .seek = STREAM_SEEK_FROM_POS,       .ofs = -7, .expected_bytes = "ghi" },
        { .seek = STREAM_SEEK_FROM_END,       .ofs = -3, .expected_bytes = "yza" }
    };
#define NUM_BLURBS  (sizeof(blurbs) / sizeof(blurb_t))

    f = fopen(BYTE_FILE, "rb");
    UT_ASSERT(f != NULL);

    /* First, get reference data by seeking and reading directly from the
       file we'll stream. */

    for (i = 0; i < NUM_BLURBS; i++)
    {
        blurb_t *b = &blurbs[i];
        int s = SEEK_SET; /** init prevent gcc to complain */

        switch (b->seek)
        {
        case STREAM_SEEK_FROM_BEGINNING:
            s = SEEK_SET;
            break;
        case STREAM_SEEK_FROM_END:
            s = SEEK_END;
            break;
        case STREAM_SEEK_FROM_POS:
            s = SEEK_CUR;
            break;
        }

        UT_ASSERT_EQUAL(0, fseek(f, b->ofs, s));
        b->expected_ofs = ftell(f);
        UT_ASSERT_EQUAL(sizeof(buf), fread(buf, 1, sizeof(buf), f));
        memcpy(b->expected_bytes, buf, sizeof(b->expected_bytes));
    }

    fclose(f);

    /* Now, stream the file and check the seeking and reading yields the
       same data as with direct access to the file. */

    UT_ASSERT_EQUAL(0, file_stream_open(&stream, BYTE_FILE, STREAM_ACCESS_READ));
    UT_ASSERT(stream != NULL);

    for (i = 0; i < NUM_BLURBS; i++)
    {
        blurb_t *b = &blurbs[i];

        UT_ASSERT_EQUAL(0, stream_seek(stream, b->ofs, b->seek));
        UT_ASSERT_EQUAL(b->expected_ofs, stream_tell(stream));

        r = stream_read(stream, buf, sizeof(buf));
        UT_ASSERT_EQUAL(sizeof(buf), r);
        UT_ASSERT_EQUAL(0, memcmp(buf, b->expected_bytes, sizeof(buf)));
    }

    stream_close(stream);
}

/* Must be root to call this function */
static const char *find_swap_partition(void)
{
    static char swap[128];
    FILE *f;
    char part[128];

    /* XXX This obviously doesn't work on Windows */
    f = popen("ls -1 /dev/sda*", "r");

    while (fgets(part, sizeof(part), f) != NULL)
    {
        size_t len = strlen(part);
        char cmd[256];

        while (len > 0 && isspace(part[len - 1]))
            len--;
        part[len] = '\0';

        os_snprintf(cmd, sizeof(cmd), "file -s %s | grep 'swap file' >/dev/null 2>&1", part);
        if (system(cmd) == 0)
        {
            os_strlcpy(swap, part, sizeof(swap));
            return swap;
        }
    }

    pclose(f);

    return NULL;
}

/* CAUTION - This test case is DANGEROUS as it writes to a blockdevice.
   To alleviate the risk a bit, it only runs if it can find a swap
   partition, and only attempts to write at the very end of said partition. */
ut_test(writing_beyond_end_of_block_device_returns_ENOSPC)
{
    const char *disk;
    stream_t *stream;
    char c;

    if (!os_user_is_admin())
    {
        ut_printf("CAN ONLY BE RUN AS ROOT, SKIPPING");
        return;
    }

    disk = find_swap_partition();
    if (disk == NULL)
    {
        ut_printf("SWAP PARTITION NOT FOUND, SKIPPING");
        return;
    }
    ut_printf("Testing on swap partition: '%s'", disk);

    UT_ASSERT_EQUAL(0, file_stream_open(&stream, disk, STREAM_ACCESS_RW));
    UT_ASSERT(stream != NULL);

    UT_ASSERT_EQUAL(0, stream_seek(stream, 0, STREAM_SEEK_FROM_END));
    UT_ASSERT_EQUAL(-ENOSPC, stream_write(stream, &c, 1));

    stream_close(stream);
}
