/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_filemap.h"
#include "os/include/os_file.h"
#include "os/include/os_string.h"
#include <unit_testing.h>

UT_SECTION(create_and_destroy)

#ifndef WIN32
#define TEST_FILE_NAME "/tmp/teeeeeeeeeeeeeesssssst"
#else
#define TEST_FILE_NAME "c:\\teeeeeeeeeeeeeesssssst"
#endif

ut_setup()
{
    unlink(TEST_FILE_NAME);
}

ut_cleanup()
{
    unlink(TEST_FILE_NAME);
}

ut_test(create_and_destroy_valid_filemap)
{
    os_fmap_t *fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);

    UT_ASSERT(fmap != NULL);
    os_fmap_delete(fmap);
}

ut_test(create_with_invalid_filemap_fails)
{
    os_fmap_t *fmap = os_fmap_create("/gni/gnu/gno/gna/gny", 10 * 1024);

    UT_ASSERT(fmap == NULL);
}

ut_test(create_with_size_zero_fails)
{
    os_fmap_t *fmap = os_fmap_create(TEST_FILE_NAME, 0);

    UT_ASSERT(fmap == NULL);
}

ut_test(create_and_already_existant_filemap)
{
    os_fmap_t *fmap = os_fmap_create(TEST_FILE_NAME, 21);

    UT_ASSERT(fmap != NULL);

    os_fmap_close(fmap);

    fmap = os_fmap_create(TEST_FILE_NAME, 21);

    UT_ASSERT(fmap == NULL);

    fmap = os_fmap_open(TEST_FILE_NAME, 21, FMAP_RDWR);

    UT_ASSERT(fmap != NULL);

    os_fmap_delete(fmap);
}


UT_SECTION(create_open_close_destroy)

ut_test(create_close_open_destroy_valid_filemap)
{
    os_fmap_t *fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);

    UT_ASSERT(fmap != NULL);

    os_fmap_close(fmap);

    fmap = os_fmap_open(TEST_FILE_NAME, 100 * 1024 /* bad size */, FMAP_RDWR);
    UT_ASSERT(fmap == NULL);

    fmap = os_fmap_open(TEST_FILE_NAME, 10 * 1024, FMAP_RDWR);
    UT_ASSERT(fmap != NULL);

    os_fmap_delete(fmap);
}

UT_SECTION(read_write)

ut_test(read_write_some_data)
{
    char data[125];
    int count;

    os_fmap_t *fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);

    UT_ASSERT(fmap != NULL);

    memset(data, 0xAB, sizeof(data));

    count = os_fmap_write(fmap, 10, data, sizeof(data));
    UT_ASSERT_EQUAL(sizeof(data), count);

    memset(data, 0, sizeof(data));

    count = os_fmap_read(fmap, 10, data, sizeof(data));
    UT_ASSERT_EQUAL(sizeof(data), count);

    for (count -= 1; count >= 0; count--)
        UT_ASSERT_VERBOSE((data[count] & 0xFF) == 0xAB,
                          "Invalid read at %d: %X != %X", count, data[count], 0xAB);

    os_fmap_delete(fmap);
}

ut_test(NULL_pointers_return_EINVAL)
{
    char data[125];
    int count;

    os_fmap_t *fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);
    UT_ASSERT(fmap != NULL);

    count = os_fmap_write(fmap, 0, NULL, 100);
    UT_ASSERT_EQUAL(-EINVAL, count);

    count = os_fmap_read(fmap, 0, NULL, 100);
    UT_ASSERT_EQUAL(-EINVAL, count);

    os_fmap_delete(fmap);

    count = os_fmap_write(NULL, 0, data, 100);
    UT_ASSERT_EQUAL(-EINVAL, count);

    count = os_fmap_read(NULL, 0, data, 100);
    UT_ASSERT_EQUAL(-EINVAL, count);

}

ut_test(invalid_offsets)
{
    char data[125];
    int count;

    os_fmap_t *fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);

    UT_ASSERT(fmap != NULL);

    count = os_fmap_write(fmap, 100 * 1024, data, sizeof(data));
    UT_ASSERT_EQUAL(-EINVAL, count);

    count = os_fmap_read(fmap, 100 * 1024, data, sizeof(data));
    UT_ASSERT_EQUAL(-EINVAL, count);

    os_fmap_delete(fmap);
}

UT_SECTION(addr)

ut_test(address_of_null_fmap_is_null)
{
    UT_ASSERT(os_fmap_addr(NULL) == NULL);
}

ut_test(read_directly_what_was_written_using_api)
{
    const char hello[8] = "bonjour";
    const char *hello2;
    float number = 3.14159265;
    float number2;
    os_fmap_t *fmap;
    size_t ofs;
    int count;
    void *addr;

    fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);
    UT_ASSERT(fmap != NULL);

    ofs = 0;
    count = os_fmap_write(fmap, ofs, hello, sizeof(hello));
    UT_ASSERT_EQUAL(sizeof(hello), count);

    ofs += count;
    count = os_fmap_write(fmap, ofs, &number, sizeof(number));
    UT_ASSERT_EQUAL(sizeof(number), count);

    addr = os_fmap_addr(fmap);
    UT_ASSERT(addr != NULL);

    hello2 = addr;
    number2 = *(float *)((char *)addr + sizeof(hello));

    UT_ASSERT_EQUAL_STR(hello, hello2);
    UT_ASSERT_EQUAL(number, number2);

    os_fmap_delete(fmap);
}

ut_test(read_using_api_what_was_written_directly)
{
    const char hello[8] = "bonjour";
    char hello2[8];
    float number = 3.14159265;
    float number2;
    os_fmap_t *fmap;
    size_t ofs;
    int count;
    void *addr;

    fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);
    UT_ASSERT(fmap != NULL);

    addr = os_fmap_addr(fmap);
    UT_ASSERT(addr != NULL);

    memcpy(addr, hello, sizeof(hello));
    memcpy((char *)addr + sizeof(hello), &number, sizeof(number));

    UT_ASSERT_EQUAL(0, os_fmap_sync(fmap));

    ofs = 0;
    count = os_fmap_read(fmap, ofs, hello2, sizeof(hello2));
    UT_ASSERT_EQUAL(sizeof(hello2), count);

    ofs += count;
    count = os_fmap_read(fmap, ofs, &number2, sizeof(number2));
    UT_ASSERT_EQUAL(sizeof(number2), count);

    UT_ASSERT_EQUAL_STR(hello, hello2);
    UT_ASSERT_EQUAL(number, number2);

    os_fmap_delete(fmap);
}

UT_SECTION(size)

ut_test(size_of_null_fmap_is_negative)
{
    UT_ASSERT(os_fmap_size(NULL) == -1);
}

ut_test(size_of_fmap_is_correct)
{
    os_fmap_t *fmap;
    fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);
    UT_ASSERT(fmap != NULL);

    UT_ASSERT_EQUAL(10 * 1024, os_fmap_size(fmap));

    os_fmap_delete(fmap);
}

UT_SECTION(access)

ut_test(access_mode_of_fmap_opened_readonly_is_readonly)
{
    os_fmap_t *fmap;

    fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);
    UT_ASSERT(fmap != NULL);
    os_fmap_close(fmap);

    fmap = os_fmap_open(TEST_FILE_NAME, 10 * 1024, FMAP_READ);
    UT_ASSERT(fmap != NULL);
    UT_ASSERT_EQUAL(FMAP_READ, os_fmap_access(fmap));

    os_fmap_close(fmap);
    unlink(TEST_FILE_NAME);
}

ut_test(access_mode_of_map_opened_readwrite_is_readwrite)
{
    os_fmap_t *fmap;

    fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);
    UT_ASSERT(fmap != NULL);
    os_fmap_close(fmap);

    fmap = os_fmap_open(TEST_FILE_NAME, 10 * 1024, FMAP_RDWR);
    UT_ASSERT(fmap != NULL);
    UT_ASSERT_EQUAL(FMAP_RDWR, os_fmap_access(fmap));

    os_fmap_delete(fmap);
}

UT_SECTION(readonly)

ut_test(writing_to_readonly_fmap_fails)
{
    os_fmap_t *fmap;
    char data[125];

    fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);
    UT_ASSERT(fmap != NULL);
    os_fmap_close(fmap);

    fmap = os_fmap_open(TEST_FILE_NAME, 10 * 1024, FMAP_READ);
    UT_ASSERT(fmap != NULL);

    memset(data, 0xAB, sizeof(data));

    UT_ASSERT_EQUAL(-EPERM, os_fmap_write(fmap, 10, data, sizeof(data)));

    UT_ASSERT_EQUAL(-EPERM, os_fmap_delete(fmap));
    os_fmap_close(fmap);
    UT_ASSERT_EQUAL(0, unlink(TEST_FILE_NAME));
}

ut_test(syncing_readonly_fmap_fails)
{
    os_fmap_t *fmap;

    fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);
    UT_ASSERT(fmap != NULL);
    os_fmap_close(fmap);

    fmap = os_fmap_open(TEST_FILE_NAME, 10 * 1024, FMAP_READ);
    UT_ASSERT(fmap != NULL);

    UT_ASSERT_EQUAL(-EPERM, os_fmap_sync(fmap));

    UT_ASSERT_EQUAL(-EPERM, os_fmap_delete(fmap));
    os_fmap_close(fmap);
    UT_ASSERT_EQUAL(0, unlink(TEST_FILE_NAME));
}

#if 0
/* FIXME We expect this one to segfault. Uncomment once the
 * UT infrastructure handles that.
 */
ut_test(writing_to_readonly_fmap_addr_segfaults)
{
    os_fmap_t *fmap;
    char *addr;

    fmap = os_fmap_create(TEST_FILE_NAME, 10 * 1024);
    UT_ASSERT(fmap != NULL);
    os_fmap_close(fmap);

    fmap = os_fmap_open(TEST_FILE_NAME, 10 * 1024, FMAP_READ);
    UT_ASSERT(fmap != NULL);

    addr = os_fmap_addr(fmap);
    UT_ASSERT(addr != NULL);

    addr[0]='\0'; /* SIGSEGV */
    UT_ASSERT_EQUAL(-EPERM, os_fmap_sync(fmap));

    UT_ASSERT_EQUAL(-EPERM, os_fmap_delete(fmap));
    os_fmap_close(fmap);
    UT_ASSERT_EQUAL(0, unlink(TEST_FILE_NAME));
}
#endif
