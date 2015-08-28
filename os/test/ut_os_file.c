/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "os/include/os_file.h"
#include "os/include/os_dir.h"
#include "os/include/os_mem.h"
#include <unit_testing.h>

UT_SECTION(file_renaming)

static void __create_file(const char *filename)
{
    FILE *f = fopen(filename, "wt");
    UT_ASSERT(f != NULL);
    fclose(f);
}

static void __delete_file(const char *filename)
{
#ifdef WIN32
    DeleteFile(filename);
#else
    unlink(filename);
#endif
}

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

ut_test(rename_nonexistent_file_fails)
{
    const char *filename = "iznogoud";
    int retval;

    /* Ensure the file doesn't exist */
    __delete_file(filename);

    retval = os_file_rename(filename, "vizir");
    UT_ASSERT(retval == -ENOENT);
}

ut_test(rename_from_null_fails)
{
    int retval;

    retval = os_file_rename(NULL, "unused");
    UT_ASSERT(retval == -EINVAL);
}

ut_test(rename_to_null_fails)
{
    const char *filename = "toto";
    int retval;

    __create_file(filename);
    retval = os_file_rename(filename, NULL);
    UT_ASSERT(retval == -EINVAL);
    __delete_file(filename);
}

ut_test(rename_directory_fails)
{
    const char *path1 = "dir1";
    const char *path2 = "dir2";
    bool ok;
    int retval;

    UT_ASSERT(os_dir_create(path1) == 0);
    UT_ASSERT(os_dir_remove(path2) == 0);

    retval = os_file_rename(path1, path2);
    ok = retval == -EISDIR && !__dir_exists(path2);

    UT_ASSERT(os_dir_remove(path1) == 0);
    UT_ASSERT(os_dir_remove(path2) == 0);

    UT_ASSERT(ok);
}

ut_test(rename_file_to_directory_fails)
{
    const char *filename = "file1";
    const char *dir = "dir2";
    bool ok;
    int retval;

    __create_file(filename);
    UT_ASSERT(os_dir_create(dir) == 0);

    retval = os_file_rename(filename, dir);
    ok = retval == -EISDIR && __file_exists(filename) && __dir_exists(dir);

    UT_ASSERT(os_dir_remove(dir) == 0);
    __delete_file(filename);

    UT_ASSERT(ok);
}

ut_test(rename_with_different_name_is_ok)
{
    const char *src = "source_file";
    const char *dest = "dest_file";
    bool ok;

    /* Ensure the destination file doesn't exist */
    __delete_file(dest);

    __create_file(src);
    ok = os_file_rename(src, dest) == 0 && !__file_exists(src)
        && __file_exists(dest);

    __delete_file(src);
    __delete_file(dest);

    UT_ASSERT(ok);
}

ut_test(rename_with_same_name_is_ok)
{
    const char *filename = "same";
    int ret;

    __create_file(filename);
    ret = os_file_rename(filename, filename);
    __delete_file(filename);

    UT_ASSERT(ret == 0);
}

ut_test(rename_to_existing_file_is_ok)
{
    const char *src = "source_file";
    const char *dest = "dest_file";
    bool ok;

    __create_file(src);
    __create_file(dest);

    ok = os_file_rename(src, dest) == 0 && !__file_exists(src)
        && __file_exists(dest);

    __delete_file(dest);
    __delete_file(src);

    UT_ASSERT(ok);
}

UT_SECTION(basename_extraction)

ut_test(basename_of_null_returns_null)
{
    UT_ASSERT(os_basename(NULL) == NULL);
}

ut_test(basename_of_filename_returns_filename)
{
    char filename[32] = "toto.txt";
    UT_ASSERT(strcmp(os_basename(filename), "toto.txt") == 0);
}

ut_test(basename_of_relative_file_path_returns_filename)
{
    char path[OS_PATH_MAX] = "this/is/a/relative/path/to/nowhere";
    UT_ASSERT(strcmp(os_basename(path), "nowhere") == 0);
}

ut_test(basename_of_abs_file_path_returns_filename)
{
    char path[OS_PATH_MAX] = "/an/absolute/path/to/the/void";
    UT_ASSERT(strcmp(os_basename(path), "void") == 0);
}

ut_test(basename_of_path_without_basename_returns_empty_string)
{
    char path[OS_PATH_MAX] = "a/non-descript/path/";
    UT_ASSERT(strcmp(os_basename(path), "") == 0);
}

UT_SECTION(program_name)

ut_test(program_name_of_null_returns_null)
{
    UT_ASSERT(os_program_name(NULL) == NULL);
}

ut_test(program_name_of_filename_succeeds)
{
    char filename[32] = "hello_world.exe";
#ifdef WIN32
    UT_ASSERT(strcmp(os_program_name(filename), "hello_world") == 0);
#else
    UT_ASSERT(strcmp(os_program_name(filename), filename) == 0);
#endif
}

ut_test(program_name_of_relative_path_returns_program_name)
{
    char filename[32] = "short/path/to/hello_world.exe";
#ifdef WIN32
    UT_ASSERT(strcmp(os_program_name(filename), "hello_world") == 0);
#else
    UT_ASSERT(strcmp(os_program_name(filename), "hello_world.exe") == 0);
#endif
}

ut_test(program_name_of_abs_path_returns_program_name)
{
    char filename[32] = "/short/path/to/hello_world.exe";
#ifdef WIN32
    UT_ASSERT(strcmp(os_program_name(filename), "hello_world") == 0);
#else
    UT_ASSERT(strcmp(os_program_name(filename), "hello_world.exe") == 0);
#endif
}

ut_test(program_name_of_path_without_basename_returns_empty_string)
{
    char path[OS_PATH_MAX] = "some/path/without/program/at/the/end/";
    UT_ASSERT(strcmp(os_program_name(path), "") == 0);
}

UT_SECTION(path_absoluteness)

ut_test(absolute_path_is_absolute)
{
#ifdef WIN32
    UT_ASSERT(os_path_is_absolute("\\Program Files\\Exanodes\\exa_admind"));
    UT_ASSERT(os_path_is_absolute("C:\\Program Files\\Exanodes\\exa_admind"));
#else
    UT_ASSERT(os_path_is_absolute("/usr/sbin/exa_admind"));
#endif
}

ut_test(relative_path_is_not_absolute)
{
#ifdef WIN32
    UT_ASSERT(!os_path_is_absolute("Program Files\\Exanodes\\exa_admind"));
    UT_ASSERT(!os_path_is_absolute("C:Program Files\\Exanodes\\exa_admind"));
#else
    UT_ASSERT(!os_path_is_absolute("usr/sbin/exa_admind"));
#endif
}

ut_test(os_dirname_tests)
{
    char *str;

    UT_ASSERT_EQUAL_STR(".", os_dirname(NULL));
    str = os_strdup("");
    UT_ASSERT_EQUAL_STR(".", os_dirname(str));
    os_free(str);
    str = os_strdup("blah.txt");
    UT_ASSERT_EQUAL_STR(".", os_dirname(str));
    os_free(str);
#ifndef WIN32
    str = os_strdup("/a/b/c/d/blah.txt");
    UT_ASSERT_EQUAL_STR("/a/b/c/d", os_dirname(str));
#else
    str = os_strdup("c:\\a\\b\\c\\d\\blah.txt");
    UT_ASSERT_EQUAL_STR("c:\\a\\b\\c\\d", os_dirname(str));
#endif
    os_free(str);
}
