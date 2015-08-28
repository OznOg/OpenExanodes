/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "os/include/os_assert.h"
#include "os/include/os_random.h"

static int fdrandom = -1;

void os_random_init(void)
{
    long int buf;
    int ret;

    OS_ASSERT(! os_random_is_initialized());

    fdrandom = open("/dev/urandom", O_RDONLY);
    OS_ASSERT(fdrandom >= 0);

    ret = read(fdrandom, &buf, sizeof(buf));
    OS_ASSERT(ret == sizeof(buf));
}

void os_random_cleanup(void)
{
    OS_ASSERT(os_random_is_initialized());
    close(fdrandom);
    fdrandom = -1;
}

bool os_random_is_initialized(void)
{
    return (fdrandom >= 0);
}

void os_get_random_bytes(void *buf, size_t len)
{
    ssize_t ret;

    OS_ASSERT(os_random_is_initialized());
    ret = read(fdrandom, buf, len);
    OS_ASSERT(ret >= 0 && (size_t)ret == len);
}

double os_drand(void)
{
    uint32_t val;

    OS_ASSERT(os_random_is_initialized());
    os_get_random_bytes(&val, sizeof(val));

    return (double)val / (double)UINT32_MAX;
}
