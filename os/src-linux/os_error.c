/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>
#include <netdb.h>

#include "../include/os_error.h"


int os_error_from_gai(int ret, int _errno)
{
    if (ret == 0)
        return 0;

    if (ret == EAI_SYSTEM)
        return _errno;

    /* getaddrinfo() error codes are negative. */
    return (-ret) | OS_ERROR_GAI_BIT;
}

const char *os_strerror(int error)
{
    if (error & OS_ERROR_GAI_BIT)
        return gai_strerror(-(error & ~OS_ERROR_GAI_BIT));

    return strerror(error);
}
