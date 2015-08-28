/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "config/pid_dir.h"

#include "common/include/pid_file.h"

#include "os/include/os_file.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_daemon_parent.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
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

UT_SECTION(creating_a_pid_file)

ut_test(create_null_pid_file_returns_EINVAL)
{
    UT_ASSERT(pid_file_create(NULL) == -EINVAL);
}

ut_test(create_valid_pid_file_succeeds)
{
    UT_ASSERT(pid_file_create("toto") == 0);
    UT_ASSERT(__file_exists("." PID_DIR OS_FILE_SEP "toto.pid"));
    unlink("." PID_DIR OS_FILE_SEP "toto.pid");
}

UT_SECTION(deleting_a_pid_file)

ut_test(delete_existing_pid_file_succeeds)
{
    UT_ASSERT(pid_file_create("titi") == 0);
    pid_file_delete("titi");
    UT_ASSERT(!__file_exists("." PID_DIR OS_FILE_SEP "titi.pid"));
}

UT_SECTION(reading_a_pid_file)

ut_test(read_null_pid_file_returns_zero)
{
    UT_ASSERT(pid_file_read(NULL) == 0);
}

ut_test(read_non_existent_pid_file_returns_zero)
{
    /* Make sure the file does *not* exist */
    if (__file_exists("." PID_DIR OS_FILE_SEP "zorglub.pid"))
        UT_ASSERT(unlink("." PID_DIR OS_FILE_SEP "zorglub.pid") == 0);
    UT_ASSERT(pid_file_read("zorglub") == 0);
}

ut_test(read_existing_pid_file_returns_pid)
{
    uint32_t my_pid = os_daemon_current_pid();
    uint32_t pid_read;

    UT_ASSERT(pid_file_create("my") == 0);
    pid_read = pid_file_read("my");
    pid_file_delete("my");

    UT_ASSERT(pid_read == my_pid);
}

UT_SECTION(checking_a_daemon_is_running)

ut_test(checking_non_existent_pid_file_returns_false)
{
    /* Make sure the file does *not* exist */
    unlink("." PID_DIR OS_FILE_SEP "toto.pid");

    UT_ASSERT(!pid_file_daemon_is_running("toto"));
}

ut_test(checking_existing_pid_file_but_dead_daemon_returns_false)
{
    FILE *f;

    /* Create the pid file ourselves. Ideally, it should be created by
     * pid_file_create() but this means we'd have to have another process to
     * call it. */
    f = fopen("." PID_DIR OS_FILE_SEP "dead_daemon.pid", "wt");
    UT_ASSERT(f != NULL);
    UT_ASSERT(fprintf(f, "%"PRIu32"\n", 999999) > 0);
    fclose(f);

    /* Since we've written the pid file "by hand" above, check that we're
     * able to re-read it */
    UT_ASSERT(pid_file_read("dead_daemon") == 999999);

    UT_ASSERT(!pid_file_daemon_is_running("dead_daemon"));

    pid_file_delete("dead_daemon");
}

ut_test(checking_pid_thief_returns_false)
{
    /* In order to properly perform the "pid thief" test, we'd have to have a
     * process A create its pid file, kill that process, start a process B
     * *with the same pid*, and then perform the is_running check on A.
     *
     * We have no control on the pid nor on the name of this very process,
     * so we just lie on the daemon's name when creating and checking the
     * pid file. (Remember that what's passed to both functions is the name
     * of the daemon.)
     */
    UT_ASSERT(pid_file_create("thief") == 0);
    UT_ASSERT(!pid_file_daemon_is_running("thief"));

    pid_file_delete("thief");
}

ut_test(checking_existing_daemon_returns_true)
{
    /* Let's check the process running this unit test. Granted, it is not a
     * daemon, but this keeps the test case simple. */
    UT_ASSERT(pid_file_create("ut_pid_file") == 0);
    UT_ASSERT(pid_file_daemon_is_running("ut_pid_file"));

    pid_file_delete("ut_pid_file");
}
