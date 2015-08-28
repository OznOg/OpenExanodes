/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdio.h>

#include "token_manager/tm_server/src/tm_tokens.h"
#include "token_manager/tm_server/src/token_manager.h"
#include "token_manager/tm_server/src/tm_err.h"
#include "token_manager/tm_server/include/token_msg.h"
#include "token_manager/tm_server/include/tm_server.h"

#include "common/include/uuid.h"

#include "os/include/os_assert.h"
#include "os/include/os_dir.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"
#include "os/include/os_network.h"
#include "os/include/os_time.h"
#include "os/include/os_string.h"

#include <stdarg.h>

/* Max number of connections. (One token per cluster and we assume 2-node
   clusters hence 2 connections per token.) */
#define MAX_NORMAL_CONNECTIONS (TM_TOKENS_MAX * 2)

/* The +1 is for the privileged socket */
#define MAX_TOTAL_CONNECTIONS (MAX_NORMAL_CONNECTIONS + 1)

#define CONNECTION_NONE          -1
#define PRIVILEGED_CONNECTION    0
#define FIRST_NORMAL_CONNECTION  1

/** Whether to keep running */
static bool tm_server_run = true;

static int listen_socket = -1;       /**< Listen socket for regular clients */
static int listen_priv_socket = -1;  /**< Listen socket for privileged client */

typedef struct
{
    int sock;
    os_net_addr_str_t ip_addr;
} connect_info_t;

static fd_set setSocks;                         /**< Set of connected clients */
static connect_info_t connectlist[MAX_TOTAL_CONNECTIONS];  /**< Connected
                                                             * client info */

/** File where tokens are persisted */
static char tokens_file[OS_PATH_MAX];

static char log_file[OS_PATH_MAX];  /**< Name of file to log to */
static FILE *logfd = NULL;          /**< Log file */
static bool debugging = false;      /**< Whether to log debug messages */

static void __log(const char *level, const char *fmt, ...)
{
    struct tm date;
    const char *date_str;
    uint64_t msecs = os_gettimeofday_msec();
    time_t secs = (time_t)msecs / 1000;
    va_list al;

    if (!debugging && strcmp(level, "debug") == 0)
        return;

    os_localtime(&secs, &date);
    date_str = os_date_msec_to_str(&date, msecs % 1000);

    fprintf(logfd, "%s [%s] ", date_str, level);

    va_start(al, fmt);
    vfprintf(logfd, fmt, al);
    va_end(al);

    fprintf(logfd, "\n");

    fflush(logfd);
}

static int token_manager_open_log(const char *logfile)
{
    char *dir = os_strdup(logfile); /* os_dirname is going to truncate "dir" */
    int r;

    r = os_dir_create_recursive(os_dirname(dir));
    os_free(dir);

    if (r != 0)
        return -errno;

    logfd = fopen(logfile, "a");
    if (logfd == NULL)
        return -errno;

    os_strlcpy(log_file, logfile, sizeof(log_file));

    return 0;
}

static int token_manager_close_log(void)
{
    if (logfd == stdout)
        return 0;

    return fclose(logfd);
}

void token_manager_reopen_log(void)
{
    if (logfd != stdout)
        logfd = freopen(log_file, "a", logfd);
}

#define __error(...)    __log("ERROR", __VA_ARGS__)
#define __warning(...)  __log("WARNING", __VA_ARGS__)
#define __info(...)     __log("Info", __VA_ARGS__)
#define __debug(...)    __log("debug", __VA_ARGS__)

/**
 * Properly shutdown and close a socket.
 *
 * @param[in] sock  The socket to close
 */
static void __close_socket(int sock)
{
    os_shutdown(sock, SHUT_RDWR);
    os_closesocket(sock);
}

/**
 * Create the socket to listen on for client connections.
 *
 * @param[in] port   The port on which the server'll listen
 * @param[in] local  Specify whether the server'll listen only on local host
 *
 * @return A positive socket descriptor of the listening socket if successful,
 *         a negative error code otherwise.
 */
static int create_listen_socket(uint16_t port, bool local)
{
    int sd;
    struct sockaddr_in sin;
    int err;
    int enable;

    sd = os_socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        __error("Failed creating listen socket: %s (%d)", os_strerror(-sd), sd);
        return sd;
    }

    enable = 1;
    err = os_setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (err < 0)
    {
        __error("Failed setting listen socket option: %s (%d)", os_strerror(-err),
                err);
        os_closesocket(sd);
        return err;
    }

    /* Set socket timeout so that a connection problem doesn't go unnoticed for
     * too long. We chose a four seconds timeout arbitrarily.
     */
    err = os_sock_set_timeouts(sd, 4000);
    if (err < 0)
    {
        __error("Failed setting socket timeouts: %s (%d)", os_strerror(-err),
                err);
        os_closesocket(sd);
        return err;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = local ? inet_addr("127.0.0.1") : INADDR_ANY;
    sin.sin_port = htons(port);

    /* bind the socket to the address */
    err = os_bind(sd, (struct sockaddr *)&sin, sizeof(sin));
    if (err < 0)
    {
        __error("Failed binding listen socket (port %"PRIu16"): %s (%d)", port,
                os_strerror(-err), err);
        os_closesocket(sd);
        return err;
    }

    /* show that we are willing to listen */
    err = os_listen(sd, 5);
    if (err < 0)
    {
        __error("Failed listening: %s (%d)", os_strerror(-err), err);
        os_closesocket(sd);
        return err;
    }

    return sd;
}

/**
 * Add a socket connection to global connection array
 *
 * @param[in] sock      The socket to add
 * @param[in] ip_addr   The ip_addr of the client
 * @param[in] priv      Whether the socket is privileged
 *
 * @return connection id if successful, -ENOMEM otherwise
 */
static int add_connection(int sock, const os_net_addr_str_t ip_addr, bool priv)
{
    int i;

    if (!priv)
    {
        /* Search free slot in connection array */
        for (i = FIRST_NORMAL_CONNECTION; i <= MAX_NORMAL_CONNECTIONS; i++)
            if (connectlist[i].sock == CONNECTION_NONE)
            {
                connectlist[i].sock = sock;
                os_strlcpy(connectlist[i].ip_addr, ip_addr,
                           sizeof(connectlist[i].ip_addr));
                FD_SET(sock, &setSocks);
                return i;
            }
    }
    else
    {
        if (connectlist[PRIVILEGED_CONNECTION].sock == CONNECTION_NONE)
        {
            connectlist[PRIVILEGED_CONNECTION].sock = sock;
            os_strlcpy(connectlist[PRIVILEGED_CONNECTION].ip_addr, ip_addr,
                       sizeof(connectlist[PRIVILEGED_CONNECTION].ip_addr));
            FD_SET(sock, &setSocks);
            return PRIVILEGED_CONNECTION;
        }
    }

    return -ENOMEM;
}

/**
 * Close a generic connection
 *
 * @param[in] Id of the connection in the connection array
 *
 */
static void close_connection(int conn_id)
{
    FD_CLR(connectlist[conn_id].sock, &setSocks);
    __close_socket(connectlist[conn_id].sock);
    connectlist[conn_id].sock = CONNECTION_NONE;
    os_strlcpy(connectlist[conn_id].ip_addr, "",
               sizeof(connectlist[conn_id].ip_addr));
}

/**
 * Send data to a socket
 *
 * @param[in]   sock    The socket to write to
 * @param[in]   buffer  What to send
 * @param[in]   size    Size of buffer
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int send_data(int sock, const void *buffer, size_t size)
{
    int nb_bytes = 0;

    OS_ASSERT(sock > 0 && buffer != NULL && size > 0);

    do
    {
        int received;
        do
            received = os_send(sock, (const char *)buffer + nb_bytes,
                               size - nb_bytes);
        while (received == -EINTR);

        if (received < 0)
            return received;

        nb_bytes += received;
    } while (nb_bytes < size);

    return 0;
}

/**
 * Read data from socket and store them
 *
 * @param[in]       sock    The socket to read from
 * @param[in,out]   buffer  The place to save data
 * @param[in]       size    Size of buffer
 *
 * @return size if OK, 0 if the connection was closed and
 *         a negative error code otherwise
 */
static int receive_data(int sock, void *buffer, size_t size)
{
    int nb_bytes = 0;

    do
    {
        int received;

        do
            received = os_recv(sock, (char *)buffer + nb_bytes, size - nb_bytes, 0);
        while (received == -EINTR);

        if (received == 0)
            return 0;

        if (received < 0)
            return -errno;

        nb_bytes += received;

    } while (nb_bytes < size);

    return size;
}

/**
 * Accept a new client
 */
static void accept_new_client(int listening_socket, bool privileged)
{
    struct sockaddr_in addr;
    int addr_size = sizeof(addr);
    int sock;
    int conn_id;
    int enable = 1;
    token_reply_msg_t reply;
    int err;

    sock = os_accept(listening_socket, (struct sockaddr *)&addr, &addr_size);
    if (sock < 0)
    {
        __error("Failed accepting connection: %s (%d)", os_strerror(-sock), sock);
        return;
    }

    err = os_setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
    if (err < 0)
    {
        __error("Failed enabling keepalive: %s (%d)", os_strerror(-err), err);
        __close_socket(sock);
        return;
    }

    conn_id = add_connection(sock, os_inet_ntoa(addr.sin_addr), privileged);
    if (conn_id < 0)
    {
        __warning("Exceeded maximum number of connections."
                  " Rejected connection from %s.", os_inet_ntoa(addr.sin_addr));
        reply.result = TOKEN_RESULT_DENIED;
    }
    else
    {
        __info("Accepted connection from %s", os_inet_ntoa(addr.sin_addr));
        reply.result = TOKEN_RESULT_ACCEPTED;
    }

    err = send_data(sock, &reply, sizeof(reply));

    if (conn_id < 0 || err != 0)
    {
        if (conn_id >= 0)
            __error("Closing connection to %s as ack failed: %s (%d)",
                    connectlist[conn_id].ip_addr,
                    os_strerror(-err), err);
        __close_socket(sock);
    }
}

static const char *token_op_str(token_op_t op)
{
    if (!TOKEN_OP_IS_VALID(op))
        return NULL;

    switch (op)
    {
    case TOKEN_OP_ACQUIRE: return "acquire";
    case TOKEN_OP_RELEASE: return "release";
    case TOKEN_OP_FORCE_RELEASE: return "force_release";
    case TOKEN_OP_HEARTBEAT: return "heartbeat";
    }

    return NULL;
}

/**
 * Process a request.
 *
 * @param[in]       conn_id    Id of connection the request came from
 * @param[in]       request    Request from the client
 * @param[in,out]   reply      Reply to the client
 * @param[out]      token_err  Token management error (0 if none)
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int process_request(int conn_id, const token_request_msg_t *request,
                           token_reply_msg_t *reply, int *token_err)
{
    token_op_t op = request->op;
    exa_uuid_t uuid = request->cluster_uuid;
    exa_nodeid_t node_id = request->sender_id;
    const char *op_str = token_op_str(op);
    int err;
    os_net_addr_str_t ip_addr;
    os_strlcpy(ip_addr, connectlist[conn_id].ip_addr, sizeof(ip_addr));

    if (op_str == NULL)
    {
        __warning("Invalid operation %d in token request from client %s"
                  " (node %"PRInodeid" of cluster "UUID_FMT")",
                  op, ip_addr, node_id, UUID_VAL(&uuid));
        return -EINVAL;
    }

    if (conn_id == PRIVILEGED_CONNECTION)
    {
        if (op != TOKEN_OP_FORCE_RELEASE)
        {
            __error("Privileged client %s is not allowed to perform %s operation",
                    ip_addr, op_str);
            return -EPERM;
        }

        tm_tokens_force_release(&uuid);

        /* The operation is assumed to always succeed */
        *token_err = 0;
    }
    else
    {
        __debug("Client %s -> %s token "UUID_FMT" (node %"PRInodeid")",
                ip_addr, op_str, UUID_VAL(&uuid), node_id);

        switch (op)
        {
        case TOKEN_OP_ACQUIRE:
            *token_err = tm_tokens_set_holder(&uuid, node_id, ip_addr);
            break;

        case TOKEN_OP_RELEASE:
            *token_err = tm_tokens_release(&uuid, node_id);
            break;

        case TOKEN_OP_FORCE_RELEASE:
            __error("Client %s is not allowed to perform %s operation",
                    ip_addr, op_str);
            return -EPERM;
            break;

        case TOKEN_OP_HEARTBEAT:
            reply->result = TOKEN_RESULT_ACCEPTED;
            return 0;
            break;
        }
    }

    if (*token_err < 0)
        reply->result = TOKEN_RESULT_DENIED;
    else
        reply->result = TOKEN_RESULT_ACCEPTED;

    if (*token_err < 0)
    {
        /* A token management error is *not* a request processing error */
        return 0;
    }

    err = tm_tokens_save(tokens_file);
    if (err < 0)
    {
        __error("Failed saving tokens to '%s': %s (%d)", tokens_file,
                os_strerror(-err), err);

        if (op != TOKEN_OP_ACQUIRE)
            return err;

        /* As we were unable to commit the changes, we try to rollback */
        err = tm_tokens_release(&uuid, node_id);
        OS_ASSERT_VERBOSE(err == 0, "Unable to rollback set_holder operation"
                                    " for cluster " UUID_FMT ", node %"PRInodeid
                                    ": %s (%d)",
                                    UUID_VAL(&uuid), node_id,
                                    os_strerror(-err), err);
    }

    return 0;
}

static void log_operation(int conn_id, const token_request_msg_t *req,
                          const token_reply_msg_t *reply, int err)
{
    exa_uuid_str_t uuid_str;
    char whom[64];

    uuid2str(&req->cluster_uuid, uuid_str);

    /* The sender may be NONE when forcefully releasing a token: the sender
       in this case is *not* a node. */
    if (req->sender_id == EXA_NODEID_NONE)
        os_snprintf(whom, sizeof(whom), "client %s", connectlist[conn_id].ip_addr);
    else
        os_snprintf(whom, sizeof(whom), "node %"PRInodeid" (client %s)",
                    req->sender_id, connectlist[conn_id].ip_addr);

    switch (req->op)
    {
    case TOKEN_OP_ACQUIRE:
        if (reply->result == TOKEN_RESULT_ACCEPTED)
            __info("Token %s given to %s", uuid_str, whom);
        else
            __info("Token %s denied to %s: %s (%d)", uuid_str, whom,
                   tm_err_str(-err), err);
        break;

    case TOKEN_OP_RELEASE:
        if (reply->result == TOKEN_RESULT_ACCEPTED)
            __info("Token %s released by %s", uuid_str, whom);
        else
            __info("Release of token %s denied to %s: %s (%d)",
                   uuid_str, whom, tm_err_str(-err), err);
        break;

    case TOKEN_OP_FORCE_RELEASE:
        if (reply->result == TOKEN_RESULT_ACCEPTED)
            __info("Token %s forcefully released", uuid_str);
        else
            __info("Forceful release of token %s denied to %s: %s (%d)",
                    uuid_str, whom, tm_err_str(-err), err);
        break;

    case TOKEN_OP_HEARTBEAT:
        __debug("Got heartbeat from %s (%s)", whom, uuid_str);
        break;
    }
}

static void handle_request(int conn_id)
{
    int sock;
    token_request_msg_t tok_request;
    token_reply_msg_t tok_reply;
    int tok_err = 0;
    int err;

    sock = connectlist[conn_id].sock;

    err = receive_data(sock, &tok_request, sizeof(tok_request));
    if (err == 0)
    {
        __info("Client %s has disconnected", connectlist[conn_id].ip_addr);
        goto failed;
    }

    if (err < 0)
    {
        __error("Failed receiving data from client %s (socket %d): %s (%d)",
                connectlist[conn_id].ip_addr, sock,
                os_strerror(-err), err);
        goto failed;
    }

    err = process_request(conn_id, &tok_request, &tok_reply, &tok_err);
    log_operation(conn_id, &tok_request, &tok_reply, tok_err);
    if (err != 0)
        goto failed;

    err = send_data(sock, &tok_reply, sizeof(tok_reply));
    if (err)
    {
        __error("Failed sending data to client %s (socket %d): %s (%d)",
              connectlist[conn_id].ip_addr, sock,
              os_strerror(-err), err);
        goto failed;
    }

    return;

failed:
    close_connection(conn_id);
}

static void check_tcp_connection(void)
{
    static struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
    fd_set sock_set = setSocks;
    int conn_id;

    if (os_select(FD_SETSIZE, &sock_set, NULL,  NULL, &timeout) <= 0)
        return;

    /* Check working sockets */
    for (conn_id = 0; conn_id < MAX_TOTAL_CONNECTIONS; ++conn_id)
    {
        int sock = connectlist[conn_id].sock;
        if (sock >= 0 && FD_ISSET(sock, &sock_set))
            handle_request(conn_id);
    }

    if (FD_ISSET(listen_socket, &sock_set))
        accept_new_client(listen_socket, false);

    if (FD_ISSET(listen_priv_socket, &sock_set))
        accept_new_client(listen_priv_socket, true);
}

void token_manager_thread_stop(void)
{
    __info("Stopping manager...");
    tm_server_run = false;
}

void token_manager_thread(void *data)
{
    int i, err;
    uint16_t port, priv_port;
    token_manager_data_t *res = data;

    tm_server_run = true;

    OS_ASSERT(res->file != NULL && res->file[0] != '\0');
    os_strlcpy(tokens_file, res->file, sizeof(tokens_file));

    port = res->port > 0 ? res->port : TOKEN_MANAGER_DEFAULT_PORT;
    priv_port = res->priv_port > 0 ? res->priv_port : TOKEN_MANAGER_DEFAULT_PRIV_PORT;

    debugging = res->debug;

    if (res->logfile == NULL)
        logfd = stdout;
    else if (token_manager_open_log(res->logfile) != 0)
    {
        res->result = 1;
        return;
    }

    err = os_net_init();
    if (err)
    {
        __error("Failed initializing network");
        res->result = 1;
        return;
    }

    for (i = 0; i < MAX_TOTAL_CONNECTIONS; i++)
    {
        connectlist[i].sock = CONNECTION_NONE;
        os_strlcpy(connectlist[i].ip_addr, "",
                   sizeof(connectlist[i].ip_addr));
    }

    listen_socket = create_listen_socket(port, false);
    if (listen_socket < 0)
    {
        res->result = 1;
        goto done;
    }

    listen_priv_socket = create_listen_socket(priv_port, true);
    if (listen_priv_socket < 0)
    {
        res->result = 1;
        goto done;
    }

    FD_ZERO(&setSocks);
    FD_SET(listen_socket, &setSocks);
    FD_SET(listen_priv_socket, &setSocks);

    tm_tokens_init();

    __info("Loading tokens...");
    err = tm_tokens_load(tokens_file);
    if (err < 0)
    {
        __error("Failed loading tokens from '%s': %s (%d)",
                tokens_file, os_strerror(-err), err);
        res->result = 1;
        goto done;
    }

    __info("Listening on port %"PRIu16", privileged port %"PRIu16"...",
           port, priv_port);
    while (tm_server_run)
        check_tcp_connection();

    res->result = 0;

done:
    tm_tokens_cleanup();

    if (listen_socket >= 0)
        __close_socket(listen_socket);

    if (listen_priv_socket >= 0)
        __close_socket(listen_priv_socket);

    for (i = 0; i < MAX_TOTAL_CONNECTIONS; i++)
        if (connectlist[i].sock != CONNECTION_NONE)
            close_connection(i);

    os_net_cleanup();

    token_manager_close_log();
}
