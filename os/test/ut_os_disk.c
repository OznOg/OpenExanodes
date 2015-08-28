/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_disk.h"
#include "os/include/os_file.h"
#include "os/include/os_stdio.h"
#include "os/include/os_error.h"

#include <unit_testing.h>

#ifndef WIN32
#include <sys/stat.h>
#endif

#ifdef WIN32

#define EXISTING_DISK_PATH        "\\\\?\\C:"
/* XXX We're not sure these paths don't exist... but for now it will do. */
#define NON_EXISTENT_DISK_PATH    "\\\\?\\V:"
#define NON_EXISTENT_DISK_PATH_2  "\\\\?\\Volume{ffffffff-ffff-ffff-ffff-ffffffffffff}"

#define STAR_PATTERN      "\\\\?\\Volume*"
#define NO_MATCH_PATTERN  "\\\\?\\VolumeToto*"

static const char *non_block_device_patterns[3] =
{
    /* XXX A CD-ROM is not considered a block device by os_disk on Windows
     * because it doesn't actually check that the drive is a block device:
     * instead, it considers as block devices the *non-removable* drives. */
    "\\Device\\CdRom*",
    "\\Device\\Floppy*",
    NULL
};

#else

#define EXISTING_DISK_PATH      "/dev/sda1"
#define NON_EXISTENT_DISK_PATH  "/dev/sdToto"

#define STAR_PATTERN      "/dev/sd*"
#define NO_STAR_PATTERN   "/dev/sda"
#define NO_MATCH_PATTERN  "toto*"

static const char *non_block_device_patterns[3] =
{
    "/dev/tty*",
    "/dev/pts/*",
    NULL
};

#endif

UT_SECTION(disk_iterator)

ut_test(iterator_with_good_pattern_yields_one_or_more_disks)
{
    os_disk_iterator_t *iter;
    const char *disk;
    unsigned count = 0;

    iter = os_disk_iterator_begin(STAR_PATTERN);
    UT_ASSERT(iter != NULL);

    while ((disk = os_disk_iterator_get(iter)) != NULL)
        count++;
    ut_printf("found %u disks", count);
    UT_ASSERT(count > 0);

    os_disk_iterator_end(iter);
}

#ifndef WIN32
static void list_all_block_devs_in_path(const char *path, char *block_dev_list[])
{
	char cmd[512]; /* long enough */
        FILE *file;
        size_t len = 0;
        ssize_t read;
        int pos = 0;

        UT_ASSERT(snprintf(cmd, sizeof(cmd), "ls -1d %s", path) < sizeof(cmd));

        file = popen(cmd, "r");
        UT_ASSERT(file != NULL);

        /* get all bdevs */
        do {
            char *line = NULL;

            read = getline(&line, &len, file);

            if (line != NULL && read >= 0)
            {
                struct stat st;
                int st_result;

                /* remove trailing '\n' */
                if (line[strlen(line) - 1] == '\n')
                    line[strlen(line) - 1] = '\0';

                /* get bdevs, release others */
                st_result = stat(line, &st);
                if (st_result == 0 && S_ISBLK(st.st_mode))
                {
                    block_dev_list[pos++] = line;
                    continue;
                }

                /* ENOENT may happen in case the /dev entry is a link to
                   a non-existent device */
                UT_ASSERT_VERBOSE(st_result == 0 || errno == ENOENT,
                                  "stat error %d = %s", errno, strerror(errno));

                free(line);
            }
        } while (read >= 0);

        pclose(file);
    }

/* for next test, see bug #4529 */
ut_test(iterator_with_star_find_all_block_devices_and_only_them)
{
    const char dev_star[] = "/dev/*";
    int i;
    os_disk_iterator_t *iter;
    const char *disk;
    unsigned count = 0;
    #define BDEV_ARRAY_SIZE 1024
    char *block_dev_list[BDEV_ARRAY_SIZE] = { 0 };

    list_all_block_devs_in_path(dev_star, block_dev_list);

    iter = os_disk_iterator_begin(dev_star);
    UT_ASSERT(iter != NULL);

    while ((disk = os_disk_iterator_get(iter)) != NULL)
    {
        for (i = 0; i < BDEV_ARRAY_SIZE; i++)
        {
            /* ut_printf("testing  '%s' against '%s'", disk, block_dev_list[i]); */
            if (block_dev_list[i] != NULL
                && strcmp(block_dev_list[i], disk) == 0)
            {
                free(block_dev_list[i]);
                block_dev_list[i] = NULL;
                break;
            }
        }

        UT_ASSERT_VERBOSE(i < BDEV_ARRAY_SIZE, "Cannot find enty '%s' in block_dev_list", disk);
        count++;
    }

    os_disk_iterator_end(iter);

    /* Check all bdevs were found */
    for (i = 0; i < BDEV_ARRAY_SIZE; i++)
        UT_ASSERT_VERBOSE(block_dev_list[i] == NULL,
                "Entry '%s' was not found by os_disk_iterator_get",
                block_dev_list[i]);

    ut_printf("found %u disks", count);
    UT_ASSERT(count > 0);

}
#endif

ut_test(iterator_with_no_star_pattern_yields_a_single_disk)
{
#ifdef WIN32
    /* To test a no-star pattern on Windows, one needs the uid U of an
     * existing volume and build the pattern \\?\Volume{U} from it. We could
     * get the first disk returned by a disk iterator, except that it will
     * probably return a drive spec 'X:' instead of a volume spec... */
    ut_printf("(FIXME No way to know an existing volume uid on Windows!)");
#else
    os_disk_iterator_t *iter;
    const char *disk;
    unsigned count = 0;

    iter = os_disk_iterator_begin(NO_STAR_PATTERN);
    UT_ASSERT(iter != NULL);

    while ((disk = os_disk_iterator_get(iter)) != NULL)
        count++;
    ut_printf("found %u disks", count);
    UT_ASSERT(count == 1);

    os_disk_iterator_end(iter);
#endif
}

ut_test(iterator_with_nomatch_pattern_yields_nothing)
{
    os_disk_iterator_t *iter;

    iter = os_disk_iterator_begin(NO_MATCH_PATTERN);
    UT_ASSERT(iter != NULL);
    UT_ASSERT(os_disk_iterator_get(iter) == NULL);

    os_disk_iterator_end(iter);
}

ut_test(iterator_passed_last_entry_yields_nothing)
{
    os_disk_iterator_t *iter;
    const char *disk;
    unsigned count = 0;

    iter = os_disk_iterator_begin(STAR_PATTERN);
    UT_ASSERT(iter != NULL);

    while ((disk = os_disk_iterator_get(iter)) != NULL)
        count++;
    UT_ASSERT(os_disk_iterator_get(iter) == NULL);

    os_disk_iterator_end(iter);
}

ut_test(iterator_on_non_block_devices_yields_nothing)
{
    os_disk_iterator_t *iter;
    int i;

    for (i = 0; non_block_device_patterns[i] != NULL; i++)
    {
        iter = os_disk_iterator_begin(non_block_device_patterns[i]);
        UT_ASSERT(iter != NULL);
        UT_ASSERT(os_disk_iterator_get(iter) == NULL);
        os_disk_iterator_end(iter);
    }
}

ut_test(iterator_with_non_numeric_suffix_fails_on_windows)
{
#ifdef WIN32
    os_disk_iterator_t *iter;

    iter = os_disk_iterator_begin(STAR_PATTERN "arbitrarySuffix");
    UT_ASSERT(iter != NULL);
    UT_ASSERT(os_disk_iterator_get(iter) == NULL);

    os_disk_iterator_end(iter);
#else
    ut_printf("This test case is irrelevant on Linux.");
    return;
#endif
}

UT_SECTION(os_disk_open_raw)

ut_test(open_raw_null_disk_returns_EINVAL)
{
    UT_ASSERT(os_disk_open_raw(NULL, 0) == -EINVAL);
}

ut_test(open_raw_0_flag_returns_EINVAL)
{
    UT_ASSERT(os_disk_open_raw(EXISTING_DISK_PATH, 0) == -EINVAL);
}

ut_test(open_raw_existing_disk_succeeds)
{
    int fd;

    ut_printf("(must be root to succeed)");

    fd = os_disk_open_raw(EXISTING_DISK_PATH, OS_DISK_READ);
    UT_ASSERT(fd >= 0);
    close(fd);
}

ut_test(open_raw_non_existent_disk_returns_ENOENT)
{
    int fd;

    fd = os_disk_open_raw(NON_EXISTENT_DISK_PATH, OS_DISK_READ);
    UT_ASSERT(fd == -ENOENT);
}

UT_SECTION(os_disk_get_size)

ut_test(get_size_of_existing_disk_succeeds)
{
    uint64_t size;
    int fd;
    int ret;

    ut_printf("(must be root to succeed)");

    fd = os_disk_open_raw(EXISTING_DISK_PATH, OS_DISK_READ);
    UT_ASSERT(fd >= 0);

    ret = os_disk_get_size(fd, &size);

    close(fd);

    UT_ASSERT(ret == 0);
    /* XXX On Linux, check against /proc/partitions? (div 1024) */
    UT_ASSERT(size > 0);
}

ut_test(get_size_with_negative_disk_descriptor_returns_EINVAL)
{
    uint64_t size;
    UT_ASSERT(os_disk_get_size(-1, &size) == -EINVAL);
}

ut_test(get_size_with_null_size_param_returns_EINVAL)
{
    int fd;

    ut_printf("(must be root to succeed)");

    fd = os_disk_open_raw(EXISTING_DISK_PATH, OS_DISK_READ);
    UT_ASSERT(fd >= 0);

    UT_ASSERT(os_disk_get_size(fd, NULL) == -EINVAL);

    close(fd);
}

UT_SECTION(os_disk_normalize_path)

ut_test(normalize_of_NULL_in_path_returns_EINVAL)
{
    char out_path[OS_PATH_MAX];
    UT_ASSERT(os_disk_normalize_path(NULL, out_path, sizeof(out_path)) == -EINVAL);
}

ut_test(normalize_to_NULL_out_path_returns_EINVAL)
{
#ifdef WIN32
    const char *in_path = "E:";
#else
    const char *in_path = "/dev/sda";
#endif
    UT_ASSERT(os_disk_normalize_path(in_path, NULL, 10) == -EINVAL);
}

ut_test(normalize_with_zero_out_size_returns_EINVAL)
{
#ifdef WIN32
    const char *in_path = "E:";
#else
    const char *in_path = "/dev/sda";
#endif
    char out_path[OS_PATH_MAX];
    UT_ASSERT(os_disk_normalize_path(in_path, out_path, 0) == -EINVAL);
}

ut_test(normalize_too_long_a_path_returns_ENAMETOOLONG)
{
#ifdef WIN32
    const char *in_path = "E:";
    /* out_path long enough to hold in_path alone, but too short to hold
     * in_path *and* the volume namespace prefix */
    char out_path[strlen(in_path) + 1 + 3];

    UT_ASSERT(os_disk_normalize_path(in_path, out_path, sizeof(out_path))
              == -ENAMETOOLONG);
#else
    ut_printf("(Normalization doesn't change the path on Linux)");
#endif
}

ut_test(normalize_uppercase_drive_alone_succeeds)
{
#ifdef WIN32
    const char *in_path = "E:";
    char out_path[OS_PATH_MAX];

    UT_ASSERT(os_disk_normalize_path(in_path, out_path, sizeof(out_path)) == 0);
    UT_ASSERT(strcmp(out_path, "\\\\?\\E:") == 0);
#else
    ut_printf("(There are no drive letters on Linux)");
#endif
}

ut_test(normalize_lowercase_drive_alone_succeeds)
{
#ifdef WIN32
    const char *in_path = "e:";
    char out_path[OS_PATH_MAX];

    UT_ASSERT(os_disk_normalize_path(in_path, out_path, sizeof(out_path)) == 0);
    UT_ASSERT(strcmp(out_path, "\\\\?\\E:") == 0);
#else
    ut_printf("(There are no drive letters on Linux)");
#endif
}

ut_test(normalize_uppercase_drive_letter_with_namespace_prefix_and_colon_succeeds)
{
#ifdef WIN32
    const char *in_path = "\\\\?\\E:";
    char out_path[OS_PATH_MAX];

    UT_ASSERT(os_disk_normalize_path(in_path, out_path, sizeof(out_path)) == 0);
    UT_ASSERT(strcmp(out_path, in_path) == 0);
#else
    ut_printf("(There are no drive letters on Linux)");
#endif
}

ut_test(normalize_lowercase_drive_letter_with_namespace_prefix_and_colon_succeeds)
{
#ifdef WIN32
    const char *in_path = "\\\\?\\e:";
    char out_path[OS_PATH_MAX];

    UT_ASSERT(os_disk_normalize_path(in_path, out_path, sizeof(out_path)) == 0);
    UT_ASSERT(strcmp(out_path, "\\\\?\\E:") == 0);
#else
    ut_printf("(There are no drive letters on Linux)");
#endif
}

ut_test(normalize_volume_succeeds)
{
#ifdef WIN32
    const char *in_path = "\\\\?\\Volume{c00a39e7-e9ca-4a2c-b423-2071a4af7f2b}";
#else
    const char *in_path = "/dev/sda1";
#endif
    char out_path[OS_PATH_MAX];

    UT_ASSERT(os_disk_normalize_path(in_path, out_path, sizeof(out_path)) == 0);
    UT_ASSERT(strcmp(out_path, in_path) == 0);
}

UT_SECTION(os_disk_path_is_valid)

ut_test(null_path_is_not_valid)
{
    UT_ASSERT(!os_disk_path_is_valid(NULL));
}

ut_test(empty_path_is_not_valid)
{
    UT_ASSERT(!os_disk_path_is_valid(""));
}

ut_test(drive_letter_without_colon_is_not_valid)
{
    UT_ASSERT(!os_disk_path_is_valid("E"));
}

ut_test(drive_letter_with_colon_is_valid)
{
#ifdef WIN32
    UT_ASSERT(os_disk_path_is_valid("E:"));
#else
    ut_printf("(There are no drive letters on Linux)");
#endif
}

ut_test(drive_without_colon_with_namespace_prefix_is_not_valid)
{
#ifdef WIN32
    UT_ASSERT(!os_disk_path_is_valid("\\\\?\\E"));
#else
    ut_printf("(There are no drive letters nor namespaces prefixes on Linux)");
#endif
}

ut_test(drive_with_colon_with_namespace_prefix_is_valid)
{
#ifdef WIN32
    UT_ASSERT(os_disk_path_is_valid("\\\\?\\E:"));
#else
    ut_printf("(There are no drive letters nor namespaces prefixes on Linux)");
#endif
}

ut_test(non_dev_volume_is_not_valid)
{
#ifdef WIN32
    ut_printf("(No /dev on Windows)");
#else
    UT_ASSERT(!os_disk_path_is_valid("/usr/bin/test"));
#endif
}

ut_test(valid_volume_is_valid_indeed)
{
#ifdef WIN32
    /* This is an uid reported by Windows for some volume */
    UT_ASSERT(os_disk_path_is_valid("\\\\?\\Volume{c00a39e7-e9ca-4a2c-b423-2071a4af7f2b}"));
    /* This is an invalid *uid* but the *volume* is considered valid because
     * it checks for the form around the uid, not the uid. XXX Should we
     * check the uid? */
    UT_ASSERT(os_disk_path_is_valid("\\\\?\\Volume{zzz}"));
#else
    UT_ASSERT(os_disk_path_is_valid("/dev/sda4"));
    UT_ASSERT(os_disk_path_is_valid("/dev/hd5"));
#endif
}

UT_SECTION(os_disk_has_fs)

ut_test(disk_system)
{
    UT_ASSERT(os_disk_has_fs(EXISTING_DISK_PATH ) == true);
}

ut_test(non_existing_disk)
{
    UT_ASSERT(os_disk_has_fs(NON_EXISTENT_DISK_PATH) == false);
}
