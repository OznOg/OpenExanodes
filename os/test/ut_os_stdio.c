/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_stdio.h"
#include "os/include/os_file.h"

#include <unit_testing.h>
#include <string.h>
#include <sys/stat.h>

UT_SECTION(os_snprintf)

ut_test(write_into_big_enough_buffer)
{
    char ok[sizeof("OK")];

    UT_ASSERT(os_snprintf(ok, sizeof(ok), "%s", "OK") == 2);
}

ut_test(write_into_too_small_buffer)
{
#define FAILURE "FAILURE"
    char failure[4];

    int ret = os_snprintf(failure, sizeof(failure), "%s", FAILURE);
    UT_ASSERT(ret == sizeof(FAILURE) - 1);

    /* this check both size and null terminated */
    UT_ASSERT(!strcmp(failure, "FAI"));
}

ut_test(write_to_non_null_size_zero)
{
    char b;
    int ret = os_snprintf(&b, 0, "%d%d%d%d%d%d%d", 1, 2, 3, 4, 5, 6, 7);
    UT_ASSERT(ret == 7);
}

ut_test(write_to_null_size_zero)
{
    int ret = os_snprintf(NULL, 0, "%d", 1234567);
    UT_ASSERT(ret == 7);
}

UT_SECTION(os_vsnprintf)

/* Takes at most 3 args in the variable part */
static void __write_3(char *str, size_t size, ...)
{
    va_list al;

    va_start(al, size);
    os_vsnprintf(str, size, "%s %s %s", al);
    va_end(al);
}

ut_test(write_into_buffer_large_enough_succeeds)
{
    char hello[sizeof("three little worlds")];
    __write_3(hello, sizeof(hello), "three", "little", "words");
    UT_ASSERT(strcmp(hello, "three little words") == 0);
}

ut_test(write_into_too_small_a_buffer_fails)
{
    char hello[5];
    __write_3(hello, sizeof(hello), "citius", "altius", "fortius");
    /* __write2 asserts before this one has a chance to */
    UT_ASSERT(strcmp(hello, "citius altius fortius") != 0);
}

UT_SECTION(os_colorful_fprintf)

/* Compare with color shell output. The comparison won't tell if colors are
 * actually visible, but that's the best we can do... */
static void __check_color_output(const char *color_file)
{
#ifndef WIN32
    char diff_cmd[128];
#endif
    int r;

    r = system("echo \"\033[1;31m red \033[1;32m green \033[0m default\" > shell_color.txt");
    UT_ASSERT(r == 0);

#ifdef WIN32
    /* Color codes are not actually printed on Windows, so we can't easily
     * compare the file sizes */
    ut_printf("not testable on Windows");
#else
    os_snprintf(diff_cmd, sizeof(diff_cmd), "diff %s shell_color.txt", color_file);
    r = system(diff_cmd);
#endif

    unlink("shell_color.txt");
}

ut_test(color_write_in_file)
{
    FILE *f;
    int r;

    f = fopen("test_color.txt", "wt");
    UT_ASSERT(f != NULL);
    r = os_colorful_fprintf(f, OS_COLOR_RED " red " OS_COLOR_GREEN " green "
                            OS_COLOR_DEFAULT " default\n");
    UT_ASSERT(r > 0);
    fclose(f);

    __check_color_output("test_color.txt");
    unlink("test_color.txt");
}

UT_SECTION(os_colorful_vfprintf)

static void __color_write_3(FILE *f, const char *fmt, ...)
{
    va_list al;
    int r;

    va_start(al, fmt);
    r = os_colorful_vfprintf(f, fmt, al);
    va_end(al);
    UT_ASSERT(r > 0);
}

ut_test(color_write_var_args_in_file)
{
    FILE *f;

    f = fopen("test_color.txt", "wt");
    UT_ASSERT(f != NULL);
    __color_write_3(f, OS_COLOR_RED " %s " OS_COLOR_GREEN " %s "
                    OS_COLOR_DEFAULT " %s\n", "red", "green", "default");
    fclose(f);

    __check_color_output("test_color.txt");
    unlink("test_color.txt");
}
