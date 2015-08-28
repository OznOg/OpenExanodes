/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "config/pid_dir.h"
#include "os/include/os_syslog.h"

#include "os/include/os_assert.h"
#include "os/include/os_dir.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_daemon_parent.h"
#include "os/include/os_daemon_lock.h"
#include "os/include/strlcpy.h"

#define DAEMON_NAME_LEN 63

struct os_daemon_lock
{
    char daemon_name[DAEMON_NAME_LEN + 1];
    int fd;
};

/* Return the directory where pid files are stored.
 *
 * NOTE: Not meant to be exposed to end users. Meant to ease unit testing.
 *
 * NOTE: Caches the value returned the first time it's called (and thus may
 * cache the default value in case the environment variable is not set).
 */
static const char *pidfile_dir(void)
{
    static const char *dir = NULL;

    if (dir == NULL)
    {
        dir = getenv("OS_DAEMON_LOCK_DIR");
        if (dir == NULL || dir[0] == '\0')
            dir = "/var/run";
    }

    return dir;
}

static void make_pidfile_path(const char *daemon_name, char *path, size_t size)
{
    os_snprintf(path, size, "%s" OS_FILE_SEP "%s.pid", pidfile_dir(), daemon_name);
}

static bool lock_file(int fd)
{
    return lockf(fd, F_TLOCK, 0) == 0;
}

static bool unlock_file(int fd)
{
    return lockf(fd, F_ULOCK, 0) == 0;
}

int os_daemon_lock(const char *daemon_name, os_daemon_lock_t **_daemon_lock)
{
    const char *dir;
    char path[OS_PATH_MAX];
    os_daemon_lock_t *daemon_lock = NULL;
    int err;
    uint32_t pid;
    char pid_str[64]; /* big enough */
    ssize_t written;

    if (daemon_name == NULL || _daemon_lock == NULL)
        return -EINVAL;

    daemon_lock = malloc(sizeof(os_daemon_lock_t));
    if (daemon_lock == NULL)
        return -ENOMEM;

    dir = pidfile_dir();
    err = os_dir_create_recursive(dir);
    if (err)
    {
        os_syslog(OS_SYSLOG_ERROR, "Failed creating pid directory '%s': %s",
                  dir, os_strerror(-err));
        return err;
    }

    make_pidfile_path(daemon_name, path, sizeof(path));

    daemon_lock->fd = open(path, O_RDWR | O_CREAT, 0640);
    if (daemon_lock->fd < 0)
    {
        err = -errno;
        os_syslog(OS_SYSLOG_ERROR, "Cannot open pid file '%s': %s",
                  path, os_strerror(-err));
	free(daemon_lock);
        return err;
    }

    if (!lock_file(daemon_lock->fd))
    {
        close(daemon_lock->fd);
        free(daemon_lock);
        return -EEXIST;
    }

    pid = os_daemon_current_pid();

    /* pid is written on a fixed size length in order to make sure that any
     * previous value is overwritten. */
    os_snprintf(pid_str, sizeof(pid_str), "%-32"PRIu32"\n", pid);

    written = write(daemon_lock->fd, pid_str, strlen(pid_str));
    if (written < strlen(pid_str))
    {
        err = -errno;
        unlock_file(daemon_lock->fd);
        close(daemon_lock->fd);
        return err;
    }
    fsync(daemon_lock->fd);

    strlcpy(daemon_lock->daemon_name, daemon_name, sizeof(os_daemon_lock_t));

    *_daemon_lock = daemon_lock;

    return 0;
}

void os_daemon_unlock(os_daemon_lock_t *daemon_lock)
{
    char path[OS_PATH_MAX];

    if (daemon_lock == NULL)
        return;

    OS_ASSERT(daemon_lock->fd >= 0);

    unlock_file(daemon_lock->fd);
    close(daemon_lock->fd);

    make_pidfile_path(daemon_lock->daemon_name, path, sizeof(path));
    unlink(path);

    free(daemon_lock);
}
