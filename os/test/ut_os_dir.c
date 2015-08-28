/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/*
 * IMPORTANT!
 *
 * This unit test *must* be common to both Linux and Windows: we want the very
 * same test cases to work identically on both platforms.
 */

#include "os/include/os_dir.h"
#include "os/include/os_file.h"
#include "os/include/os_stdio.h"

#include <unit_testing.h>
#include <string.h>

#ifdef WIN32
#define mkdir  _mkdir
#define rmdir  _rmdir
#define unlink _unlink
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

UT_SECTION(creation_and_deletion)

static bool __dir_exists(const char *dir)
{
#ifdef WIN32
    DWORD attr = GetFileAttributes(dir);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(dir, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

ut_test(create_empty_name_dir)
{
    UT_ASSERT_EQUAL(-EINVAL, os_dir_create(""));
}

ut_test(create_current_dir)
{
    UT_ASSERT(os_dir_create(".") == 0);
}

ut_test(remove_current_dir)
{
    UT_ASSERT(os_dir_remove(".") != 0);
}

ut_test(null_pathes)
{
    UT_ASSERT(os_dir_remove(NULL) == -EINVAL);
    UT_ASSERT(os_dir_create(NULL) == -EINVAL);
}

#ifdef WIN32
ut_test(create_c_drive)
{
    UT_ASSERT(os_dir_create_recursive("c:\\") == 0);
    UT_ASSERT(__dir_exists("c:\\"));
}
#endif

ut_test(remove_inexistent_dir_is_ok)
{
    /* Ensure the directory does not exist */
    rmdir("_1_1_1");

    UT_ASSERT(os_dir_remove("_1_1_1") == 0);
}

ut_test(create_then_remove_dir)
{
    UT_ASSERT(os_dir_create("_1_1_1") == 0);
    UT_ASSERT(__dir_exists("_1_1_1"));
    UT_ASSERT(os_dir_remove("_1_1_1") == 0);
}

ut_test(create_existing_dir_is_ok)
{
    UT_ASSERT(os_dir_create("_1_1_1") == 0);
    UT_ASSERT(__dir_exists("_1_1_1"));
    UT_ASSERT(os_dir_create("_1_1_1") == 0);
    os_dir_remove("_1_1_1");
}

ut_test(create_dir_while_existing_file_with_same_name)
{
    int ret;
    FILE *f;

    f = fopen("_1_1_1", "wb");
    UT_ASSERT(f != NULL);
    fclose(f);

    ret = os_dir_create("_1_1_1");
    unlink("_1_1_1");

    UT_ASSERT(ret != 0);
}

UT_SECTION(recursive_creation_and_deletion)

#define TOTO "toto"

#define TOTOTATATUTU TOTO OS_FILE_SEP "tata" OS_FILE_SEP "tutu"

ut_test(null_pathes_in_recursive)
{
    UT_ASSERT_EQUAL(-EINVAL, os_dir_create_recursive(NULL));
    UT_ASSERT_EQUAL(-EINVAL, os_dir_remove_tree(NULL));
}

ut_test(create_single_dir_from_root)
{
    UT_ASSERT_EQUAL(0, os_dir_create_recursive(OS_FILE_SEP "tmp" OS_FILE_SEP TOTO));
    UT_ASSERT(__dir_exists(OS_FILE_SEP "tmp" OS_FILE_SEP TOTO));
    UT_ASSERT(os_dir_remove_tree(OS_FILE_SEP "tmp" OS_FILE_SEP TOTO) == 0);
}

#ifdef WIN32
ut_test(create_recursive_c_drive)
{
    UT_ASSERT(os_dir_create_recursive("c:\\") == 0);
    UT_ASSERT(__dir_exists("c:\\"));
}
#endif

ut_test(create_single_dir)
{
    UT_ASSERT(os_dir_create_recursive(TOTO) == 0);
    UT_ASSERT(__dir_exists(TOTO));
    UT_ASSERT(os_dir_remove_tree(TOTO) == 0);
}

ut_test(create_single_existing_dir)
{
    UT_ASSERT(os_dir_create_recursive(TOTO) == 0);
    UT_ASSERT(__dir_exists(TOTO));
    UT_ASSERT(os_dir_create_recursive(TOTO) == 0);
    UT_ASSERT(__dir_exists(TOTO));
    UT_ASSERT(os_dir_remove_tree(TOTO) == 0);
}

ut_test(create_multiple_dirs)
{
    UT_ASSERT_EQUAL(0, os_dir_create_recursive(TOTOTATATUTU));
    UT_ASSERT(__dir_exists(TOTOTATATUTU));
    UT_ASSERT(os_dir_remove_tree(TOTO) == 0);
}

ut_test(create_multiple_existing_dir)
{
    UT_ASSERT(os_dir_create_recursive(TOTOTATATUTU) == 0);
    UT_ASSERT(__dir_exists(TOTOTATATUTU));
    UT_ASSERT(os_dir_create_recursive(TOTOTATATUTU) == 0);
    UT_ASSERT(__dir_exists(TOTOTATATUTU));
    UT_ASSERT(os_dir_remove_tree(TOTOTATATUTU) == 0);
}

ut_test(create_multiple_dirs_prefixed_by_dot)
{
    UT_ASSERT_EQUAL(0, os_dir_create_recursive("." OS_FILE_SEP TOTOTATATUTU));
    UT_ASSERT(__dir_exists("." OS_FILE_SEP TOTOTATATUTU));
    UT_ASSERT(os_dir_remove_tree("." OS_FILE_SEP TOTO) == 0);
}

ut_test(create_slash_ended_dir)
{
    UT_ASSERT(os_dir_create_recursive(TOTO OS_FILE_SEP) == 0);
    UT_ASSERT(__dir_exists(TOTO));
    UT_ASSERT(os_dir_remove_tree(TOTO OS_FILE_SEP) == 0);
}
