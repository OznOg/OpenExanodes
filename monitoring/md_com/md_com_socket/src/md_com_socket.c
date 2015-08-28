/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "monitoring/md_com/include/md_com.h"
#include "os/include/os_thread.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>


#define MAX_CONNECTIONS 256
os_thread_mutex_t send_lock = PTHREAD_MUTEX_INITIALIZER;




md_com_error_code_t md_com_listen(const void *arg, int* connection_id)
{
    int fd;
    socklen_t len;
    struct sockaddr_un local;
    const char* socket_path = arg;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return COM_UNKNOWN_ERROR;
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, socket_path);
    unlink(local.sun_path);

    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(fd, (struct sockaddr *)&local, len) == -1) {
        return COM_UNKNOWN_ERROR;
    }

    if (listen(fd, MAX_CONNECTIONS) == -1)
        return COM_TOO_MANY_CONNECTIONS;

    *connection_id = fd;

    return COM_SUCCESS;
}




md_com_error_code_t md_com_accept(int server_connection_id, int* client_connection_id)
{
    socklen_t t ;
    int remote_fd;
    struct sockaddr_un remote;

    t = sizeof(remote);

    /* blocking */
    remote_fd = accept(server_connection_id,
		       (struct sockaddr *)&remote, &t);
    if (remote_fd == -1)
    {
	if (errno == EINVAL)
	    return COM_CONNECTION_CLOSED;

        return COM_UNKNOWN_ERROR;
    }

    *client_connection_id = remote_fd;

    return COM_SUCCESS;
}






md_com_error_code_t md_com_recv_msg(int connection_id, md_com_msg_t* rx_msg)
{
    int ret, expected_size;
    expected_size = sizeof(rx_msg->size);
    ret = recv(connection_id, &rx_msg->size, expected_size, MSG_WAITALL);
    if (ret != expected_size)
	goto rx_error;

    expected_size = sizeof(rx_msg->type);
    ret = recv(connection_id, &rx_msg->type, expected_size, MSG_WAITALL);
    if (ret != expected_size)
	goto rx_error;

    rx_msg->payload = malloc(rx_msg->size);
    assert(rx_msg->payload != NULL);

    expected_size = rx_msg->size;
    if (expected_size > 0)
    {
	ret = recv(connection_id, rx_msg->payload, expected_size, MSG_WAITALL);
	if (ret != expected_size)
	    goto rx_error;
    }

    return COM_SUCCESS;

rx_error:
    if (ret == 0)
	return COM_CONNECTION_CLOSED;
    return COM_READ_ERROR;
}



md_com_error_code_t md_com_connect(const void *arg, int* connection_id)
{
    int fd, len;
    struct sockaddr_un remotesock;
    const char* socket_path = arg;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        return COM_UNKNOWN_ERROR;

    remotesock.sun_family = AF_UNIX;
    strcpy(remotesock.sun_path, socket_path);
    len = strlen(remotesock.sun_path) + sizeof(remotesock.sun_family);

    if (connect(fd, (struct sockaddr *)&remotesock, len) == -1)
        return COM_UNKNOWN_ERROR;

    *connection_id = fd;
    return COM_SUCCESS;
}



md_com_error_code_t md_com_send_msg(int connection_id, const md_com_msg_t* tx_msg)
{
    int ret;
    int total_size = sizeof(tx_msg->size) +
	sizeof(tx_msg->type) + tx_msg->size;

    char* buffer = (char*) malloc(total_size);

    memcpy(buffer, &tx_msg->size, sizeof(tx_msg->size));
    memcpy(buffer + sizeof(tx_msg->size), &tx_msg->type, sizeof(tx_msg->type));
    memcpy(buffer + sizeof(tx_msg->size) + sizeof(tx_msg->type),
	   tx_msg->payload, tx_msg->size);

    os_thread_mutex_lock(&send_lock);
    ret = send(connection_id, buffer, total_size, 0);

    os_thread_mutex_unlock(&send_lock);
    free(buffer);
    if (ret != total_size)
	goto tx_error;

    return COM_SUCCESS;

tx_error:
    return (errno == EPIPE) ? COM_CONNECTION_CLOSED : COM_WRITE_ERROR;
}



md_com_error_code_t md_com_close(int connection_id)
{
    int ret;
    os_thread_mutex_lock(&send_lock);
    ret = shutdown(connection_id, SHUT_RDWR);
    ret = close(connection_id);
    os_thread_mutex_unlock(&send_lock);
    if (ret != 0)
	return COM_UNKNOWN_ERROR;
    return COM_SUCCESS;
}


