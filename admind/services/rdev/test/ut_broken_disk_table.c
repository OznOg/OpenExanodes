/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "admind/services/rdev/include/broken_disk_table.h"
#include "common/include/exa_constants.h"
#include "os/include/os_file.h"
#include "os/include/os_random.h"

#ifndef WIN32
#include <sys/stat.h>
#endif

#include <errno.h>

/* XXX Move test cases for uuid_{to,from}_str() from ut_service_rdev to here */

static bool __file_exists(const char *filename)
{
#ifdef WIN32
    DWORD attr = GetFileAttributes(filename);
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(filename, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

UT_SECTION(opening_and_closing)

#define TABLE1 "__table1__"

ut_setup()
{
    os_random_init();
    unlink(TABLE1);
}

ut_cleanup()
{
    unlink(TABLE1);
    os_random_cleanup();
}

static void __get_random_uuids(exa_uuid_t *uuids)
{
    int i;

    for (i = 0; i < NBMAX_DISKS; i++)
        os_get_random_bytes(&uuids[i], sizeof(exa_uuid_t));
}

static void __save_uuids(const exa_uuid_t *uuids, const char *filename)
{
    broken_disk_table_t *table;
    int err;

    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, filename, true));

    UT_ASSERT_EQUAL(0, broken_disk_table_set(table, uuids));
    err = broken_disk_table_write(table);

    broken_disk_table_unload(&table);

    UT_ASSERT_EQUAL(0, err);
}

static void __load_uuids(exa_uuid_t *uuids, const char *filename)
{
    broken_disk_table_t *table;
    const exa_uuid_t *uuids_read;

    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, filename, true));

    uuids_read = broken_disk_table_get(table);
    if (uuids_read != NULL)
        memcpy(uuids, uuids_read, NBMAX_DISKS * sizeof(exa_uuid_t));

    broken_disk_table_unload(&table);

    UT_ASSERT(uuids_read != NULL);
}

ut_test(opening_null_table_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, broken_disk_table_load(NULL, TABLE1, true));
}

ut_test(opening_table_from_null_file_returns_EINVAL)
{
    broken_disk_table_t *table;
    UT_ASSERT_EQUAL(-EINVAL, broken_disk_table_load(&table, NULL, true));
}

ut_test(opening_non_existent_table_creates_it_empty_with_version_1)
{
    broken_disk_table_t *table;
    int i;

    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, TABLE1, true));
    UT_ASSERT(__file_exists(TABLE1));

    UT_ASSERT_EQUAL(1, broken_disk_table_get_version(table));

    for (i = 0; i < NBMAX_DISKS; i++)
    {
        const exa_uuid_t *uuid;

        uuid = broken_disk_table_get_disk(table, i);
        UT_ASSERT(uuid != NULL && uuid_is_zero(uuid));
    }

    broken_disk_table_unload(&table);
    UT_ASSERT(table == NULL);
}

ut_test(filling_table_then_writing_and_reading_it_back)
{
    static exa_uuid_t uuids[NBMAX_DISKS];
    static exa_uuid_t uuids2[NBMAX_DISKS];
    int i;

    __get_random_uuids(uuids);

    __save_uuids(uuids, TABLE1);
    __load_uuids(uuids2, TABLE1);

    for (i = 0; i < NBMAX_DISKS; i++)
        UT_ASSERT(uuid_is_equal(&uuids2[i], &uuids[i]));
}

UT_SECTION(broken_disk_table_contains)

ut_setup()
{
    os_random_init();
    unlink(TABLE1);
}

ut_cleanup()
{
    unlink(TABLE1);
    os_random_cleanup();
}

ut_test(filling_table_then_checking_containership)
{
    static exa_uuid_t uuids[NBMAX_DISKS];
    broken_disk_table_t *table;
    int i;

    __get_random_uuids(uuids);

    __save_uuids(uuids, TABLE1);

    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, TABLE1, true));

    for (i = 0; i < NBMAX_DISKS; i++)
        UT_ASSERT(broken_disk_table_contains(table, &uuids[i]));

    broken_disk_table_unload(&table);
}

UT_SECTION(broken_disk_table_clear)

ut_setup()
{
    os_random_init();
    unlink(TABLE1);
}

ut_cleanup()
{
    unlink(TABLE1);
    os_random_cleanup();
}

ut_test(clearing_table_without_saving_leaves_on_disk_version_unchanged)
{
    static exa_uuid_t uuids[NBMAX_DISKS];
    broken_disk_table_t *table;
    int i;

    __get_random_uuids(uuids);

    __save_uuids(uuids, TABLE1);

    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, TABLE1, true));

    broken_disk_table_clear(table);

    /* Check the in-memory version has been wiped out */
    for (i = 0; i < NBMAX_DISKS; i++)
        UT_ASSERT(!broken_disk_table_contains(table, &uuids[i]));

    broken_disk_table_unload(&table);

    /* Check the on-disk version is unchanged */
    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, TABLE1, true));

    for (i = 0; i < NBMAX_DISKS; i++)
        UT_ASSERT(broken_disk_table_contains(table, &uuids[i]));

    broken_disk_table_unload(&table);
}

UT_SECTION(version)

ut_setup()
{
    os_random_init();
    unlink(TABLE1);
}

ut_cleanup()
{
    unlink(TABLE1);
    os_random_cleanup();
}

ut_test(increment_version_increments_it)
{
    broken_disk_table_t *table;

    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, TABLE1, true));
    UT_ASSERT(__file_exists(TABLE1));

    UT_ASSERT_EQUAL(1, broken_disk_table_get_version(table));
    broken_disk_table_increment_version(table);
    UT_ASSERT_EQUAL(2, broken_disk_table_get_version(table));

    broken_disk_table_unload(&table);
}

ut_test(increment_version_after_max_wraps_to_1)
{
    broken_disk_table_t *table;

    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, TABLE1, true));
    UT_ASSERT(__file_exists(TABLE1));

    UT_ASSERT_EQUAL(1, broken_disk_table_get_version(table));

    broken_disk_table_set_version(table, UINT64_MAX - 1);
    UT_ASSERT_EQUAL(UINT64_MAX - 1, broken_disk_table_get_version(table));

    broken_disk_table_increment_version(table);
    UT_ASSERT_EQUAL(UINT64_MAX, broken_disk_table_get_version(table));

    broken_disk_table_increment_version(table);
    UT_ASSERT_EQUAL(1, broken_disk_table_get_version(table));

    broken_disk_table_unload(&table);
}

UT_SECTION(load_nonexistent_table_readonly)

ut_setup()
{
    os_random_init();
    unlink(TABLE1);
}

ut_cleanup()
{
    unlink(TABLE1);
    os_random_cleanup();
}

ut_test(loading_fails)
{
    broken_disk_table_t *table;

    UT_ASSERT_EQUAL(-ENOENT, broken_disk_table_load(&table, TABLE1, false));
    UT_ASSERT(!__file_exists(TABLE1));
}

UT_SECTION(load_table_readonly)

ut_setup()
{
    static exa_uuid_t uuids[NBMAX_DISKS];
    os_random_init();
    unlink(TABLE1);

    __get_random_uuids(uuids);
    __save_uuids(uuids, TABLE1);
}

ut_cleanup()
{
    unlink(TABLE1);
    os_random_cleanup();
}

ut_test(touching_disks_fails)
{
    broken_disk_table_t *table;

    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, TABLE1, false));
    UT_ASSERT_EQUAL(1, broken_disk_table_get_version(table));

    UT_ASSERT_EQUAL(-EPERM, broken_disk_table_set_disk(table, 0, &exa_uuid_zero));
    UT_ASSERT_EQUAL(-EPERM, broken_disk_table_clear(table));

    broken_disk_table_unload(&table);
}

ut_test(touching_version_fails)
{
    broken_disk_table_t *table;

    UT_ASSERT_EQUAL(0, broken_disk_table_load(&table, TABLE1, false));

    UT_ASSERT_EQUAL(-EPERM, broken_disk_table_increment_version(table));
    UT_ASSERT_EQUAL(-EPERM, broken_disk_table_set_version(table, 10));

    broken_disk_table_unload(&table);
}
