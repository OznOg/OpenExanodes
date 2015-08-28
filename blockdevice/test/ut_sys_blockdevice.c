/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "blockdevice/include/sys_blockdevice.h"

#include "common/include/exa_constants.h"

#include "os/include/os_disk.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"
#include "os/include/os_system.h"

static blockdevice_t *bdev;

#ifdef WIN32
#define TEST_PATH  "\\\\?\\M:"
#else
#define TEST_PATH  "/dev/sda1"
#endif

static void common_setup(blockdevice_access_t access)
{
    UT_ASSERT_EQUAL(0, sys_blockdevice_open(&bdev, TEST_PATH, access));
}

static void common_cleanup(void)
{
    blockdevice_close(bdev);
}

UT_SECTION(sys_blockdevice_open)

#ifdef WIN32
static const char *find_nonexistent_disk(void)
{
    static char disk[OS_PATH_MAX];
    char drive[3] = "?:";

    /* Find first non-existent disk, skipping A: and B: (floppies)
       and C: (system) */
    for (drive[0] = 'D'; drive[0] <= 'Z'; drive[0]++)
    {
        os_disk_normalize_path(drive, disk, sizeof(disk));
        if (os_disk_open_raw(disk, OS_DISK_READ) == -ENOENT)
            return disk;
    }

    return NULL;
}
#endif

ut_test(opening_non_existent_blockdevice_returns_ENOENT)
{
    int err;

#ifdef WIN32
    const char *nonexistent_disk = find_nonexistent_disk();
    ut_printf("Testing on non-existent_disk: '%s'", nonexistent_disk);
    err = sys_blockdevice_open(&bdev, nonexistent_disk, BLOCKDEVICE_ACCESS_READ);
#else
    err = sys_blockdevice_open(&bdev, "/dev/azertyuiop", BLOCKDEVICE_ACCESS_READ);
#endif
    UT_ASSERT_EQUAL(-ENOENT, err);
}

ut_test(opening_file_not_blockdevice_returns_ENODEV)
{
#define DUMMY_FILE  "dummy"
    char *const touch_cmd[] = {"touch", DUMMY_FILE, NULL };

    UT_ASSERT_EQUAL(0, os_system(touch_cmd));
    UT_ASSERT_EQUAL(-ENODEV, sys_blockdevice_open(&bdev, DUMMY_FILE,
                                                  BLOCKDEVICE_ACCESS_READ));
    unlink(DUMMY_FILE);
}

ut_test(opening_blockdevice_succeeds)
{
    int err;

    ut_printf("(must be root to succeed)");
    err = sys_blockdevice_open(&bdev, TEST_PATH, BLOCKDEVICE_ACCESS_READ);
    UT_ASSERT_EQUAL(0, err);
}

UT_SECTION(blockdevice_get_sector_count_and_size)

ut_setup()
{
    common_setup(BLOCKDEVICE_ACCESS_READ);
}

ut_cleanup()
{
    common_cleanup();
}

static uint64_t __disk_size(const char *path)
{
    int fd;
    uint64_t size;

    fd = os_disk_open_raw(path, OS_DISK_READ);
    UT_ASSERT(fd >= 0);
    UT_ASSERT_EQUAL(0, os_disk_get_size(fd, &size));
    close(fd);

    return size;
}

ut_test(device_size_is_correct)
{
    uint64_t expected_size = __disk_size(TEST_PATH);
    uint64_t size = blockdevice_size(bdev);

    ut_printf("size: %"PRIu64" bytes", size);
    UT_ASSERT_EQUAL(expected_size, size);
}

UT_SECTION(blockdevice_read)

ut_setup()
{
    common_setup(BLOCKDEVICE_ACCESS_READ);
}

ut_cleanup()
{
    common_cleanup();
}

ut_test(reading_with_unaligned_buffer_succeeds)
{
    size_t buf_size = 3456;
    char buf1[buf_size], buf2[buf_size];

    UT_ASSERT_EQUAL(0, blockdevice_read(bdev, buf1, buf_size, 0));
    UT_ASSERT_EQUAL(0, blockdevice_read(bdev, buf2, buf_size, 0));
    UT_ASSERT_EQUAL(0, memcmp(buf1, buf2, buf_size));
}

ut_test(reading_at_specified_block_succeeds)
{
    size_t buf_size = 2 * SECTOR_SIZE;
    char buf1[buf_size], buf2[buf_size];
    size_t sector = 3;

    /* Read big buffer in one step */
    UT_ASSERT_EQUAL(0, blockdevice_read(bdev, buf1, buf_size, sector));

    /* Read small buffer in two step */
    UT_ASSERT_EQUAL(0, blockdevice_read(bdev, buf2, buf_size / 2, sector));
    UT_ASSERT_EQUAL(0, blockdevice_read(bdev, buf2 + buf_size / 2, buf_size / 2, sector + 1));

    UT_ASSERT_EQUAL(0, memcmp(buf1, buf2, buf_size));

    {
        char ref_buf[buf_size];
        int fd = os_disk_open_raw(TEST_PATH, OS_DISK_READ);

        UT_ASSERT(fd >= 0);
        UT_ASSERT_EQUAL(sector * SECTOR_SIZE, lseek(fd, sector * SECTOR_SIZE, SEEK_SET));
        UT_ASSERT_EQUAL(buf_size, read(fd, ref_buf, buf_size));
        close(fd);

        UT_ASSERT_EQUAL(0, memcmp(buf1, ref_buf, buf_size));
    }
}

UT_SECTION(blockdevice_write)

ut_setup()
{
    common_setup(BLOCKDEVICE_ACCESS_RW);
}

ut_cleanup()
{
    common_cleanup();
}

ut_test(writing_succeeds)
{
    ut_printf("(too dangerous, doing nothing!)");
}
