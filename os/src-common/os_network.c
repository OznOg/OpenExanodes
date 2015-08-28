/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_error.h"
#include "os/include/os_network.h"
#include "os/include/os_assert.h"
#include <stdlib.h>
#include <string.h>

bool os_net_ip_is_valid(const os_net_addr_str_t ip)
{
    const char *c;
    int i = 0;
    unsigned int value = 0;
    int nb_parts = 0;

    if (ip == NULL)
        return false;

    for (c = ip; *c; ++c)
    {
        if (*c >= '0' && *c <= '9')
        {
            if (i == 3)
                return false;

            value = value * 10 + (*c - '0');
            i++;
        }
        else if (*c == '.')
        {
            if (value > 255)
                return false;

            ++nb_parts;
            value = 0;
            i = 0;
        }
        else
            return false;
    }

    if (i == 0)
        return false;

    if (value > 255)
        return false;

    if (nb_parts != 3)
	return false;

    return true;
}

int os_sock_set_timeouts(int sockfd, int tm_msec)
{
    struct timeval timeout;
    int err;

    if (tm_msec < 0)
        return -EINVAL;

    timeout.tv_sec  = tm_msec / 1000;
    timeout.tv_usec = (tm_msec % 1000) * 1000;

    err = os_setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                        sizeof(timeout));
    if (err != 0)
        return err;

    return os_setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                         sizeof(timeout));
}

int __os_host_addr(const char *hostname, struct in_addr *addr)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int ret;

    OS_ASSERT(hostname != NULL && addr != NULL);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (ret != 0)
        return ret;

    OS_ASSERT(res != NULL);
    OS_ASSERT(res->ai_addrlen == sizeof(struct sockaddr_in));
    OS_ASSERT(res->ai_addr != NULL);

    *addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
    freeaddrinfo(res);

    return 0;
}

