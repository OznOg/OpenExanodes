/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_daemon_parent.h"
#include "os/include/os_daemon_child.h"
#include "os/include/os_time.h"
#include "os/include/os_file.h"
#include "os/include/os_stdio.h"

#define FAILED_INIT_VALUE      -666
#define EXIT_WITH_ERROR_VALUE  55

#ifdef CHILD

#include <string.h>
#ifndef WIN32
#include <signal.h>
#endif

static char *behavior = NULL;
static bool quit = false;

#ifndef WIN32
static void term_handler(int sig)
{
    quit = true;
}
#endif

int daemon_init(int argc, char **argv)
{
#ifndef WIN32
    signal(SIGTERM, &term_handler);
#endif

    behavior = argv[1];
    if (behavior == NULL)
        goto unknown_behavior;

    if (strcmp(behavior, "all_good") == 0)
        return 0;
    else if (strcmp(behavior, "failed_init") == 0)
        return FAILED_INIT_VALUE;
    else if (strcmp(behavior, "1min_lifespan") == 0)
        return 0;
    else if (strcmp(behavior, "handle_quit") == 0)
        return 0;
    else if (strcmp(behavior, "exit_with_error") == 0)
        return 0;

unknown_behavior:
    /* Should not get there. This positive value will make
     * the daemon child lib assert */
    return 3;
}

int daemon_main(void)
{
    if (strcmp(behavior, "1min_lifespan") == 0)
    {
        /* Make the daemon live long enough so that its parent
         * (this unit test) may perform some checks on it. */
        os_sleep(60);
        return 0;
    }
    else if (strcmp(behavior, "handle_quit") == 0)
    {
        while (!quit && !daemon_must_quit())
            os_sleep(1);
    }
    else if (strcmp(behavior, "exit_with_error") == 0)
        return EXIT_WITH_ERROR_VALUE;

    return 0;
}

#else /* CHILD */

/* Do the trick to compile outside ut framework */
#include <unit_testing.h>

/*
 * Helper function to spawn a daemon with a given behavior.
 * Returns the spawned daemon or OS_DAEMON_INVALID.
 */
static os_daemon_t __spawn_daemon_get_result(const char *behavior, int *result)
{
    /* Yeah, this cast is *bad* */
    char *const argv[] = { "daemon_test_child", (char *)behavior, NULL };
    os_daemon_t daemon;
    int r;

    r = os_daemon_spawn(argv, &daemon);
    if (result)
        *result = r;
    if (r == 0 && daemon != OS_DAEMON_INVALID)
        return daemon;

    return OS_DAEMON_INVALID;
}

#define __spawn_daemon(behavior)  __spawn_daemon_get_result(behavior, NULL)

UT_SECTION(os_daemon_spawn)

ut_test(init_fails)
{
    int r;

    UT_ASSERT(__spawn_daemon_get_result("failed_init", &r) == OS_DAEMON_INVALID
              && r == FAILED_INIT_VALUE);
}

ut_test(all_success)
{
    os_daemon_t daemon;

    daemon = __spawn_daemon("all_good");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);
    os_daemon_free(daemon);
}

UT_SECTION(os_daemon_current_pid)

ut_test(pid_of_current_process)
{
    UT_ASSERT(os_daemon_current_pid() > 0);
}

UT_SECTION(os_daemon_wait)

ut_test(wait_for_daemon_that_exited_with_success)
{
    os_daemon_t daemon;

    daemon = __spawn_daemon("all_good");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);
    UT_ASSERT(os_daemon_wait(daemon) == 0);
    os_daemon_free(daemon);
}

ut_test(wait_for_daemon_that_exited_with_error)
{
    os_daemon_t daemon;

    daemon = __spawn_daemon("exit_with_error");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);
    UT_ASSERT(os_daemon_wait(daemon) == EXIT_WITH_ERROR_VALUE);
    os_daemon_free(daemon);
}

ut_test(wait_for_daemon_brutally_terminated)
{
    os_daemon_t daemon;

    daemon = __spawn_daemon("1min_lifespan");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

    /* Forcefully terminate the child. Do not use os_daemon_terminate() to
     * avoid a circular dependency (since os_daemon_terminate() test cases
     * use os_daemon_status()). */
#ifdef WIN32
    TerminateProcess(daemon, 1); /* Force exit code to 1 */
#else
    kill(daemon, SIGKILL);
#endif
    UT_ASSERT(os_daemon_wait(daemon) != 0);

    os_daemon_free(daemon);
}

UT_SECTION(os_daemon_status)

ut_test(status_of_alive_daemon)
{
    os_daemon_t daemon;
    bool alive;

    daemon = __spawn_daemon("1min_lifespan");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

    UT_ASSERT(os_daemon_status(daemon, &alive, NULL) == 0 && alive);

    /* Cleanup. The test is about os_daemon_status() above, not about
     * os_daemon_wait() below. */
#ifdef WIN32
    TerminateProcess(daemon, 0);
#else
    kill(daemon, SIGKILL);
#endif
    os_daemon_wait(daemon);

    os_daemon_free(daemon);
}

ut_test(status_of_daemon_that_exited_with_success)
{
    os_daemon_t daemon;
    bool alive;

    daemon = __spawn_daemon("all_good");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

    while (os_daemon_status(daemon, &alive, NULL) == 0 && alive)
        os_sleep(1);
    UT_ASSERT(!alive);

    os_daemon_free(daemon);
}

ut_test(status_of_daemon_that_was_brutally_terminated)
{
    os_daemon_t daemon;
    bool alive;
    int status;
    int r;

    daemon = __spawn_daemon("1min_lifespan");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

    UT_ASSERT(os_daemon_status(daemon, &alive, NULL) == 0 && alive);

#ifdef WIN32
    TerminateProcess(daemon, 1); /* Force exit code to 1 */
#else
    kill(daemon, SIGKILL);
#endif

    /* Must loop: the daemon may not have been terminated right away!
     * (as witnessed on Windows) */
    while (true)
    {
        r = os_daemon_status(daemon, &alive, &status);
        if (r == 0 && !alive)
            break;
        os_sleep(1);
    }

    UT_ASSERT(r == 0 && !alive && status != 0);

    os_daemon_free(daemon);
}

UT_SECTION(os_daemon_exists)

ut_test(alive_daemon_exists)
{
    os_daemon_t daemon;

    daemon = __spawn_daemon("1min_lifespan");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

    UT_ASSERT(os_daemon_exists(daemon));

#ifdef WIN32
    TerminateProcess(daemon, 1); /* Force exit code to 1 */
#else
    kill(daemon, SIGKILL);
#endif

    os_daemon_wait(daemon);
    os_daemon_free(daemon);
}

ut_test(dead_daemon_does_not_exist)
{
    os_daemon_t daemon;

    daemon = __spawn_daemon("1min_lifespan");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

#ifdef WIN32
    TerminateProcess(daemon, 1); /* Force exit code to 1 */
#else
    kill(daemon, SIGKILL);
#endif

    UT_ASSERT(os_daemon_wait(daemon) >= 0);
    UT_ASSERT(!os_daemon_exists(daemon));

    os_daemon_free(daemon);
}

#ifndef WIN32
/* Helper function that returns when the given daemon is zombie.
 * Only makes sense on Linux. */
static void wait_zombie(os_daemon_t daemon)
{
    char cmd[128];
    FILE *f;
    char buf[128];
    bool zombie = false;

    do
    {
        os_snprintf(cmd, sizeof(cmd), "cat /proc/%u/status | grep State:", daemon);

        f = popen(cmd, "r");
        UT_ASSERT(f != NULL);

        UT_ASSERT(fgets(buf, sizeof(buf), f) != NULL);

        pclose(f);

        zombie = strstr(buf, "zombie") != NULL;
        if (!zombie)
            os_sleep(1);
    } while (!zombie);
}
#endif

ut_test(zombie_daemon_exists)
{
#ifdef WIN32
    /* FIXME Replace with UT_SKIP() once implemented */
    ut_printf("This test case is irrelevant on Windows.");
    return;
#else
    os_daemon_t daemon;

    daemon = __spawn_daemon("1min_lifespan");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

    kill(daemon, SIGKILL);
    wait_zombie(daemon);

    UT_ASSERT(os_daemon_exists(daemon));

    os_daemon_wait(daemon);
    os_daemon_free(daemon);
#endif
}

UT_SECTION(os_daemon_terminate)

ut_test(terminate_existing_daemon)
{
    os_daemon_t daemon;
    bool alive;

    daemon = __spawn_daemon("1min_lifespan");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

    UT_ASSERT(os_daemon_status(daemon, &alive, NULL) == 0 && alive);

    os_daemon_terminate(daemon);
    /* Interrupted by signal => strictly positive exit code */
    UT_ASSERT(os_daemon_wait(daemon) > 0);

    os_daemon_free(daemon);
}

ut_test(tell_quit_existing_daemon)
{
    os_daemon_t daemon;
    bool alive;

    daemon = __spawn_daemon("handle_quit");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

    UT_ASSERT(os_daemon_status(daemon, &alive, NULL) == 0 && alive);

    os_daemon_tell_quit(daemon);
    UT_ASSERT(os_daemon_wait(daemon) == 0);

    os_daemon_free(daemon);
}

UT_SECTION(os_daemon_from_to_pid)

ut_test(invalid_daemon_to_pid)
{
    uint32_t pid;
    int c;

    c = os_daemon_to_pid(OS_DAEMON_INVALID, &pid);
    UT_ASSERT(c == -EINVAL && pid == 0);
}

ut_test(daemon_to_pid)
{
    os_daemon_t daemon;
    uint32_t pid, platform_pid;
    int c, w;

    daemon = __spawn_daemon("1min_lifespan");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

#if WIN32
    platform_pid = (uint32_t)GetProcessId(daemon);
#else
    platform_pid = (uint32_t)daemon;
#endif

    c = os_daemon_to_pid(daemon, &pid);
    /* Daemon no longer needed, kill it*/
    os_daemon_terminate(daemon);
    w = os_daemon_wait(daemon);

    UT_ASSERT(c == 0 && pid == platform_pid);
    /* Interrupted by signal => strictly positive exit code */
    UT_ASSERT(w > 0);

    os_daemon_free(daemon);
}

ut_test(daemon_from_ok_pid)
{
    os_daemon_t daemon, daemon2;
    uint32_t pid, pid2;
    int c, w = 1;

    daemon = __spawn_daemon("1min_lifespan");
    UT_ASSERT(daemon != OS_DAEMON_INVALID);

    c = os_daemon_to_pid(daemon, &pid);
    if (c != 0)
    {
        os_daemon_terminate(daemon);
        w = os_daemon_wait(daemon);
    }
    UT_ASSERT(c == 0);
    /* Interrupted by signal => strictly positive exit code */
    UT_ASSERT(w > 0);

    c = os_daemon_from_pid(&daemon2, pid);

    /* Daemon no longer needed, kill it*/
    os_daemon_terminate(daemon);
    w = os_daemon_wait(daemon);
    UT_ASSERT(c == 0);
    /* Interrupted by signal => strictly positive exit code */
    UT_ASSERT(w > 0);

    c = os_daemon_to_pid(daemon2, &pid2);
    UT_ASSERT(c == 0);

    UT_ASSERT(pid2 == pid);

    os_daemon_free(daemon2);
}

ut_test(daemon_from_pid_0_returns_einval)
{
    os_daemon_t daemon;
    UT_ASSERT(os_daemon_from_pid(&daemon, 0) == -EINVAL
              && daemon == OS_DAEMON_INVALID);
}

#endif  /* CHILD */
