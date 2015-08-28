/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "os/include/os_assert.h"
#include "os/include/os_system.h"
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <stdio.h>


void os_exit(int status)
{
    exit(status);
}

int os_system(char *const *command)
{
    int retval;
    pid_t pid;
    int fd;

    if (!command || !command[0])
	return -EINVAL;

    pid = fork();
    switch (pid)
    {
    case -1:
	return -errno;

    case 0: /* child */
        setsid();

        /* close std file descriptors */
        for (fd = getdtablesize() - 1; fd >= 0; fd--)
            close(fd);

        fd = open("/dev/null", O_RDWR);
        retval = dup(fd);
        if (retval == -1)
	    return -errno;

	retval = dup(fd);
        if (retval == -1)
	    return -errno;

        retval = execvp(command[0], command);
        exit(retval == -1 ? errno : 0);

    default: /* parent */
        pid = waitpid(pid, &retval, 0);
    }

    if (-1 == pid) /* waitpid failed */
	return -errno;

    OS_ASSERT_VERBOSE((WIFEXITED(retval)),
                      "incoherent return from waitpid %d %d", pid, retval);
    retval = WEXITSTATUS(retval);

    return -retval;
}
