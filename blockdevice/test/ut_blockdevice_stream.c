/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "blockdevice/include/blockdevice_stream.h"
#include "blockdevice/include/sys_blockdevice.h"

#include "os/include/os_disk.h"

static blockdevice_t *bdev;
static stream_t *stream;

#define __CACHE_SIZE  (3 * 512)

static const char *find_disk(void)
{
#ifdef WIN32
    static char disk[OS_PATH_MAX];
    char drive[3] = "?:";

    /* Find first non-existent disk, starting from M:, which we have on lab
       nodes, and thus far from the usual suspects (A: to D:) which we want
       to avoid */
    for (drive[0] = 'M'; drive[0] <= 'Z'; drive[0]++)
    {
        int fd;

        os_disk_normalize_path(drive, disk, sizeof(disk));
        fd = os_disk_open_raw(disk, OS_DISK_READ);
        if (fd >= 0)
        {
            close(fd);
            return disk;
        }
    }

    return NULL;
#else
    return "/dev/sda1";
#endif
}

static void common_setup(void)
{
    const char *disk = find_disk();

    UT_ASSERT_EQUAL(0, sys_blockdevice_open(&bdev, disk, BLOCKDEVICE_ACCESS_READ));
    UT_ASSERT_EQUAL(0, blockdevice_stream_on(&stream, bdev, __CACHE_SIZE, STREAM_ACCESS_READ));
}

static void common_cleanup(void)
{
    stream_close(stream);
    blockdevice_close(bdev);
}

UT_SECTION(opening)

ut_test(opening_stream_on_null_blockdevice_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, blockdevice_stream_on(&stream, NULL, __CACHE_SIZE, STREAM_ACCESS_READ));
}

ut_test(opening_stream_on_blockdevice_with_different_access_mode_returns_EPERM)
{
    const char *disk = find_disk();

    UT_ASSERT_EQUAL(0, sys_blockdevice_open(&bdev, disk, BLOCKDEVICE_ACCESS_READ));
    UT_ASSERT_EQUAL(-EPERM, blockdevice_stream_on(&stream, bdev, __CACHE_SIZE, STREAM_ACCESS_RW));
    blockdevice_close(bdev);
}

ut_test(opening_stream_on_valid_blockdevice_succeeds)
{
    const char *disk = find_disk();

    UT_ASSERT_EQUAL(0, sys_blockdevice_open(&bdev, disk, BLOCKDEVICE_ACCESS_RW));
    UT_ASSERT_EQUAL(0, blockdevice_stream_on(&stream, bdev, __CACHE_SIZE, STREAM_ACCESS_READ));
    stream_close(stream);
    blockdevice_close(bdev);
}

UT_SECTION(reading)

ut_setup()
{
    common_setup();
}

ut_cleanup()
{
    common_cleanup();
}

ut_test(reading_one_byte_at_a_time_succeeds)
{
    char c;
    int i;

    for (i = 0; i < 5793; i++)
        UT_ASSERT_EQUAL(1, stream_read(stream, &c, 1));
}

ut_test(overlapping_read_of_blocks_succeeds)
{
    char buf1[__CACHE_SIZE + 2];
    char buf2[__CACHE_SIZE + 2];

    UT_ASSERT_EQUAL(0, stream_seek(stream, __CACHE_SIZE - 25, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(sizeof(buf1), stream_read(stream, buf1, sizeof(buf1)));
    UT_ASSERT_EQUAL(0, stream_seek(stream, __CACHE_SIZE - 25, STREAM_SEEK_FROM_BEGINNING));
    UT_ASSERT_EQUAL(sizeof(buf2), stream_read(stream, buf2, sizeof(buf2)));

    UT_ASSERT_EQUAL(0, memcmp(buf1, buf2, sizeof(buf2)));
}
