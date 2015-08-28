/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_network.h"
#include "os/include/os_error.h"
#include "os/include/os_assert.h"
#include "os/include/os_time.h"
#include "os/include/strlcpy.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

int os_net_init(void)
{
    return 0;
}

int os_net_cleanup(void)
{
    return 0;
}

int os_socket(int af, int type, int protocol)
{
    int sock;

    sock = socket(af, type, protocol);
    if (sock == -1)
	sock = -errno;

    return sock;
}

int os_accept(int s, struct sockaddr *addr, int *addrlen)
{
    int retval;

    retval = accept(s, addr, (socklen_t *)addrlen);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_connect(int s, struct sockaddr *addr, int addrlen)
{
    int retval;

    retval = connect(s, addr, addrlen);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_bind(int s, const struct sockaddr *name, int namelen)
{
    int retval;

    retval = bind(s, name, namelen);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_getpeername(int s, struct sockaddr *name, int *namelen)
{
    int retval;

    retval =  getpeername(s, name, (socklen_t *) namelen);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_listen(int s, int backlog)
{
    int retval;

    retval = listen(s, backlog);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	      const struct timeval *_timeout)
{
    struct timeval timeout;
    int retval;

    if (_timeout)
    {
        OS_ASSERT(TIMEVAL_IS_VALID(_timeout));
        timeout.tv_usec = _timeout->tv_usec;
        timeout.tv_sec  = _timeout->tv_sec;
    }

    retval = select(nfds, readfds, writefds, exceptfds,
		    _timeout ? &timeout : NULL);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_getsockopt(int s, int level, int optname, void *optval, int *optlen)
{
    int retval;

    retval = getsockopt(s, level, optname, optval, (socklen_t *)optlen);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_setsockopt(int s, int level, int optname, const void *optval, int optlen)
{
    int retval;

    retval =  setsockopt(s, level, optname, optval, (socklen_t)optlen);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_send(int socket, const void *buffer, int length)
{
    int retval;

    retval = send(socket, buffer, length, 0);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_recv(int socket, void *buffer, int length, int flags)
{
    int retval;

    retval = recv(socket, buffer, length, flags);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_sendto(int s, const void *buf, int len, int flags,
	      const struct sockaddr *to, int tolen)
{
    int retval;

    retval = sendto(s, buf, len, flags, to, tolen);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_recvfrom(int s, char *buf, int len, int flags,
		struct sockaddr *from, unsigned int *fromlen)
{
    int retval;

    retval = recvfrom(s, buf, len, flags, from, fromlen);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_closesocket(int socketdesc)
{
    int retval;

    retval = close(socketdesc);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_shutdown(int sock, int how)
{
    int retval;

    retval = shutdown(sock, how);
    if (retval == -1)
	retval = -errno;

    return retval;
}

int os_inet_aton(const char *cp, struct in_addr *pin)
{
    return inet_aton(cp, pin);
}

char *os_inet_ntoa(struct in_addr in)
{
    return inet_ntoa(in);
}

int os_host_addr(const char *hostname, struct in_addr *addr)
{
    int ret = __os_host_addr(hostname, addr);
    if (ret != 0)
        return -os_error_from_gai(ret, errno);

    return 0;
}

int os_local_host_name(char *hostname, size_t size)
{
    static struct utsname host;

    if (hostname == NULL || size == 0)
        return -EINVAL;

    if (uname(&host) != 0)
        return -errno;

    if (strlcpy(hostname, host.nodename, size) >= size)
        return -ENAMETOOLONG;

    return 0;
}

int os_host_canonical_name(const char *hostname, char *canonical, size_t size)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int err;

    OS_ASSERT(hostname != NULL && canonical != NULL);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_CANONNAME;

    err = (getaddrinfo(hostname, NULL, &hints, &res) != 0
	    || res == NULL || res->ai_canonname == NULL);

    if (!err)
	strlcpy(canonical, res->ai_canonname, size);

    if (res != NULL)
	freeaddrinfo(res);

    return err;
}

int os_find_iface_with_addr(int sockfd, const struct in_addr *addr,
                            os_iface *iface)
{
/* Search at most this many interfaces */
#define MAX_IFACES 8
    os_iface iftab[MAX_IFACES];
    int i;
    int num;

    num = os_iface_get_all(sockfd, iftab, MAX_IFACES);

    for (i = 0; i < num; i++)
    {
        os_iface *itf = &iftab[i];
        const struct sockaddr_in *a = os_iface_addr(itf);

        if (memcmp(&a->sin_addr, addr, sizeof(*addr)) == 0)
        {
            memcpy(iface, itf, sizeof(struct ifreq));
            /* Get the flags */
            if (ioctl(sockfd, SIOCGIFFLAGS, iface) != 0)
                return -errno;
            return 0;
        }
    }

    return -ENOENT;
}

const char *os_iface_name(const os_iface *iface)
{
    return iface->ifr_name;
}

const struct sockaddr_in *os_iface_addr(const os_iface *iface)
{
    return (struct sockaddr_in *)&iface->ifr_addr;
}

int os_iface_flags(const os_iface *iface)
{
    return iface->ifr_flags;
}

int os_iface_set_flag(int sockfd, os_iface *iface, int flag)
{
    int retval;

    if (iface->ifr_flags & flag)
        return 0;

    iface->ifr_flags |= flag;
    retval = ioctl(sockfd, SIOCSIFFLAGS, &iface);
    if (retval < 0)
	retval = -errno;

    return retval;
}

int os_iface_get_all(int fd, os_iface ifaces[], int max_ifaces)
{
    int i;
    int err;
    struct ifreq iftab[MAX_IFACES];
    struct ifconf ifconf;

    ifconf.ifc_len = sizeof(iftab);
    ifconf.ifc_buf = (char *)iftab;

    err = ioctl(fd, SIOCGIFCONF, &ifconf);
    if (err != 0)
        return -errno;

    for(i = 0; i < max_ifaces && i < ifconf.ifc_len / sizeof(struct ifreq); i++)
    {
        const struct ifreq *ifr = &ifconf.ifc_req[i];
        memcpy(&ifaces[i], ifr, sizeof(struct ifreq));
    }

    return i;
}
