/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "os/include/os_stdio.h"
#include "os/include/os_dir.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "admind/src/adm_file_ops.h"

#define TMP_DIR    "." OS_FILE_SEP "tmp"
#define TMP_FILE   "." OS_FILE_SEP "tmp" OS_FILE_SEP "tmpfile"
#define TEST_STR   "azertyuiopsdfghjklmwxcvbn" \
                   "azertyuiopsdfghjklmwxcvbn" \
                   "azertyuiopsdfghjklmwxcvbn" \
                   "azertyuiopsdfghjklmwxcvbn" \
                   "azertyuiopsdfghjklmwxcvbn" \
                   "azertyuiopsdfghjklmwxcvbn" \
                   "azertyuiopsdfghjklmwxcvbn" \
                   "azertyuiopsdfghjklmwxcvbn" \
                   "azertyuiopsdfghjklmwxcvbn"

ut_setup()
{
    FILE *fp = NULL;

    UT_ASSERT(os_dir_create_recursive(TMP_DIR) == 0);
    UT_ASSERT((fp = fopen(TMP_FILE, "w")) != NULL);
    UT_ASSERT(fwrite(TEST_STR, 1, sizeof(TEST_STR), fp) == sizeof(TEST_STR));
    UT_ASSERT(fclose(fp) == 0);
}

ut_cleanup()
{
    UT_ASSERT(os_dir_remove_tree(TMP_DIR) == 0);
}

ut_test(adm_file_read_to_str_correct)
{
    cl_error_desc_t err;
    char *str = adm_file_read_to_str(TMP_FILE, &err);

    UT_ASSERT(str != NULL);
    UT_ASSERT_EQUAL_STR(str, TEST_STR);

    os_free(str);
}

ut_test(adm_file_read_NULL_to_str_correct)
{
    cl_error_desc_t err;
    char *str = adm_file_read_to_str(NULL, &err);

    UT_ASSERT(str == NULL);
    UT_ASSERT(err.code == -ENOENT);

    os_free(str);
}

ut_test(adm_file_read_non_existing_to_str_correct)
{
    cl_error_desc_t err;
    char *str = adm_file_read_to_str("blah-blah-i-do-not-exist", &err);

    UT_ASSERT(str == NULL);
    UT_ASSERT(err.code == -ENOENT);

    os_free(str);
}

ut_test(adm_file_write_from_str_correct)
{
    cl_error_desc_t err;
    char *str;

    UT_ASSERT(unlink(TMP_FILE) == 0);

    adm_file_write_from_str(TMP_FILE, TEST_STR, &err);

    UT_ASSERT_EQUAL(err.code, EXA_SUCCESS);

    str = adm_file_read_to_str(TMP_FILE, &err);

    UT_ASSERT_EQUAL_STR(str, TEST_STR);

    os_free(str);
}

ut_test(adm_file_write_from_str_can_t_write_return_error)
{
    cl_error_desc_t err;

    UT_ASSERT(unlink(TMP_FILE) == 0);

    adm_file_write_from_str(OS_FILE_SEP "blah" OS_FILE_SEP "blah", TEST_STR, &err);

    UT_ASSERT_EQUAL(err.code, -ENOENT);
}

