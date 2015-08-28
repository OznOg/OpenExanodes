/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_NETWORK_H
#define _OS_NETWORK_H

#include "os/include/os_inttypes.h"

/* Note : since all net/socket function are not still wrapped we need to include
   standard headers */

#ifdef WIN32

#include "os/include/os_windows.h"

/* Socket shutdown parameter */
#define SHUT_RDWR SD_BOTH
#define SHUT_RD   SD_SEND
#define SHUT_WR   SD_RECEIVE

/* Internet address.  */
typedef unsigned int in_addr_t;

#else /* !WIN32 */

#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

typedef int socket_t;

#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Length of a string containing a human readable IPv4 address
 * 12 digits + 3 dots
 */
#define OS_NET_ADDR_STR_LEN 15

typedef char os_net_addr_str_t[OS_NET_ADDR_STR_LEN + 1];

/**
 * Initialise network
 *
 * *Must* be called before any net function
 *
 * FIXME Should return a *negative* error code instead of a positive one.
 *
 * @return 0 if successfull, non zero value otherwise
 */
int os_net_init(void);

/**
 * Cleanup network
 *
 * @return 0 if successfull, non zero value otherwise
 */
int os_net_cleanup(void);

/**
 * Check whether an IP is valid.
 *
 * An IP is considered valid if it is made up of 4 integers in the 0..255
 * range, each separated from the next by a dot: xxx.xxx.xxx.xxx
 *
 * @param[in] ip  Null-terminated IP string
 *
 * @return true if IP is valid, false otherwise
 */
bool os_net_ip_is_valid(const os_net_addr_str_t ip);

/**
 * Create a socket
 *
 * @param af        The address family
 * @param type      The type of socket (connected, datagram, ...)
 * @param protocol  The protocol to be used
 *
 * @return file descriptor of the socket if successfull, negative error code otherwise
 *
 * @os_replace{Linux, socket}
 * @os_replace{Windows, socket}
 */
int os_socket(int af, int type, int protocol);

/**
 * Accept connections on a given socket
 *
 * @param s             The listening socket
 * @param[out] addr     The address of the connecting client
 * @param[out] addrlen  The length of the preceding parameter
 *
 * @return file descriptor of the resulting socket, negative error code otherwise
 *
 * @os_replace{Linux, accept}
 * @os_replace{Windows, accept}
 */
int os_accept(int s, struct sockaddr *addr, int *addrlen);

/**
 * Connect to a server socket
 *
 * @param s        The socket
 * @param addr     The address to which connect
 * @param addrlen  The length of the preceding parameter

 * @return 0 if successfull, negative error code otherwise
 *
 * @os_replace{Linux, connect}
 * @os_replace{Windows, connect}
 */
int os_connect(int s, struct sockaddr *addr, int addrlen);

/**
 * Associate a local address with a socket
 *
 * @param s        The socket
 * @param addr     Address to assign to the socket
 * @param addrlen  The length of the preceding parameter
 *
 * @return 0 if successfull, negative error code otherwise
 *
 * @os_replace{Linux, bind}
 * @os_replace{Windows, bind}
 */
int os_bind(int s, const struct sockaddr *name, int namelen);

/**
 * Retrieves the address of the peer to which a socket is connected
 *
 * @param s             The socket
 * @param[out] name     Address of the peer
 * @param[out] namelen  The length of the preceding parameter
 *
 * @return 0 if successfull, negative error code otherwise
 *
 * @os_replace{Linux, getpeername}
 * @os_replace{Windows, getpeername}
 */
int os_getpeername(int s, struct sockaddr *name, int *namelen);

/**
 * Listen to incoming connections
 *
 * @param s        The socket
 * @param backlog  The maximum length of the queue of pending connections
 *
 * @return 0 if successfull, negative error code otherwise
 *
 * @os_replace{Linux, listen}
 * @os_replace{Windows, listen}
 */
int os_listen(int s, int backlog);

/**
 * Monitor given file descriptors, possibly waiting until one or more of them
 * becomes "ready" for some class of I/O operation
 *
 * @param nfds
 * @param[in,out] readfds    Set of sockets to be checked for readability
 * @param[in,out] writefds   Set of sockets to be checked for writability
 * @param[in,out] exceptfds  Set of sockets to be checked for errors
 * @param timeout            Maximum time for select to wait
 *                           CAREFUL: timeout is NOT updated (remains const)
 *
 * @return The number of ready file descriptors if any, negative error code otherwise
 *
 * @os_replace{Linux, select}
 * @os_replace{Windows, select}
 */
int os_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	      const struct timeval *timeout);

/**
 * Get the options of a given socket
 *
 * @param s            The socket
 * @param level        The level at which the option is defined
 * @param optname      The socket option for which the value is to be retrieved
 * @param[out] optval  A pointer to the buffer in which the value for
 *                     the requested option is to be returned
 * @param optlen       The size of the preceding parameter
 *
 * @return 0 if successfull, negative error code otherwise
 *
 * @os_replace{Linux, getsockopt}
 * @os_replace{Windows, getsockopt}
 */
int os_getsockopt(int s, int level, int optname, void *optval, int *optlen);

/**
 * Set the options of a given socket
 *
 * @param s        The socket
 * @param level    The level at which the option is defined
 * @param optname  The socket option for which the value is to be set
 * @param optval   A pointer to the buffer in which the value for the requested
 *                 option is specified
 * @param optlen   The size of the preceding parameter
 *
 * @return 0 if successfull, negative error code otherwise
 *
 * @os_replace{Linux, setsockopt}
 * @os_replace{Windows, setsockopt}
 */
int os_setsockopt(int s, int level, int optname, const void *optval, int optlen);

/**
 * Send data over network using given connected socket
 *
 * @param socket  The socket
 * @param buffer  The buffer containg data to be sent
 * @param length  The length of the buffer
 *
 * @return Number of bytes sent if successfull, negative error code otherwise
 *
 * @os_replace{Linux, send}
 * @os_replace{Windows, send}
 */
int os_send(int socket, const void *buffer, int length);

/**
 * Receive data from network using given socket
 *
 * @param socket       The socket
 * @param[out] buffer  The buffer to receive incoming data
 * @param length       The length of the buffer
 * @param flags        Flags to specify the behavior of the function invocation
 *                     beyond the options specified for the associated socket
 *
 * @return Number of bytes received if successfull, negative error code otherwise
 *
 * @os_replace{Linux, recv}
 * @os_replace{Windows, recv}
 */
int os_recv(int socket, void *buffer, int length, int flags);

/**
 * Send data to a specific destination
 *
 * @param s      The socket
 * @param buf    The buffer containg data to be sent
 * @param len    The length of the buffer
 * @param flags  Flags that specify the way in which the call is made
 * @param to     Address of the target socket
 * @param tolen  The size of the preceding parameter
 *
 * @return Number of bytes sent if successfull, negative error code otherwise
 *
 * @os_replace{Linux, sendto}
 * @os_replace{Windows, sendto}
 */
int os_sendto(int s, const void *buf, int len, int flags,
	      const struct sockaddr * to, int tolen);

/**
 * Receive data and store the source address
 *
 * @param s             The socket
 * @param[out] buf      The buffer to receive incoming data
 * @param len           The length of the buffer
 * @param flags         Flags to specify the behavior of the function invocation
 *                      beyond the options specified for the associated socket
 * @param[out] from     Address of the source socket
 * @param[out] fromlen  The size of the preceding parameter
 *
 * @return Number of bytes received if successfull, negative error code otherwise
 *
 * @os_replace{Linux, recvfrom}
 * @os_replace{Windows, recvfrom}
 */
int os_recvfrom(int s, char *buf, int len, int flags,
		struct sockaddr *from, unsigned int *fromlen);

/**
 * Disables sends or receives on a socket
 *
 * @param sock  The socket
 * @param how   Types of operation that will no longer be allowed
 *
 * @return 0 if successfull, negative error code otherwise
 *
 * @os_replace{Linux, shutdown}
 * @os_replace{Windows, shutdown}
 */
int os_shutdown(int sock, int how);

/**
 * Close a socket
 *
 * Win32 sockets *Must* be closed with closesocket
 *
 * @param socketdesc  The socket to close
 *
 * @return 0 if successfull, negative error code otherwise
 *
 * @os_replace{Linux, close}
 * @os_replace{Windows, closesocket}
 */
int os_closesocket(int socketdesc);

/**
 * Convert IP address into binary
 *
 * @param cp        IP address
 * @param[out] pin  Structure to store the binary value
 *
 * @return Non zero value if successfull, 0 otherwise
 *
 * @os_replace{Linux, inet_aton}
 * @os_replace{Windows, inet_addr}
 */
int os_inet_aton(const char *cp, struct in_addr *pin);

/**
 * Converts the Internet host address in, given in network byte order,
 * to a string in IPv4 dotted-decimal notation. The string is returned
 * in a statically allocated buffer, which subsequent calls will overwrite.
 *
 * @param[in] in  IP address in network byte order
 *
 * @return pointer to a statically allocated string containing the
 *         dotted-decimal notation of the address.
 *
 * @os_replace{Linux, inet_ntoa}
 * @os_replace{Windows, inet_ntoa}
 */
char *os_inet_ntoa(struct in_addr in);

/**
 * Get the IP address of a host.
 *
 * @param[in]  hostname  Host to get the IP of
 * @param[out] addr      IP address
 *
 * @return 0 if successfull, a negative error code otherwise
 *
 * @os_replace{Linux, getaddrinfo}
 * @os_replace{Windows, getaddrinfo}
 */
int os_host_addr(const char *hostname, struct in_addr *addr);


/**
 * Helper function that is used in os_host_addr. This function returns
 * OS specific errors and SHOULD NOT be used directly.
 * FIXME maybe declaration should go into a src-common/netwok.h header.
 *
 * @param[in]  hostname  Host to get the IP of
 * @param[out] addr      IP address
 *
 * @return 0 if successfull, a OS specific error code otherwise
 */
int __os_host_addr(const char *hostname, struct in_addr *addr);

/**
 * Get the hostname of the local host.
 *
 * @param[out] hostname  Name of the host
 * @param[in]  size      Size of the name
 *
 * @return 0 if successfull, a negative error code otherwise (most notably,
 *         -ENAMETOOLONG if 'hostname' can't hold the host name)
 *
 * @os_replace{Linux, uname, gethostname}
 * @os_replace{Windows, GetComputerName, GetComputerNameEx, gethostname}
 */
int os_local_host_name(char *hostname, size_t size);

/**
 * Get the canonical hostname of a host.
 *
 * @param[in]  hostname            Host to get the canonical name of
 * @param[out] canonical_hostname  Resulting canonical name
 * @param[in]  size                Size of the canonical_hostname
 *                                 buffer given in input.
 *
 * @return 0 if successful, non-zero otherwise
 *
 * @os_replace{Linux, getaddrinfo}
 * @os_replace{Windows, getaddrinfo}
 */
int os_host_canonical_name(const char *hostname,
	                    char *canonical_hostname, size_t size);

#ifdef WIN32
typedef INTERFACE_INFO os_iface;
#else
typedef struct ifreq os_iface;
#endif

/**
 * Find the interface associated with the specified address
 * and get its flags.
 *
 * @param      sockfd  Socket
 * @param[in]  addr    Address to find the interface of
 * @param[out] iface   Pointer to an interface buffer
 *
 * @return 0 if interface found, negative error code otherwise
 */
int os_find_iface_with_addr(int sockfd, const struct in_addr *addr,
                            os_iface *iface);

/**
 * Find the name associated with an interface
 * FIXME WIN32: in windows it returns the IP address.
 *
 * @param[in] iface Interface
 *
 * @return name of the interface
 */
const char *os_iface_name(const os_iface *iface);

/**
 * Find the address associated with an interface
 *
 * @param[in] iface Interface
 *
 * @return address of the interface
 */
const struct sockaddr_in *os_iface_addr(const os_iface *iface);

/**
 * Get flags for an interface
 *
 * @param[in]  iface   Interface
 *
 * @return flags
 */
int os_iface_flags(const os_iface *iface);

/**
 * Set an interface flag
 *
 * @param     sockfd  Socket
 * @param[in] iface   Interface
 * @param[in] flag    Flag to set (eg. IFF_UP, IFF_LOOPBACK, IFF_MULTICAST)
 *
 * @return 0 if success, a negative error code otherwise.
 */
int os_iface_set_flag(int sockfd, os_iface *iface, int flag);

/**
 * Set send and receive timeouts on a socket
 *
 * @param     sockfd    Socket
 * @param[in] tm_msec    Timeout value, in milliseconds
 *
 * @return 0 if success, a negative error code otherwise.
 */
int os_sock_set_timeouts(int sockfd, int tm_msec);

/**
 * Get the list of all interfaces.
 *
 * @param[in]       fd          A socket descriptor
 * @param[in,out]   ifaces      A table of os_iface
 * @param[in]       max_ifaces  The table size
 *
 * @return the number of found interfaces if successful,
 * a negative error code otherwise.
 */
int os_iface_get_all(int fd, os_iface ifaces[], int max_ifaces);

#ifdef __cplusplus
}
#endif

#endif
