/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/** \file iface.c
 * \brief Network interfaces manipulation functions
 *
 * This file implements interface manipulation routines for the
 * cluster messaging daemon.
 * \sa examsgd.c
 */

#include "iface.h"

#include "log/include/log.h"
#include "os/include/os_error.h"
#include "os/include/os_network.h"
#include "os/include/os_time.h"
#include "os/include/strlcpy.h"


/* --- examsgIfaceConfig --------------------------------------------- */

/**
 * Configure interface for multicast.
 *
 * Use socket #sockfd to configure the interface to which the address
 * of #hostname is bound.
 *
 * \param[in]  sockfd        Socket descriptor.
 * \param[in]  hostname      Host name
 * \param[out] addr          IP address bound to interface
 *
 * return 0 on success, negative error code otherwise
 */
int
examsgIfaceConfig(int sockfd, const char *hostname, struct in_addr *addr)
{
    os_iface iface;
    int err;

    /* Get address of specified host */
    err = os_host_addr(hostname, addr);
    if (err != 0)
    {
        exalog_error("no address for '%s'", hostname);
        errno = ENONET;
        return -errno;
    }

    /* Get interface bound to address found */
    err = os_find_iface_with_addr(sockfd, addr, &iface);
    if (err != 0)
    {
        exalog_error("no interface with address %s", os_inet_ntoa(*addr));
        errno = ENONET;
        return -errno;
    }

    exalog_info("host %s: address %s, bound to interface %s", hostname,
                os_inet_ntoa(*addr), os_iface_name(&iface));

    /* check interface is up */
    if (!(os_iface_flags(&iface) & IFF_UP))
    {
        exalog_error("interface %s is not up", os_iface_name(&iface));
        errno = ENETDOWN;
        return -errno;
    }

    /* check interface is not a loopback */
    if (os_iface_flags(&iface) & IFF_LOOPBACK)
    {
        exalog_error("interface %s is a loopback", os_iface_name(&iface));
        errno = EINVAL;
        return -errno;
    }

    /* check interface supports multicast */
    if (os_iface_set_flag(sockfd, &iface, IFF_MULTICAST) != 0)
    {
        exalog_error("interface %s does not support multicast", os_iface_name(&iface));
        errno = ENETUNREACH;
        return -errno;
    }

    return 0;
}
