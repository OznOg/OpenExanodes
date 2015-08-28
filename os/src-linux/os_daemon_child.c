/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_daemon_child.h"
#include "os/src-linux/os_daemon_common.h"
#include "os/include/os_assert.h"
#include "os/include/os_error.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <errno.h>

static bool send_status(const char *daemon_name, int status)
{
    int r;

    r = write(DAEMON_CHILD_STATUS_FD, &status, sizeof(status));
    close(DAEMON_CHILD_STATUS_FD);

    if (r < 0)
    {
	printf("%s: Can't reply to parent process: %s (%d)."
		" This program is not meant to be run standalone.\n",
		daemon_name, os_strerror(errno), errno);
	return false;
    }

    return true;
}

/* FIXME Should be plugged to signals (at least) somehow */
bool daemon_must_quit(void)
{
    return false;
}

int main(int argc, char *argv[])
{
    char *daemon_name = argv[0];
    int status;

    status = daemon_init(argc, argv);
    OS_ASSERT(status <= 0);

    /* Send the initialization status to the parent process, regardless
       of the initialization outcome. */
    if (!send_status(daemon_name, status))
        exit(1);

    /* Exit with error if the initialization failed. */
    if (status != 0)
        exit(1);

    return daemon_main();
}
