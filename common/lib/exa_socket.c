/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common/include/exa_error.h"
#include "common/include/exa_socket.h"
#include "common/lib/exa_common_kernel.h"


/**
 * Set a socket kernel allocation to GFP_NOIO
 */
int exa_socket_set_atomic(int socket_id)
{
    int fd, ret;

    fd = open(EXACOMMON_MODULE_PATH, O_RDWR);
    if (fd < 0)
        return -EXA_ERR_MODULE_OPEN;

    ret = ioctl(fd, EXA_NOIO, socket_id);

    close(fd);

    return ret;
}

