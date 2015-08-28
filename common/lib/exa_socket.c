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


/**
 * Change the size (in KB) added to emergency pool for exanodes (NBD/TCP)
 * This pool is used by GFP_ATOMIC allocation (used in NBD netplugin tcp).
 * Notice: to call to this function will not increase two time the emergency pool size
 * but only change the size added, so call exa_emergency_pool_add_size(0)
 * will set the pool size to original size.
 * For example if the initial pool was 10
 * exa_emergency_pool_add_size(10) -> pool size 20
 * exa_emergency_pool_add_size(10) -> pool size 20
 * exa_emergency_pool_add_size(5)  -> pool size 15
 * exa_emergency_pool_add_size(15) -> pool size 25
 * exa_emergency_pool_add_size(10) -> pool size 10
 *
 */

int exa_socket_tweak_emergency_pool(int size)
{
    int fd, ret;

    fd = open(EXACOMMON_MODULE_PATH, O_RDWR);
    if (fd < 0)
        return -EXA_ERR_MODULE_OPEN;

    ret = ioctl(fd, EXA_EMERGENCY_SIZE, size);

    close(fd);

    return ret;
}
