/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "config/pid_dir.h"

#include "os/include/os_daemon_lock.h"
#include "os/include/os_file.h"
#include "os/include/os_dir.h"

#ifndef WIN32
static void make_pidfile_path(const char *daemon_name, char *path, size_t size)
{
    os_snprintf(path, size, "%s" OS_FILE_SEP "%s.pid",
                getenv("OS_DAEMON_LOCK_DIR"), daemon_name);
}
#endif

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

static void __setup(void)
{
#ifndef WIN32
    UT_ASSERT(os_dir_create("./var_run") == 0);
    putenv("OS_DAEMON_LOCK_DIR=./var_run");
#endif
}

static void __cleanup(void)
{
#ifndef WIN32
    os_dir_remove_tree("./var_run");
#endif
}

UT_SECTION(create_lock)

ut_setup()
{
    __setup();
}

ut_cleanup()
{
    __cleanup();
}

ut_test(create_lock_with_null_name_returns_EINVAL)
{
    UT_ASSERT(os_daemon_lock(NULL, NULL) == -EINVAL);
    UT_ASSERT(os_daemon_lock("test", NULL) == -EINVAL);
}

ut_test(create_valid_lock_succeeds)
{
    os_daemon_lock_t *lock = NULL;

    UT_ASSERT(os_daemon_lock("toto", &lock) == 0);

#ifndef WIN32
    {
        char pidfile[OS_PATH_MAX];

        make_pidfile_path("toto", pidfile, sizeof(pidfile));
        UT_ASSERT(__file_exists(pidfile));
    }
#endif
}

UT_SECTION(unlock_daemon)

ut_setup()
{
    __setup();
}

ut_cleanup()
{
    __cleanup();
}

ut_test(lock_and_unlock_succeed)
{
    os_daemon_lock_t *lock;

    UT_ASSERT(os_daemon_lock("titi", &lock) == 0);
    os_daemon_unlock(lock);

#ifndef WIN32
    {
        char pidfile[OS_PATH_MAX];

        make_pidfile_path("titi", pidfile, sizeof(pidfile));
        UT_ASSERT(!__file_exists(pidfile));
    }
#endif
}

