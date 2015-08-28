/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "admind/services/lum/include/target_config.h"

#include "os/include/os_file.h"

#include "common/include/exa_error.h"

#include <sys/stat.h>

#define TEST_FILE "__target_test_conf.txt"

UT_SECTION(target_config_init_defaults)

ut_test(target_config_init_defaults_set_listen_address_to_ANY)
{
    target_config_init_defaults();
    UT_ASSERT_EQUAL(htonl(INADDR_ANY), target_config_get_listen_address());
}

UT_SECTION(target_config_parse_line)

ut_setup()
{
    target_config_init_defaults();
}

ut_cleanup()
{
}

ut_test(target_config_parse_line_NULL)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_TARGET_CONFIG_PARAM,
                    target_config_parse_line(NULL));
}

ut_test(target_config_parse_empty_line)
{
    UT_ASSERT_EQUAL(0, target_config_parse_line(""));
}

ut_test(target_config_parse_space_only_line)
{
    UT_ASSERT_EQUAL(0, target_config_parse_line("   "));
}

ut_test(target_config_parse_space_and_newline_line)
{
    UT_ASSERT_EQUAL(0, target_config_parse_line("   \n"));
}

ut_test(target_config_parse_line_wrong_syntax)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_TARGET_CONFIG_PARAM,
                    target_config_parse_line("missing equal sign"));
}

ut_test(target_config_parse_line_listen_address_not_an_ip)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_TARGET_CONFIG_PARAM,
                    target_config_parse_line("listen_address=not_an_ip"));
}

ut_test(target_config_parse_line_listen_address_wrong_ip)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_TARGET_CONFIG_PARAM,
                    target_config_parse_line("listen_address=333.444.555.111"));
}

ut_test(target_config_parse_line_unknown_parameter)
{
    UT_ASSERT_EQUAL(-LUM_ERR_UNKNOWN_TARGET_CONFIG_PARAM,
                    target_config_parse_line("tralala=1234"));
}

ut_test(target_config_parse_line_listen_address_correct_ip)
{
    struct in_addr addr;

    UT_ASSERT_EQUAL(0, target_config_parse_line("listen_address=127.0.0.1"));
    UT_ASSERT_EQUAL(htonl(INADDR_LOOPBACK), target_config_get_listen_address());

    UT_ASSERT_EQUAL(0, target_config_parse_line("listen_address=172.16.33.1"));
    os_inet_aton("172.16.33.1", &addr);
    UT_ASSERT_EQUAL(addr.s_addr, target_config_get_listen_address());
}

ut_test(target_config_parse_line_listen_address_correct_ip_with_spaces)
{
    struct in_addr addr;

    UT_ASSERT_EQUAL(0, target_config_parse_line("  listen_address =   172.16.55.1  \n"));

    os_inet_aton("172.16.55.1", &addr);
    UT_ASSERT_EQUAL(addr.s_addr, target_config_get_listen_address());
}

UT_SECTION(target_config_load)

ut_setup()
{
    target_config_init_defaults();
}

ut_cleanup()
{
}

ut_test(target_config_load_NULL_file_fails)
{
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_TARGET_CONFIG_FILE,
                    target_config_load(NULL));
}

ut_test(target_config_load_non_existent_file_succeeds)
{
    UT_ASSERT_EQUAL(0, target_config_load("notexisting.conf"));
}

ut_test(target_config_load_non_file_fails)
{
#ifndef WIN32
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_TARGET_CONFIG_FILE,
                    target_config_load("/dev/null"));
#else
    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_TARGET_CONFIG_FILE,
                    target_config_load("c:\\"));
#endif
}

/**
 * Write a null-terminated string to TEST_FILE.
 *
 * @param[in] str the string to write
 *
 * @Note: Writes nothing, but still creates the file, if str is NULL.
 */
static void __write_str_to_test_file(const char *str)
{
    FILE *fp = fopen(TEST_FILE, "wb");
    struct stat st;

    UT_ASSERT(fp != NULL);
    if (str != NULL)
    {
        UT_ASSERT_EQUAL(strlen(str), fwrite(str, 1, strlen(str), fp));
    }
    UT_ASSERT_EQUAL(0, fclose(fp));

    UT_ASSERT_EQUAL(0, stat(TEST_FILE, &st));
}

ut_test(target_config_load_empty_file_succeeds_and_keeps_defaults)
{
    __write_str_to_test_file(NULL);

    UT_ASSERT_EQUAL(0, target_config_load(TEST_FILE));

    UT_ASSERT_EQUAL(htonl(INADDR_ANY), target_config_get_listen_address());

    unlink(TEST_FILE);
}

ut_test(target_config_load_file_with_empty_line_succeeds_and_keeps_defaults)
{
    __write_str_to_test_file("\n\n\n");

    UT_ASSERT_EQUAL(0, target_config_load(TEST_FILE));

    UT_ASSERT_EQUAL(htonl(INADDR_ANY), target_config_get_listen_address());

    unlink(TEST_FILE);
}

ut_test(target_config_load_file_with_syntax_error_fails)
{
    __write_str_to_test_file("tralala_blabla");

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_TARGET_CONFIG_PARAM,
                    target_config_load(TEST_FILE));

    unlink(TEST_FILE);
}

ut_test(target_config_load_file_with_wrong_parameter_fails)
{
    __write_str_to_test_file("tralala=1234\n");

    UT_ASSERT_EQUAL(-LUM_ERR_UNKNOWN_TARGET_CONFIG_PARAM,
                    target_config_load(TEST_FILE));

    unlink(TEST_FILE);
}

ut_test(target_config_load_file_with_invalid_listen_address_fails)
{
    __write_str_to_test_file("listen_address=1234\n");

    UT_ASSERT_EQUAL(-LUM_ERR_INVALID_TARGET_CONFIG_PARAM,
                    target_config_load(TEST_FILE));

    unlink(TEST_FILE);
}

ut_test(target_config_load_file_with_valid_listen_address_works)
{
    __write_str_to_test_file("listen_address=127.0.0.1\n");

    UT_ASSERT_EQUAL(0, target_config_load(TEST_FILE));
    UT_ASSERT_EQUAL(htonl(INADDR_LOOPBACK), target_config_get_listen_address());

    unlink(TEST_FILE);
}
