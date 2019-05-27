/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_network.h"
#include "os/include/os_error.h"
#include "os/include/os_stdio.h"
#include "os/include/strlcpy.h"
#include "os/include/os_time.h"
#include "common/include/exa_constants.h"

#include <unit_testing.h>

#include <ctype.h>
#include <string.h>

/* The DATA_PORT is chose with a different value from the exanodes one.
 * This is done on purpose as the unit test should success even if exanodes'
 * service is running. But the 'reuse' flag on sockets has unexpected side
 * effects when both exanodes and unit test a trying to use the same port,
 * making the unit test deadlock. */
#define DATA_PORT 31796

#define BACKLOG 5
#define BUFFER_SIZE 27

static int server_sock;
static int client_sock;
static int accept_sock;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;

UT_SECTION(init_cleanup)

ut_test(init)
{
    if (os_net_init())
	UT_FAIL();
}

ut_test(cleanup)
{
    if (os_net_cleanup())
	UT_FAIL();
}

UT_SECTION(ip_valid)

ut_test(null_string_is_not_a_valid_ip)
{
    UT_ASSERT(!os_net_ip_is_valid(NULL));
}

ut_test(empty_string_is_not_a_valid_ip)
{
    UT_ASSERT(!os_net_ip_is_valid(""));
}

ut_test(non_numeric_string_is_not_a_valid_ip)
{
    UT_ASSERT(!os_net_ip_is_valid("s"));
    UT_ASSERT(!os_net_ip_is_valid("aaa.bbb.ccc.ddd"));
}

ut_test(ip_with_spaces_is_not_valid)
{
    UT_ASSERT(!os_net_ip_is_valid(" 125.25.32.36"));
    UT_ASSERT(!os_net_ip_is_valid(" 125.25.32.36 "));
    UT_ASSERT(!os_net_ip_is_valid("125.2 5.32.36"));
}

ut_test(ip_with_wrong_number_of_numbers_is_not_valid)
{
    UT_ASSERT(!os_net_ip_is_valid("..."));
    UT_ASSERT(!os_net_ip_is_valid("125.25.32."));
    UT_ASSERT(!os_net_ip_is_valid("125.3.2."));
    UT_ASSERT(!os_net_ip_is_valid("12"));
}

ut_test(ip_with_out_of_range_numbers_is_not_valid)
{
    UT_ASSERT(!os_net_ip_is_valid("256.0.0.0"));
    UT_ASSERT(!os_net_ip_is_valid("300.25.32.36"));
    UT_ASSERT(!os_net_ip_is_valid("256.25.32.36"));
    UT_ASSERT(!os_net_ip_is_valid("56.256.32.36"));
    UT_ASSERT(!os_net_ip_is_valid("56.56.256.36"));
    UT_ASSERT(!os_net_ip_is_valid("56.56.256.0056"));
    UT_ASSERT(!os_net_ip_is_valid("56.56.56.256"));
}

ut_test(well_formed_ips_are_valid)
{
    UT_ASSERT(os_net_ip_is_valid("125.25.32.36"));
    UT_ASSERT(os_net_ip_is_valid("0.0.0.0"));
    UT_ASSERT(os_net_ip_is_valid("255.255.255.255"));
    UT_ASSERT(os_net_ip_is_valid("192.168.8.042"));
    UT_ASSERT(os_net_ip_is_valid("12.1.9.99"));
}

UT_SECTION(socket_create)

ut_setup()
{
    UT_ASSERT(!os_net_init());
}

ut_cleanup()
{
    os_net_cleanup();
}

ut_test(inet_aton)
{
    char net_ip[27] = "127.0.0.1";
    struct in_addr addr;

    UT_ASSERT(os_inet_aton(net_ip, &addr) != 0);
    UT_ASSERT(addr.s_addr == htonl(INADDR_LOOPBACK));
}

ut_test(inet_ntoa)
{
    struct in_addr addr;
    char *net_ip;

    addr.s_addr = htonl(INADDR_LOOPBACK);
    net_ip = os_inet_ntoa(addr);
    UT_ASSERT(net_ip != NULL);
    UT_ASSERT(strcmp(net_ip, "127.0.0.1") == 0);
}

ut_test(socket)
{
    UT_ASSERT(os_socket(AF_INET, SOCK_STREAM, 0) >= 0);
}

ut_test(bind)
{
    int sock;
    struct sockaddr_in serv_addr;
    int reuse;

    sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sock >= 0);

    reuse = 1;
    UT_ASSERT(os_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			    &reuse, sizeof(reuse)) >= 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(DATA_PORT);

    UT_ASSERT(os_bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0);

    os_closesocket(sock);
}

ut_test(listen)
{
    int sock;
    struct sockaddr_in serv_addr;
    int reuse;

    sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sock >= 0);

    reuse = 1;
    UT_ASSERT(os_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			    &reuse, sizeof(reuse)) >= 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(DATA_PORT);

    UT_ASSERT(os_bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0);

    UT_ASSERT(os_listen(sock, BACKLOG) >= 0);

    os_closesocket(sock);
}

ut_test(connect_accept)
{
    int len;
    struct in_addr addr;
    int reuse;

    /* create sockets */
    server_sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(server_sock >= 0);

    reuse = 1;
    UT_ASSERT(os_setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,
			    &reuse, sizeof(reuse)) >= 0);


    client_sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(client_sock >= 0);

    /* fill in server information */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DATA_PORT);

    /* bind sockets */
    UT_ASSERT(os_bind(server_sock, (struct sockaddr *)&server_addr,
		      sizeof(server_addr)) >= 0);

    UT_ASSERT(os_listen(server_sock, BACKLOG) >= 0);

    /* connect to server */
    os_inet_aton("127.0.0.1", &addr);
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = addr.s_addr;
    client_addr.sin_port = htons(DATA_PORT);

    if (os_connect(client_sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
	UT_FAIL();

    len = sizeof(struct sockaddr_in);
    UT_ASSERT(os_accept(server_sock, (struct sockaddr *)&server_addr, &len) >= 0);

    os_closesocket(server_sock);
    os_closesocket(client_sock);
}

ut_test(setsockopt)
{
    int sock;
    int size;

    sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sock >= 0);

    size = 1024;
    UT_ASSERT(!os_setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)));

    os_closesocket(sock);
}

ut_test(get_set_sockopt_sndtimeo_works_correctly_on_w32)
{
    int sock;
    struct timeval set_time_out;
    struct timeval get_time_out;
    struct timeval diff_time_out;
    int len = sizeof(struct timeval);

    sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sock >= 0);

    UT_ASSERT_EQUAL(0, os_getsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &get_time_out, &len));
    UT_ASSERT_EQUAL(sizeof(get_time_out), len);
    UT_ASSERT_EQUAL(0, get_time_out.tv_sec);
    UT_ASSERT_EQUAL(0, get_time_out.tv_usec);
    set_time_out.tv_sec = 4;
    set_time_out.tv_usec = 10000;   /* Using 10000 here because it seems to be the minimum
                                 * resolution under Linux. */
    UT_ASSERT_EQUAL(0, os_setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &set_time_out, sizeof(set_time_out)));

    get_time_out.tv_sec = 0;
    get_time_out.tv_usec = 0;
    UT_ASSERT_EQUAL(0, os_getsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &get_time_out, &len));
    UT_ASSERT_EQUAL(sizeof(get_time_out), len);

    diff_time_out = os_timeval_diff(&get_time_out, &set_time_out);

    /* just check the diff is >= 0 as the kernel may round the value to match
     * some HZ constraints, so no mean to know the exact value we may get here. */
    UT_ASSERT(diff_time_out.tv_sec + diff_time_out.tv_usec >= 0);

    os_closesocket(sock);
}

ut_test(getsockopt)
{
    int sock;
    int size;
    int value;
    int len = sizeof(value);

    sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sock >= 0);

    size = 1024;
    UT_ASSERT(!os_setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)));

    UT_ASSERT(!os_getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &value, &len));

    /* Linux doubles the value to set */
#ifdef WIN32
    UT_ASSERT_EQUAL(value, size);
#else
    UT_ASSERT(value >= 2 * size);
#endif

    os_closesocket(sock);
}

ut_test(sock_set_timeouts_succeeds_with_valid_timeout)
{
    int sock;
    struct timeval time_out;
    int len = sizeof(time_out);

    sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sock >= 0);

    UT_ASSERT_EQUAL(0, os_sock_set_timeouts(sock, 5000));

    UT_ASSERT_EQUAL(0, os_getsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &time_out, &len));
    UT_ASSERT_EQUAL(sizeof(time_out), len);
    UT_ASSERT_EQUAL(5, time_out.tv_sec);
    UT_ASSERT_EQUAL(0, time_out.tv_usec);

    UT_ASSERT_EQUAL(0, os_getsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &time_out, &len));
    UT_ASSERT_EQUAL(sizeof(time_out), len);
    UT_ASSERT_EQUAL(5, time_out.tv_sec);
    UT_ASSERT_EQUAL(0, time_out.tv_usec);

    os_closesocket(sock);
}

ut_test(sock_set_timeouts_returns_EINVAL_with_invalid_timeout)
{
    int sock;

    sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sock >= 0);

    UT_ASSERT_EQUAL(-EINVAL, os_sock_set_timeouts(sock, -1));

    os_closesocket(sock);
}

ut_test(sock_set_timeouts_returns_EBADF_with_invalid_socket)
{
    UT_ASSERT_EQUAL(-EBADF, os_sock_set_timeouts(-1, 2000));
}

UT_SECTION(send_receive)

ut_setup()
{
    int len;
    struct in_addr addr;
    int reuse;

    UT_ASSERT(!os_net_init());

    /* create sockets */
    server_sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(server_sock >= 0);

    reuse = 1;
    UT_ASSERT(os_setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,
			    &reuse, sizeof(reuse)) >= 0);

    client_sock = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(client_sock >= 0);

    /* fill in server information */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(DATA_PORT);

    /* bind sockets */

    UT_ASSERT(os_bind(server_sock, (struct sockaddr *)&server_addr,
		      sizeof(server_addr)) >= 0);

    UT_ASSERT(os_listen(server_sock, BACKLOG) >= 0);

    /* connect to server */
    os_inet_aton("127.0.0.1", &addr);
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = addr.s_addr;
    client_addr.sin_port = htons(DATA_PORT);
    if (os_connect(client_sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
	UT_FAIL();

    len = sizeof(struct sockaddr_in);
    accept_sock = os_accept(server_sock, (struct sockaddr *)&server_addr, &len);
    UT_ASSERT(accept_sock >= 0);
}

ut_cleanup()
{
    os_closesocket(server_sock);
    os_closesocket(client_sock);
    os_closesocket(accept_sock);
    os_net_cleanup();
}


ut_test(send_receive)
{
    char send_buffer[BUFFER_SIZE];
    char receive_buffer[BUFFER_SIZE];
    int ret;
    int size;

    memset(send_buffer, 0, BUFFER_SIZE);
    strcpy(send_buffer, "test_message");

    ret = os_send(client_sock, send_buffer, BUFFER_SIZE);
    if (ret != BUFFER_SIZE)
	UT_FAIL();

    memset(receive_buffer, 0, BUFFER_SIZE);
    size = 0;
    while (size != BUFFER_SIZE)
    {
	ret = os_recv(accept_sock, receive_buffer, BUFFER_SIZE, 0);
	if (ret > 0)
	    size += ret;
    }
}

ut_test(sendto_recvfrom)
{
    char send_buffer[BUFFER_SIZE];
    char receive_buffer[BUFFER_SIZE];
    int ret;
    int size;
    unsigned int len;

    memset(send_buffer, 0, BUFFER_SIZE);
    strcpy(send_buffer, "test_message");

    ret = os_sendto(client_sock, send_buffer, BUFFER_SIZE, 0,
		    (struct sockaddr *)&server_addr, sizeof(struct sockaddr));

    if (ret != BUFFER_SIZE)
	UT_FAIL();

    memset(receive_buffer, 0, BUFFER_SIZE);
    size = 0;
    len = sizeof(struct sockaddr);
    while (size != BUFFER_SIZE)
    {
	ret = os_recvfrom(accept_sock, receive_buffer, BUFFER_SIZE, 0,
			  (struct sockaddr *)&client_addr,
			  &len);
	if (ret > 0)
	    size += ret;
    }
}

ut_test(select)
{
    char send_buffer[BUFFER_SIZE];
    char receive_buffer[BUFFER_SIZE];
    int ret;
    int size;
    fd_set receive_set;
    struct timeval timeout;
    unsigned int len;

    /* Initialize the fd_set for select */
    FD_ZERO(&receive_set);
    FD_SET(accept_sock, &receive_set);

    /* Set the timeout to 5s */
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;

    /* send buffer */
    memset(send_buffer, 0, BUFFER_SIZE);
    strcpy(send_buffer, "test_message");

    ret = os_send(client_sock, send_buffer, BUFFER_SIZE);
    if (ret != BUFFER_SIZE)
	UT_FAIL();

    /*see if data has arrived */
    ret = os_select(accept_sock + 1, NULL, &receive_set, NULL, &timeout);

    /* timeout MUST NOT be updated */
    UT_ASSERT(timeout.tv_sec == 5 && timeout.tv_usec == 0);

    if (ret < 0)
	UT_FAIL();

    if (!FD_ISSET(accept_sock, &receive_set))
	UT_FAIL();

    /*receive data */
    memset(receive_buffer, 0, BUFFER_SIZE);
    size = 0;
    len = sizeof(struct sockaddr);
    while (size != BUFFER_SIZE)
    {
	ret = os_recvfrom(accept_sock, receive_buffer, BUFFER_SIZE, 0,
			  (struct sockaddr *)&client_addr,
			  &len);
	if (ret > 0)
	    size += ret;
    }
}

ut_test(select_null_timeout)
{
    char send_buffer[BUFFER_SIZE];
    char receive_buffer[BUFFER_SIZE];
    int ret;
    int size;
    fd_set receive_set;
    unsigned int len;

    /* Initialize the fd_set for select */
    FD_ZERO(&receive_set);
    FD_SET(accept_sock, &receive_set);

    memset(send_buffer, 0, BUFFER_SIZE);
    strcpy(send_buffer, "test_message");

    /* Send buffer */
    UT_ASSERT_EQUAL(BUFFER_SIZE, os_send(client_sock, send_buffer, BUFFER_SIZE));

    /* See if data has arrived */
    UT_ASSERT(os_select(accept_sock + 1, NULL, &receive_set, NULL, NULL) >= 0);

    UT_ASSERT(FD_ISSET(accept_sock, &receive_set));

    /* Receive data */
    memset(receive_buffer, 0, BUFFER_SIZE);
    size = 0;
    len = sizeof(struct sockaddr);
    while (size != BUFFER_SIZE)
    {
	ret = os_recvfrom(accept_sock, receive_buffer, BUFFER_SIZE, 0,
			  (struct sockaddr *)&client_addr,
			  &len);
	if (ret > 0)
	    size += ret;
    }
}

ut_test(shutdown)
{
    UT_ASSERT(!os_shutdown(client_sock, SHUT_RDWR));
}

ut_test(closesocket)
{
    UT_ASSERT(!os_closesocket(client_sock));
}

UT_SECTION(local_host_name)

ut_test(local_host_name_with_NULL_buffer_returns_EINVAL)
{
    UT_ASSERT(os_local_host_name(NULL, 12) == -EINVAL);
}

ut_test(local_host_name_with_size_0_returns_EINVAL)
{
    char dummy[1024]; /* Ridiculously big; ensures a hostname fits */
    UT_ASSERT(os_local_host_name(dummy, 0) == -EINVAL);
}

ut_test(local_host_name_with_small_buffer_returns_ENAMETOOLONG)
{
    char dummy[1]; /* Ridiculously small; ensures a hostname doesn't fit */
    UT_ASSERT(os_local_host_name(dummy, sizeof(dummy)) == -ENAMETOOLONG);
}

ut_test(local_host_name_does_get_actual_hostname)
{
    char hostname_lib[EXA_MAXSIZE_HOSTNAME + 1];
    char hostname_chk[EXA_MAXSIZE_HOSTNAME + 1];
    FILE *f;
    size_t len;

    UT_ASSERT(os_local_host_name(hostname_lib, sizeof(hostname_lib)) == 0);

#ifdef WIN32
    f = popen("C:\\Windows\\System32\\hostname", "r");
#else
    f = popen("hostname", "r");
#endif

    UT_ASSERT(f != NULL);
    UT_ASSERT(fgets(hostname_chk, sizeof(hostname_chk), f) == hostname_chk);
    pclose(f);

    len = strlen(hostname_chk);
    while (len > 0 && isspace(hostname_chk[len - 1]))
    {
        hostname_chk[len - 1] = '\0';
        len--;
    }

    UT_ASSERT(strcmp(hostname_lib, hostname_chk) == 0);
}

UT_SECTION(canonical_name)

ut_test(os_host_canonical_name)
{
    char canonical[256];

    os_net_init();
    UT_ASSERT(!os_host_canonical_name("isima.fr", canonical, sizeof(canonical)));
    UT_ASSERT(!strcmp("isima.fr", canonical));
    os_net_cleanup();
}

UT_SECTION(host_addr)

ut_test(os_host_addr)
{
    struct in_addr addr;
    int ret;

    os_net_init();

    ret = os_host_addr("isima.fr", &addr);
    UT_ASSERT(ret == 0);
    UT_ASSERT_EQUAL(0x275F37C1, addr.s_addr);

    ret = os_host_addr("193.55.95.39", &addr);
    UT_ASSERT(ret == 0);
    
    UT_ASSERT_EQUAL(0x275F37C1, addr.s_addr);

    ret = os_host_addr("sjkfhqsdfhmorqze", &addr);
#ifdef WIN32
    UT_ASSERT(ret == -os_error_from_win(WSAHOST_NOT_FOUND));
#else
    /* Debian and RHEL do not return the same error */
    UT_ASSERT(ret == -os_error_from_gai(EAI_NODATA, 0) ||
              ret == -os_error_from_gai(EAI_NONAME, 0));
#endif

    os_net_cleanup();
}

UT_SECTION(iface)

ut_setup()
{
    UT_ASSERT(!os_net_init());
}

ut_cleanup()
{
    os_net_cleanup();
}

ut_test(os_find_iface_with_addr)
{
    int sockfd;
    struct in_addr addr;
    os_iface iface;

    sockfd = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sockfd >= 0);

    addr.s_addr = htonl(INADDR_LOOPBACK);
    UT_ASSERT_EQUAL(0, os_find_iface_with_addr(sockfd, &addr, &iface));
}

ut_test(os_iface_name)
{
    int sockfd;
    struct in_addr addr;
    os_iface iface;

    sockfd = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sockfd >= 0);

    addr.s_addr = htonl(INADDR_LOOPBACK);
    UT_ASSERT_EQUAL(0, os_find_iface_with_addr(sockfd, &addr, &iface));

#ifdef WIN32
    UT_ASSERT(strcmp(os_iface_name(&iface), "127.0.0.1") == 0);
#else
    UT_ASSERT(strcmp(os_iface_name(&iface), "lo") == 0);
#endif

    os_closesocket(sockfd);
}

ut_test(os_iface_flags)
{
    int sockfd;
    struct in_addr addr;
    os_iface iface;

    sockfd = os_socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(sockfd >= 0);

    addr.s_addr = htonl(INADDR_LOOPBACK);
    UT_ASSERT(os_find_iface_with_addr(sockfd, &addr, &iface) == 0);

    UT_ASSERT(os_iface_flags(&iface) && IFF_LOOPBACK);

    os_closesocket(sockfd);
}

ut_test(os_iface_get_all)
{
    int num;
    int i;
    os_iface ifaces[8];
    int sockfd = os_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    num = os_iface_get_all(sockfd, ifaces, 8);

    UT_ASSERT(num > 0);
    for (i = 0; i < num; i++)
    {
        const struct sockaddr_in *addr = os_iface_addr(&ifaces[i]);
        ut_printf("Interface %s: %s", os_iface_name(&ifaces[i]),
                                      os_inet_ntoa(addr->sin_addr));
    }
    os_closesocket(sockfd);
}
/* FIXME: how to test os_iface_set_flag() without dangerous system-wide side effects ? */
