/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "admind/src/adm_hostname.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_env.h"
#include "os/include/os_dir.h"
#include "os/include/os_file.h"
#include "os/include/os_network.h"

#include <sys/types.h>
#include <sys/stat.h>

UT_SECTION(hostname_override)

#define OVERRIDE  "toto987654321"

ut_test(adm_hostname_without_override_returns_real_hostname)
{
    char n[EXA_MAXSIZE_HOSTNAME + 1];

    UT_ASSERT(os_local_host_name(n, sizeof(n)) == 0);
    UT_ASSERT(strcmp(adm_hostname(), n) == 0);
}

ut_test(adm_hostname_with_override_returns_configured_hostname)
{
    adm_hostname_override(OVERRIDE);
    UT_ASSERT(strcmp(adm_hostname(), OVERRIDE) == 0);
}

ut_test(adm_hostname_after_override_reset_returns_real_hostname)
{
    char n[EXA_MAXSIZE_HOSTNAME + 1];

    adm_hostname_override(OVERRIDE);
    UT_ASSERT(strcmp(adm_hostname(), OVERRIDE) == 0);

    adm_hostname_reset();

    UT_ASSERT(os_local_host_name(n, sizeof(n)) == 0);
    UT_ASSERT(strcmp(adm_hostname(), n) == 0);
}

UT_SECTION(read_write_hostname_file)

#define HOSTNAME_FILE  "hostname"
#define HOSTNAME       "here"

#define TMP_DIR    "." OS_FILE_SEP "tmp"

ut_setup()
{
    UT_ASSERT_EQUAL(0, os_dir_create(TMP_DIR));
#ifdef WIN32
    _putenv(EXA_ENV_CACHE_DIR"="TMP_DIR);
#else
    putenv(EXA_ENV_CACHE_DIR"="TMP_DIR);
#endif
}

ut_cleanup()
{
    UT_ASSERT_EQUAL(0, os_dir_remove_tree(TMP_DIR));
}

static bool __create_dir(void)
{
    return os_dir_create_recursive(exa_env_cachedir()) == 0;
}

static void __remove_dir()
{
    UT_ASSERT(os_dir_remove_tree(exa_env_cachedir()) == 0);
}

static bool __file_exists(const char *filename)
{
    struct stat st;
    return stat(filename, &st) == 0;
}

ut_test(saving_valid_hostname_succeeds)
{
    char filename[OS_PATH_MAX];

    UT_ASSERT(__create_dir());

    UT_ASSERT_EQUAL(0, adm_hostname_save(HOSTNAME));

    exa_env_make_path(filename, sizeof(filename), exa_env_cachedir(), HOSTNAME_FILE);
    UT_ASSERT(__file_exists(filename));

    __remove_dir();
}

ut_test(saving_too_long_a_hostname_returns_EINVAL)
{
    char too_long_name[EXA_MAXSIZE_HOSTNAME * 2];

    UT_ASSERT(__create_dir());

    memset(too_long_name, 'a', sizeof(too_long_name) - 1);
    too_long_name[sizeof(too_long_name) - 1] = '\0';

    UT_ASSERT(adm_hostname_save(too_long_name) == -EINVAL);

    __remove_dir();
}

ut_test(load_existing_hostname_file_succeeds)
{
    char n[EXA_MAXSIZE_HOSTNAME + 1];

    UT_ASSERT(__create_dir());

    UT_ASSERT(adm_hostname_save(HOSTNAME) == 0);
    UT_ASSERT(adm_hostname_load(n) == 0 && strcmp(n, HOSTNAME) == 0);

    __remove_dir();
}

ut_test(load_non_existent_hostname_file_returns_ENOENT)
{
    char n[EXA_MAXSIZE_HOSTNAME + 1];

    __remove_dir();

    UT_ASSERT(adm_hostname_load(n) == -ENOENT);
}

ut_test(delete_existing_hostname_file_succeeds)
{
    char filename[OS_PATH_MAX];

    UT_ASSERT(__create_dir());

    UT_ASSERT(adm_hostname_save(HOSTNAME) == 0);
    UT_ASSERT(adm_hostname_delete_file() == 0);

    exa_env_make_path(filename, sizeof(filename), exa_env_cachedir(), HOSTNAME_FILE);
    UT_ASSERT(!__file_exists(filename));

    __remove_dir();
}

ut_test(delete_non_existent_hostname_file_succeeds)
{
    char filename[OS_PATH_MAX];

    exa_env_make_path(filename, sizeof(filename), exa_env_cachedir(), HOSTNAME_FILE);

    __remove_dir();
    UT_ASSERT(!__file_exists(filename));

    UT_ASSERT(adm_hostname_delete_file() == 0);
    UT_ASSERT(!__file_exists(filename));
}
