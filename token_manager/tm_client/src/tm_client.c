/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "token_manager/tm_client/include/tm_client.h"
#include "token_manager/tm_server/include/token_msg.h"

#include "os/include/os_assert.h"
#include "os/include/os_error.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"
#include "os/include/os_network.h"

struct token_manager {
    char *address;
    uint16_t port;
    int sock;
};

bool tm_is_connected(const token_manager_t *tm)
{
    OS_ASSERT(tm != NULL);

    return tm->sock != -1;
}

static int __send_request(token_manager_t *tm, token_op_t op,
                          const exa_uuid_t *uuid, exa_nodeid_t sender_id)
{
    token_request_msg_t req;
    size_t sent;

    if (tm == NULL || tm->sock < 0)
        return -EINVAL;

    if (uuid == NULL || uuid_is_zero(uuid))
        return -EINVAL;

    if (!EXA_NODEID_VALID(sender_id) && sender_id != EXA_NODEID_NONE)
        return -EINVAL;

    /* The sender id is not used when forcefully releasing a token. This is
       the only case where sender may be EXA_NODEID_NONE. */
    if (op != TOKEN_OP_FORCE_RELEASE && sender_id == EXA_NODEID_NONE)
        return -EINVAL;

    req.op = op;
    uuid_copy(&req.cluster_uuid, uuid);
    req.sender_id = sender_id;

    sent = 0;
    while (sent < sizeof(req))
    {
        int r = os_send(tm->sock, (char *)&req + sent, sizeof(req) - sent);

        if (r < 0)
        {
            if (r != -EINTR)
                return r;
        }
        else
            sent += r;
    }

    return 0;
}

static int __receive_reply(token_manager_t *tm)
{
    token_reply_msg_t reply;
    size_t received;

    if (tm == NULL || tm->sock < 0)
        return -EINVAL;

    received = 0;
    while (received < sizeof(reply))
    {
        int r = os_recv(tm->sock, (char *)&reply + received,
                        sizeof(reply) - received, 0);
        if (r < 0)
        {
            if (r != -EINTR)
                return r;
        }
        else if (r == 0)
            return -ECONNRESET;
        else
            received += r;
    }

    OS_ASSERT(TOKEN_RESULT_IS_VALID(reply.result));

    if (reply.result == TOKEN_RESULT_ACCEPTED)
        return 0;

    return -ENOENT; /* FIXME Define proper error code */
}

int tm_init(token_manager_t **tm, const char *ip_addr, uint16_t port)
{
    if (tm == NULL)
        return -EINVAL;

    if (!os_net_ip_is_valid(ip_addr))
        return -EINVAL;

    *tm = os_malloc(sizeof(token_manager_t));
    if (*tm == NULL)
        return -ENOMEM;

    (*tm)->address = os_strdup(ip_addr);
    (*tm)->port = port == 0 ? TOKEN_MANAGER_DEFAULT_PORT : port;
    (*tm)->sock = -1;

    return 0;
}

int tm_connect(token_manager_t *tm)
{
    struct sockaddr_in server_addr;
    int err;

    if (tm == NULL)
        return -EINVAL;

    tm->sock = os_socket(AF_INET, SOCK_STREAM, 0);
    if (tm->sock < 0)
        return tm->sock;

    server_addr.sin_family = AF_INET;
    os_inet_aton(tm->address, &server_addr.sin_addr);
    server_addr.sin_port = htons(tm->port);

    err = os_sock_set_timeouts(tm->sock, 200);
    if (err < 0)
    {
        tm_disconnect(tm);
        return err;
    }

    err = os_connect(tm->sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err != 0)
    {
        tm_disconnect(tm);
        return err;
    }

    err = __receive_reply(tm);
    if (err != 0)
    {
        tm_disconnect(tm);
        return err;
    }

    err = os_sock_set_timeouts(tm->sock, 4000);
    if (err < 0)
    {
        tm_disconnect(tm);
        return err;
    }

    return 0;
}

void tm_disconnect(token_manager_t *tm)
{
    if (tm->sock >= 0)
    {
        os_shutdown(tm->sock, SHUT_RDWR);
        os_closesocket(tm->sock);
    }
    tm->sock = -1;
}

void tm_free(token_manager_t **tm)
{
    if (tm == NULL || *tm == NULL)
        return;

    os_free((*tm)->address);

    os_free(*tm);
    *tm = NULL;
}

int tm_request_token(token_manager_t *tm, const exa_uuid_t *uuid,
                     exa_nodeid_t sender_id)
{
    int err;

    err = __send_request(tm, TOKEN_OP_ACQUIRE, uuid, sender_id);
    if (err != 0)
        return err;

    return __receive_reply(tm);
}

int tm_release_token(token_manager_t *tm, const exa_uuid_t *uuid,
                     exa_nodeid_t sender_id)
{
    int err;

    err = __send_request(tm, TOKEN_OP_RELEASE, uuid, sender_id);
    if (err != 0)
        return err;

    return __receive_reply(tm);
}

int tm_force_token_release(token_manager_t *tm, const exa_uuid_t *uuid)
{
    int err;

    err = __send_request(tm, TOKEN_OP_FORCE_RELEASE, uuid, EXA_NODEID_NONE);
    if (err != 0)
        return err;

    return __receive_reply(tm);
}

int tm_check_connection(token_manager_t *tm, const exa_uuid_t *uuid,
                        exa_nodeid_t sender_id)
{
   int err;
    fd_set read_fd;
    struct timeval timeout;

    if (tm == NULL)
        return -ENOENT;

    if (tm->sock < 0)
        return -EINVAL;

    FD_ZERO(&read_fd);
    FD_SET(tm->sock, &read_fd);

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    err = os_select(tm->sock + 1, &read_fd, NULL, NULL, &timeout);
    if (err < 0)
        return err;

    /* This socket is not supposed to be readable, so if it is, it means
     * it's been closed (and read() would return 0).
     */
    if (FD_ISSET(tm->sock, &read_fd))
        return -EBADF;

    err = __send_request(tm, TOKEN_OP_HEARTBEAT, uuid, sender_id);
    if (err != 0)
        return err;

    return __receive_reply(tm);
}
