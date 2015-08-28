/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_daemon_parent.h"
#include "os/src-linux/os_daemon_common.h"
#include "os/include/os_time.h"
#include "os/include/os_file.h"
#include "os/include/strlcpy.h"
#include "os/include/os_assert.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <errno.h>


/* XXX Split this up */
int os_daemon_spawn(char *const argv[], os_daemon_t *daemon)
{
    const char *daemon_name = argv[0];
    pid_t pid;
    int child_status;
    int i;
    int pipe_fds[2] = { -1, -1 };
    fd_set rfds;
    ssize_t count;

    OS_ASSERT(argv[0] != NULL);
    OS_ASSERT(daemon != NULL);

    *daemon = OS_DAEMON_INVALID;

    if (pipe(pipe_fds) == -1)
    {
        printf("failed creating pipe: %d=%m\n", errno);
        return errno;
    }

    pid = fork();
    switch (pid)
    {
    case -1:
        printf("failed forking: %d=%m\n", errno);
        return errno;

    case 0:
        if (dup2(pipe_fds[1], DAEMON_CHILD_STATUS_FD) < 0)
        {
            printf("failed dup'ing status fd: %d=%m\n", errno);
            exit(1);
        }
        for (i = getdtablesize(); i >= 0; i--)
            /* FIXME Close stdin and stdout */
            if (i != DAEMON_CHILD_STATUS_FD && i != 0 && i != 1)
                close(i);

        if (execv(daemon_name, argv) < 0)
        {
            printf("failed exec'ing daemon %s: %d=%m\n", daemon_name, errno);
            exit(1);
        }

        /* Can't happen */
        return false;

    default:
        /* XXX Forcefully kill child in case of error */
        /* FIXME Refactor error handling */

        close(pipe_fds[1]);

        /* Monitor pipe */
        FD_ZERO(&rfds);
        FD_SET(pipe_fds[0], &rfds);

        if (select(FD_SETSIZE, &rfds, NULL, NULL, NULL) < 0)
        {
            int err = -errno;
            printf("select failed on child fd for daemon %s: %d=%m\n",
                   daemon_name, err);
            close(pipe_fds[0]);
            return err;
        }

        count = read(pipe_fds[0], &child_status, sizeof(child_status));
        if (count < 0)
        {
            int err = -errno;
            printf("reading daemon %s's status failed: %d=%m\n",
                   daemon_name, err);
            close(pipe_fds[0]);
            return err;
        }
        else
        if (count == 0)
        {
            printf("got 0 bytes from daemon %s, connection broken\n", daemon_name);
            close(pipe_fds[0]);
            OS_ASSERT(waitpid(pid, &child_status, 0) == pid);
            if (WIFEXITED(child_status))
                printf("unexpected termination of daemon %s, exit code %d\n",
                       daemon_name, WEXITSTATUS(child_status));
            else
                printf("unexpected termination of daemon %s, unknown exit code\n",
                       daemon_name);
            return -ECONNRESET;
        }
        else
        if (count != sizeof(child_status))
        {
            printf("reading daemon %s's status: bad count %zd (broken pipe?)\n",
                   daemon_name, count);
            close(pipe_fds[0]);
            return -EINVAL;
        }
        else
        if (child_status != 0)
        {
            OS_ASSERT(child_status < 0);
            /* FIXME Use exa_error_msg(child_status) */
            printf("failed spawning daemon %s: %d\n", daemon_name, child_status);
            close(pipe_fds[0]);
            waitpid(pid, NULL, 0);
            return child_status;
        }

        close(pipe_fds[0]);
	*daemon = pid;

        return 0;
    }

    return -EINVAL;
}


uint32_t os_daemon_current_pid(void)
{
    return (uint32_t)getpid();
}

int os_daemon_from_pid(os_daemon_t *daemon, uint32_t pid)
{
    if (pid <= 0)
    {
        *daemon = OS_DAEMON_INVALID;
        return -EINVAL;
    }

    *daemon = (os_daemon_t)pid;
    return 0;
}

int os_daemon_to_pid(os_daemon_t daemon, uint32_t *pid)
{
    if (daemon == OS_DAEMON_INVALID)
    {
        *pid = 0;
        return -EINVAL;
    }

    *pid = (uint32_t)daemon;
    return 0;
}

void __os_daemon_free(os_daemon_t *daemon)
{
    /* Nothing to free as it's merely a pid. */
    *daemon = OS_DAEMON_INVALID;
}

int os_daemon_status(os_daemon_t daemon, bool *alive, int *status)
{
    pid_t w;
    int exit_code;

    if (daemon <= 1)
        return -EINVAL;

    w = waitpid(daemon, &exit_code, WNOHANG);
    switch (w)
    {
    case -1:
        return -errno;

    case 0:
        /* The daemon is alive */
        if (alive != NULL)
            *alive = true;
        return 0;

    default:
        /* The daemon is dead */
        OS_ASSERT(w == daemon);
        if (alive != NULL)
            *alive = false;
        if (status != NULL)
        {
            if (WIFEXITED(exit_code))
                *status = WEXITSTATUS(exit_code);
            else
                *status = exit_code;
        }
        return 0;
    }
}

bool os_daemon_exists(os_daemon_t daemon)
{
    uint32_t pid;

    if (os_daemon_to_pid(daemon, &pid) != 0)
        return false;

    return kill(pid, 0) == 0;
}

void os_daemon_terminate(os_daemon_t daemon)
{
    /* Pids less than 1 (inclusive) aren't allowed (1 = init process) */
    if (daemon > 1)
        kill(daemon, SIGKILL);
}

void os_daemon_tell_quit(os_daemon_t daemon)
{
    /* Pids less than 1 (inclusive) aren't allowed (1 = init process) */
    if (daemon > 1)
        kill(daemon, SIGTERM);
}

int os_daemon_wait(os_daemon_t daemon)
{
    int exit_code;

    OS_ASSERT(daemon != OS_DAEMON_INVALID);

    if (waitpid(daemon, &exit_code, 0) != daemon)
        return -errno;

    if (WIFEXITED(exit_code))
	return WEXITSTATUS(exit_code);

    return exit_code;
}

