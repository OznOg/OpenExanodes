/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "common/include/exa_env.h"
#include "common/include/exa_error.h"

#include "os/include/os_dir.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"
#ifdef WIN32
#include "os/include/os_windows.h"
#endif
#include "os/include/strlcpy.h"

#ifdef WIN32
#define putenv _putenv
#endif

/* Size must be enough to hold <env var>=<value> */
typedef char setting_t[64];

typedef struct {
    const char *(*fn)(void);    /* Env accessor */
    const char *var;            /* Env var returned by accessor */
    setting_t setting;          /* Used to set the env var. *Must* be static. */
    bool is_dir;                /* Env var represents a directory. */
} env_t;

/* The array is statically allocated, hence the 'setting' field of each
   entry is too. */
static env_t envs[] = {
    { exa_env_bindir,      EXA_ENV_BIN_DIR,       "", true },
    { exa_env_sbindir,     EXA_ENV_SBIN_DIR,      "", true },
    { exa_env_libdir,      EXA_ENV_LIB_DIR,       "", true },
    { exa_env_piddir,      EXA_ENV_PID_DIR,       "", true },
    { exa_env_nodeconfdir, EXA_ENV_NODE_CONF_DIR, "", true },
    { exa_env_cachedir,    EXA_ENV_CACHE_DIR,     "", true },
    { exa_env_datadir,     EXA_ENV_DATA_DIR,      "", true },
    { exa_env_logdir,      EXA_ENV_LOG_DIR,       "", true },
#ifdef WITH_PERF
    { exa_env_perf_config, EXA_ENV_PERF_CONFIG,   "", false },
#endif
    { NULL,                NULL,                  "", false }
};

/* Set value to NULL to remove (unset) the variable from the environment.
 * NOTE 'setting' *must* be statically allocated. */
static void set_env_var(setting_t setting, const char *var, const char *value)
{
    UT_ASSERT(setting != NULL && var != NULL);

    if (value == NULL)
        os_snprintf(setting, sizeof(setting_t), "%s", var);
    else
        os_snprintf(setting, sizeof(setting_t), "%s=%s", var, value);

    putenv(setting);
}

UT_SECTION(exa_env_XXXdir)

ut_test(null_env_var_returns_null)
{
    int i;

    for (i = 0; envs[i].fn != NULL; i++)
    {
        set_env_var(envs[i].setting, envs[i].var, NULL);
        UT_ASSERT(envs[i].fn() == NULL);
    }
}

ut_test(env_var_no_trailing_slash_returns_env_var_with_trailing_slash_if_dir)
{
    int i;

    for (i = 0; envs[i].fn != NULL; i++)
    {
        set_env_var(envs[i].setting, envs[i].var, "hello");
        if (envs[i].is_dir)
            UT_ASSERT_EQUAL_STR("hello" OS_FILE_SEP, envs[i].fn());
        else
            UT_ASSERT_EQUAL_STR("hello", envs[i].fn());
    }
}

ut_test(env_var_trailing_slash_returns_env_var)
{
    int i;

    for (i = 0; envs[i].fn != NULL; i++)
    {
        set_env_var(envs[i].setting, envs[i].var, "hello" OS_FILE_SEP);
        UT_ASSERT_EQUAL_STR("hello" OS_FILE_SEP, envs[i].fn());
    }
}

UT_SECTION(exa_env_properly_set)

ut_test(one_var_not_set_returns_false)
{
    int i;

    /* Properly set all variables except the first one (not set) */
    for (i = 0; envs[i].fn != NULL; i++)
        set_env_var(envs[i].setting, envs[i].var, i == 0 ? NULL : "dummy");

    UT_ASSERT(!exa_env_properly_set());
}

ut_test(one_var_empty_returns_false)
{
    int i;

    /* Properly set all variables except the first one (empty) */
    for (i = 0; envs[i].fn != NULL; i++)
        set_env_var(envs[i].setting, envs[i].var, i == 0 ? "" : "dummy");

    UT_ASSERT(!exa_env_properly_set());
}

ut_test(all_vars_non_empty_returns_true)
{
    int i;

    for (i = 0; envs[i].fn != NULL; i++)
        set_env_var(envs[i].setting, envs[i].var, "dummy");

    UT_ASSERT(exa_env_properly_set());
}

UT_SECTION(exa_env_make_path)

/* Fake "usr sbin" directory for both Windows and Linux. */
#define USR_SBIN  "." OS_FILE_SEP "usr_sbin"

ut_setup()
{
    static setting_t setting;
    set_env_var(setting, EXA_ENV_SBIN_DIR, USR_SBIN);
}

ut_cleanup()
{
    /* os_dir_remove_tree(USR_SBIN); */
}

ut_test(buffer_null_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, exa_env_make_path(NULL, 10, "dummy_dir", "dummy_format"));
}

ut_test(size_zero_returns_EINVAL)
{
    char buf[10];
    UT_ASSERT_EQUAL(-EINVAL, exa_env_make_path(buf, 0, "dummy_dir", "dummy_format"));
}

ut_test(dir_null_returns_EINVAL)
{
    char buf[10];
    UT_ASSERT_EQUAL(-EINVAL, exa_env_make_path(buf, sizeof(buf), NULL, "dummy_format"));
}

ut_test(null_format)
{
    char buf[OS_PATH_MAX];
    UT_ASSERT_EQUAL(0, exa_env_make_path(buf, sizeof(buf), exa_env_sbindir(), NULL));
    UT_ASSERT_EQUAL(0, strcmp(buf, USR_SBIN OS_FILE_SEP));
}

ut_test(constant_format)
{
    char buf[OS_PATH_MAX];

    UT_ASSERT_EQUAL(0, exa_env_make_path(buf, sizeof(buf), exa_env_sbindir(),
                                         "toto", NULL));
    UT_ASSERT_EQUAL(0, strcmp(buf, USR_SBIN OS_FILE_SEP "toto"));
}

ut_test(buffer_too_small)
{
    char buf[1];
    UT_ASSERT_EQUAL(-ENAMETOOLONG, exa_env_make_path(buf, sizeof(buf), "toto", "tata"));
}


ut_test(without_sep)
{
    char buf[OS_PATH_MAX];

    UT_ASSERT_EQUAL(0, exa_env_make_path(buf, sizeof(buf), exa_env_sbindir(),
                                         "toto", "tata"));
    UT_ASSERT(strcmp(buf, USR_SBIN OS_FILE_SEP "toto") == 0);
}


ut_test(with_format)
{
    char buf[OS_PATH_MAX];

    UT_ASSERT_EQUAL(0, exa_env_make_path(buf, sizeof(buf), exa_env_sbindir(),
                                         "toto" OS_FILE_SEP "%s_is_%d", "answer", 42));
    UT_ASSERT(strcmp(buf, USR_SBIN OS_FILE_SEP "toto" OS_FILE_SEP "answer_is_42") == 0);
}


ut_test(with_dir_sep)
{
    char buf[OS_PATH_MAX];

    UT_ASSERT_EQUAL(0, exa_env_make_path(buf, sizeof(buf), exa_env_sbindir(),
                                         OS_FILE_SEP "toto" OS_FILE_SEP "%s", "tata"));
    UT_ASSERT(strcmp(buf, USR_SBIN OS_FILE_SEP "toto" OS_FILE_SEP "tata") == 0);
}


ut_test(with_file_sep)
{
    char buf[OS_PATH_MAX];

    UT_ASSERT_EQUAL(0, exa_env_make_path(buf, sizeof(buf), exa_env_sbindir(),
                                         "toto%s", OS_FILE_SEP "tata"));
    UT_ASSERT(strcmp(buf, USR_SBIN OS_FILE_SEP "toto" OS_FILE_SEP "tata") == 0);
}


ut_test(with_duplicate_sep)
{
    char buf[OS_PATH_MAX];

    UT_ASSERT_EQUAL(0, exa_env_make_path(buf, sizeof(buf), exa_env_sbindir(),
                                         OS_FILE_SEP "toto" OS_FILE_SEP "%s", OS_FILE_SEP "tata"));
    UT_ASSERT(strcmp(buf, USR_SBIN OS_FILE_SEP "toto" OS_FILE_SEP OS_FILE_SEP "tata") == 0);
}
