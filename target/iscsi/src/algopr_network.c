/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

#include "log/include/log.h"

#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"
#include "common/include/exa_nbd_list.h"
#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_select.h"
#include "common/include/exa_socket.h"
#include "os/include/os_error.h"
#include "os/include/os_network.h"
#include "os/include/os_string.h"
#include "os/include/os_time.h"
#include "os/include/os_thread.h"
#include "os/include/os_semaphore.h"
#include "common/include/threadonize.h"
#include "rdev/include/exa_rdev.h"
#include "target/iscsi/include/pr_lock_algo.h"

#ifdef HAVE_VALGRIND_MEMCHECK_H
# include <valgrind/memcheck.h>
#endif

#define MIN_THREAD_STACK_SIZE_OF_THIS_PLUGIN (4096 * 14)

#define SOCK_LISTEN_FLAGS 1
#define SOCK_FLAGS 2

#define DATA_TRANSFER_COMPLETE  1
#define DATA_TRANSFER_PENDING   0
#define DATA_TRANSFER_ERROR    -1
#define DATA_TRANSFER_NEED_BIG_BUFFER -3

/* 200 stands for the number of buffer that are possibly waiting to be sent.
 * FIXME I do not really understand why I would be needed to be able to send
 * to more than EXA_MAX_NODES_NUMBER... */
#define MAX_SEND_ELT 200
#define MAX_BIG_RECV_ELT 3 /* FIXME I really wonder how this value was found,
                              as it is really small compared to
                              EXA_MAX_NODES_NUMBER */

typedef struct payload
{
    uint16_t      size1;
    uint32_t      size2;
    /* FIXME This structure needs to hold a buffer that is actually a
     * algopr_msgnetwork_t which is private. This size is kind of
     * hardcoded here because the network part would deverve to be
     * reworked (for exemple by having a callback in pr_algo_pr to
     * get a buffer with the correct amount of space) but the
     * cleanup work that was once begun here was not finished and
     * the code remained dirty.
     * Careful: if you need to modifiy this value, make sure the
     * actual size of data is the same under linux AND windows.
     * see bug #4652 */
#define SIZEOF_ALGOPR_MSGNETWORK_T 20
    unsigned char payload[SIZEOF_ALGOPR_MSGNETWORK_T];
    unsigned char *buffer;
} __attribute__((packed)) payload_t;

typedef struct wq
{
    os_thread_mutex_t lock;
    os_sem_t           sem;
    int ev;
    int wait;
} wq_t;

struct pending_request
{
    payload_t *payload;
    int        nb_readwrite;
    bool       used;
    bool       big_buffer;
};

struct ethernet
{
    os_thread_t receive_thread;
    os_thread_t send_thread;
    os_thread_t accept_thread;
    int         accept_sock;
    os_thread_mutex_t      sem;
    /* Internal structure initialised before calling init_plugin used to send data */
    struct nbd_root_list root_list_send;
    struct nbd_root_list root_list_big_recv;
    struct nbd_list      send_list[EXA_MAX_NODES_NUMBER];
    wq_t                 wq_send;
    int                  max_buffer_size;
};

typedef struct
{
    char ip_addr[EXA_MAXSIZE_NICADDRESS + 1];
    int  sock;
} peer_t;

static volatile bool algopr_run;
static uint16_t algopr_network_port = 30799;         /**< tcp listen port used */
static exa_nodeid_t this_node_id = EXA_NODEID_NONE;  /**< Id of the local node */
static struct ethernet eth;                          /**< bag of crap */

static peer_t peers[EXA_MAX_NODES_NUMBER];
static os_thread_mutex_t peers_lock;

static bool suspended = true;

void algopr_network_suspend(void)
{
    suspended = true;
}

void algopr_network_resume(void)
{
    suspended = false;
}

static int internal_setsock_opt(int sock, int islisten)
{
    int authorization;
    struct linger linger;
    int TCP_buffers;

    /* make it possible to reuse the socket */
    if (islisten & SOCK_LISTEN_FLAGS)
    {
        authorization = 1;
        if (os_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		          &authorization, sizeof(authorization)) < 0)
            goto error;
    }

    if (islisten & SOCK_FLAGS)
    {
        /* tcp_nodelay : deactivate the nagle algorithm + linger to
           immediatly shutdown a socket + setting the good send/recv
           size of tcp buffer */
        authorization = 1;
        if (os_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		          &authorization, sizeof(authorization)) < 0)
            goto error;

        /* Set the socket kernel allocation to GFP_ATOMIC */
        if (exa_socket_set_atomic(sock) < 0)
            goto error;

        /* Set the size of the socket TCP send buffers */
	TCP_buffers = 65536;
        if (os_setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
		          &TCP_buffers, sizeof(TCP_buffers)) < 0)
            goto error;

        /* Set the size of the socket TCP receive buffers */
        TCP_buffers = 128 * 1024;
        if (os_setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		          &TCP_buffers, sizeof(TCP_buffers)) < 0)
            goto error;

        /* Fix the delay in socket shutdown */
        linger.l_onoff = 1;
        linger.l_linger = 0;
        if (os_setsockopt(sock, SOL_SOCKET, SO_LINGER,
                          &linger, sizeof(linger)) < 0)
            goto error;
    }

    return EXA_SUCCESS;

error:
    return -EXA_ERR_CREATE_SOCKET;
}

static void __close_socket(int sock)
{
    os_shutdown(sock, SHUT_RDWR);
    os_closesocket(sock);
}

static int __connect_to_peer(exa_nodeid_t node_id)
{
    peer_t *peer;
    struct sockaddr_in this_node_addr;
    struct sockaddr_in serv_addr;
    struct in_addr inaddr;
    int sock = -1;
    int err = EXA_SUCCESS;
    int r;

    peer = &peers[node_id];

    exalog_debug("connecting to peer %"PRInodeid" '%s'", node_id, peer->ip_addr);

    EXA_ASSERT(peer->ip_addr[0] != '\0');
    EXA_ASSERT(peer->sock == -1);

    r = os_inet_aton(peer->ip_addr, &inaddr);
    if (r == 0)
    {
        exalog_error("Invalid IP '%s' for node %"PRInodeid, peer->ip_addr, node_id);
        err = -EXA_ERR_CREATE_SOCKET;
        goto done;
    }

    r = os_socket(PF_INET, SOCK_STREAM, 0);
    if (r < 0)
    {
        exalog_error("Failed creating socket: %s (%d)", os_strerror(-r), r);
        err = -EXA_ERR_CREATE_SOCKET;
        goto done;
    }
    sock = r;

    /* Bind the socket. Otherwise the system would be free to use whichever
       interface it pleases, which may not match the IP address this node is
       known as on other nodes participating in the PR. */
    this_node_addr.sin_family = AF_INET;
    os_inet_aton(peers[this_node_id].ip_addr, &this_node_addr.sin_addr);
    this_node_addr.sin_port = htons(0); /* let the system choose */
    r = os_bind(sock, (struct sockaddr *)&this_node_addr, sizeof(this_node_addr));
    if (r < 0)
    {
        exalog_error("Failed binding socket %d to %s: %s (%d)", sock,
                     peers[this_node_id].ip_addr, os_strerror(-r), r);
        err = -EXA_ERR_CREATE_SOCKET;
        goto done;
    }

    os_sock_set_timeouts(sock, 4000);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inaddr.s_addr;
    serv_addr.sin_port = htons(algopr_network_port);
    r = os_connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (r < 0)
    {
        exalog_error("Cannot connect to node %d (IP '%s'): %s (%d)",
                     node_id, peer->ip_addr, os_strerror(-r), r);
        err = -NBD_ERR_SERVER_REFUSED_CONNECTION;
        goto done;
    }

    os_sock_set_timeouts(sock, 0);
    internal_setsock_opt(sock, SOCK_FLAGS);

    peer->sock = sock;

done:
    if (err != EXA_SUCCESS && sock != -1)
        __close_socket(sock);

    return err;
}

static void __disconnect_from_peer(exa_nodeid_t node_id)
{
    peer_t *peer = &peers[node_id];

    exalog_debug("disconnecting from peer %"PRInodeid" '%s'", node_id, peer->ip_addr);

    EXA_ASSERT(peer->sock >= 0);

    __close_socket(peer->sock);
    peer->sock = -1;
}

static exa_nodeid_t get_peer_id_from_ip_addr(const char *ip_addr)
{
    exa_nodeid_t i;
    exa_nodeid_t node_id = EXA_NODEID_NONE;

    os_thread_mutex_lock(&peers_lock);

    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        if (!strcmp(ip_addr, peers[i].ip_addr))
        {
            node_id = i;
            break;
        }

    os_thread_mutex_unlock(&peers_lock);

    return node_id;
}

static void set_peer_socket(exa_nodeid_t node_id, const char *ip_addr, int sock)
{
    peer_t *peer;

    exalog_debug("setting socket of peer %"PRInodeid": %d '%s'", node_id, sock, ip_addr);

    os_thread_mutex_lock(&peers_lock);

    peer = &peers[node_id];

    EXA_ASSERT(peer->sock == -1);
    /* A node's IP address is not supposed to change during the lifetime of a
       cluster (ie, the node id <-> IP address mapping is bijective), so we
       assert if the received IP doesn't match the one registered */
    EXA_ASSERT_VERBOSE(strcmp(ip_addr, peer->ip_addr) == 0,
                       "peer %"PRInodeid": received addr %s, expected %s",
                       node_id, ip_addr, peer->ip_addr);

    peer->sock = sock;

    os_thread_mutex_unlock(&peers_lock);
}

static int __get_peer_socket(exa_nodeid_t node_id)
{
    return peers[node_id].sock;
}

static bool __peer_is_connected(exa_nodeid_t node_id)
{
    return peers[node_id].sock >= 0;
}

/* If the peer is already known, this function doesn't do anything
   (apart from checking that the IP address given is identical to the one
   already known). Most notably, it leaves the peer's socket alone */
static void __set_peer(exa_nodeid_t node_id, const char *ip_addr)
{
    peer_t *peer;

    exalog_debug("setting peer %"PRInodeid": '%s'", node_id, ip_addr);

    peer = &peers[node_id];

    /* ip addr of a node can't change */
    EXA_ASSERT(peer->ip_addr[0] == '\0' || strcmp(peer->ip_addr, ip_addr) == 0);
    EXA_ASSERT(os_strlcpy(peer->ip_addr, ip_addr, sizeof(peer->ip_addr))
               < sizeof(peer->ip_addr));
}

static void __reset_peer(exa_nodeid_t node_id)
{
    peer_t *peer = &peers[node_id];

    memset(peer->ip_addr, '\0', sizeof(peer->ip_addr));
    peer->sock = -1;
}

static void init_peers(void)
{
    exa_nodeid_t node_id;

    exalog_debug("initializing peers");

    for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
        __reset_peer(node_id);

    os_thread_mutex_init(&peers_lock);
}

static void cleanup_peers(void)
{
    exa_nodeid_t node_id;

    os_thread_mutex_destroy(&peers_lock);

    for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
    {
        if (__peer_is_connected(node_id))
            __disconnect_from_peer(node_id);
        __reset_peer(node_id);
    }
}

static void set_peers(const char ip_addresses[][EXA_MAXSIZE_NICADDRESS + 1])
{
    exa_nodeid_t node_id;

    os_thread_mutex_lock(&peers_lock);

    for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
        if (ip_addresses[node_id][0] == '\0')
        {
            EXA_ASSERT(!__peer_is_connected(node_id));
            __reset_peer(node_id);
        }
        else
            __set_peer(node_id, ip_addresses[node_id]);

    os_thread_mutex_unlock(&peers_lock);
}

static void wq_wake(wq_t *wq)
{
    os_thread_mutex_lock(&wq->lock);
    if (wq->wait != 0)
    {
        os_sem_post(&wq->sem);
        wq->wait = 0;
    }
    else
        wq->ev = 1;

    os_thread_mutex_unlock(&wq->lock);
}

static void wq_wait(wq_t *wq)
{
    int wait = 0;

    os_thread_mutex_lock(&wq->lock);
    if (wq->ev != 0)
        wq->ev = 0;
    else
        wait = 1;
    wq->wait = wait;

    os_thread_mutex_unlock(&wq->lock);

    if (wait == 1)
        os_sem_wait(&wq->sem);
}

static void wq_init(wq_t *wq)
{
    wq->ev = 0;
    wq->wait = 0;
    os_thread_mutex_init(&wq->lock);
    os_sem_init(&wq->sem, 0);
}

/**
 * Thread responsible for accepting connections
 *
 * It's a separate thread because we accept do some memory allocation and we
 * must avoid that in recv thread.
 *
 * @param unused  Unused parameter
 */
static void accept_thread(void *unused)
{
    exalog_as(EXAMSG_ISCSI_ID);

    while (algopr_run)
    {
        exa_nodeid_t node_id;
        const char *ip_addr;
        struct sockaddr_in client_address;
        int size = sizeof(client_address);

        int sock =
            os_accept(eth.accept_sock, (struct sockaddr *)&client_address, &size);

        if (sock < 0)
            continue; /* it's a false accept */

        ip_addr = os_inet_ntoa(client_address.sin_addr);

        if (!suspended)
        {
            exalog_warning("Closing incoming connection from %s while not"
                           " suspended.", ip_addr);
            __close_socket(sock);
            continue;
        }

        internal_setsock_opt(sock, SOCK_FLAGS);

        node_id = get_peer_id_from_ip_addr(ip_addr);
        if (!EXA_NODEID_VALID(node_id))
        {
            exalog_warning("Closing incoming connection from unknown node %s.",
                           ip_addr);
            __close_socket(sock);
            continue;
        }

        set_peer_socket(node_id, ip_addr, sock);
    }
}

/* reset pending header to NULL */
static inline payload_t *request_reset(struct pending_request *request)
{
    payload_t *payload = request->payload;

    request->nb_readwrite = 0;
    request->payload = NULL;
    request->used = false;

    return payload;
}

/* set the pending header to the passed header */
static inline int request_init_transfer(payload_t *new_payload,
                                        struct pending_request *request)
{
    if (request->payload != NULL)
        return 0;

    request->payload = new_payload;
    request->used = true;
    request->big_buffer = false;

    return 1;
}

/**
 * Send header and buffer if any
 *
 * @param[in] fd       socket in which to perform the send
 * @param[in] request  the request describing the send operation
 *
 * @return DATA_TRANSFER_ERROR     socket invalid or closed.
 *         DATA_TRANSFER_COMPLETE  if successfully transferred all pending
 *                                 data (header and buffer if any)
 *         DATA_TRANSFER_PENDING   if some remaining data to transfer
 */
static int request_send(int fd, struct pending_request *request)
{
    int ret;

    if (request->nb_readwrite < sizeof(payload_t))
    {
        const void *buffer = request->payload + request->nb_readwrite;
        int size = sizeof(payload_t) - request->nb_readwrite;
        do
            ret = os_send(fd, buffer, size);
        while (ret == -EINTR);
    }
    else
    {
        const void *buffer = request->payload->buffer
                             + request->nb_readwrite - sizeof(payload_t);
        int size = request->payload->size2
                             - (request->nb_readwrite - sizeof(payload_t));

        do
            ret = os_send(fd, buffer, size);
        while (ret == -EINTR);
    }

    /* XXX For some unknown reason, the send can return 0 here, and I cannot
     * figure out why.... For now I just ignore it an treat it as 'normal' but
     * we should probably pay attention to this... */
    if (ret < 0)
        return DATA_TRANSFER_ERROR;

    request->nb_readwrite += ret;

    if (request->nb_readwrite < sizeof(payload_t) + request->payload->size2)
        return DATA_TRANSFER_PENDING;

    return DATA_TRANSFER_COMPLETE;
}

/**
 * Receive header and buffer if any
 *
 * @param[in] fd       socket in which to perform the receive
 * @param[in] request  the request describing the receive operation
 *
 * @return DATA_TRANSFER_ERROR     socket invalid or closed.
 *         DATA_TRANSFER_COMPLETE  if successfully transferred all pending
 *                                 data (header and buffer if any)
 *         DATA_TRANSFER_PENDING   if some remaining data to transfer
 */
static int request_receive(int fd, struct pending_request *request)
{
    int ret;

    if (request->nb_readwrite < sizeof(payload_t))
    {
        void *buffer = request->payload + request->nb_readwrite;
        int size = sizeof(payload_t) - request->nb_readwrite;
        do
            ret = os_recv(fd, buffer, size, 0);
        while (ret == -EINTR);
    }
    else
    {
        void *buffer = request->payload->buffer
                             + request->nb_readwrite - sizeof(payload_t);
        int size = request->payload->size2
                             - (request->nb_readwrite - sizeof(payload_t));

        do
            ret = os_recv(fd, buffer, size, 0);
        while (ret == -EINTR);
    }

    /* recv returning 0 means the peer disconnected: this is an error */
    if (ret <= 0)
        return DATA_TRANSFER_ERROR;

    request->nb_readwrite += ret;

    /* If header was not fully retrieved, pending data are remaining */
    if (request->nb_readwrite < sizeof(payload_t))
        return DATA_TRANSFER_PENDING;

    /* If we are here, this means that the header was fully received, thus we
     * can check if more data are coming with this request. */
    if (request->payload->size2 > 0 && !request->big_buffer)
        return DATA_TRANSFER_NEED_BIG_BUFFER;

    if (request->nb_readwrite < sizeof(payload_t) + request->payload->size2)
        return DATA_TRANSFER_PENDING;

    return DATA_TRANSFER_COMPLETE;
}

static void drop_all_messages_for_node(exa_nodeid_t node_id)
{
    payload_t *payload;

    while ((payload = nbd_list_remove(&eth.send_list[node_id], NULL,
                                      LISTNOWAIT)) != NULL)
        nbd_list_post(&eth.send_list[node_id].root->free, payload, -1);
}

/* thread for asynchronously sending data for a client or a server */
static void algopr_send_thread(void *unused)
{
    struct pending_request pending_requests[EXA_MAX_NODES_NUMBER];
    int i;
    exa_select_handle_t *sh = exa_select_new_handle();

    exalog_as(EXAMSG_ISCSI_ID);

    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        request_reset(&pending_requests[i]);

    while (algopr_run)
    {
        fd_set fds;
	int nfds = 0;
        bool active_sock = false;

        FD_ZERO(&fds);

        /* if one node is added or deleted, this deletion or addition are
           effective after this */
        os_thread_mutex_lock(&peers_lock);

        for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        {
            int fd_act = __get_peer_socket(i);
            if (fd_act < 0)
            {
                /* release all buffer of clients who's sockets were closed */
                payload_t *payload = request_reset(&pending_requests[i]);
                if (payload != NULL)
                    nbd_list_post(&eth.send_list[i].root->free, payload, -1);

                /* release all pending messages for node i: connection is dead,
                 * those messages will never be delivered anyway. */
                drop_all_messages_for_node(i);
                continue;
            }

            if (!pending_requests[i].used)
            {
                /* pick a new request if no one is in progress for this peer */
                payload_t *payload = nbd_list_remove(&eth.send_list[i], NULL,
                                                     LISTNOWAIT);
                if (payload)
                    request_init_transfer(payload, &pending_requests[i]);
            }

            if (pending_requests[i].used)
            {
                /* if buffers are waiting to be sent, add peer to select list */
                FD_SET(fd_act, &fds);
		nfds = fd_act > nfds ? fd_act : nfds;
                active_sock = true;
            }
        }
        os_thread_mutex_unlock(&peers_lock);

        if (!active_sock)
        {
            wq_wait(&eth.wq_send);
            /* we were waiting for new requests to send, someone signaled us
             * so restart the loop and look for new request. */
            continue;
        }

	exa_select_out(sh, nfds + 1, &fds);

        os_thread_mutex_lock(&peers_lock);
        for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        {
            struct pending_request *request = &pending_requests[i];
            int fd_act = __get_peer_socket(i);

            if (fd_act >= 0 && pending_requests[i].used
                && FD_ISSET(fd_act, &fds))
            {
                /* send remaining data if any */
                int ret = request_send(fd_act, request);
                switch (ret)
                {
                case DATA_TRANSFER_COMPLETE:
                    nbd_list_post(&eth.send_list[i].root->free,
                                  request->payload, -1);
                    request_reset(request);
                    break;

                case DATA_TRANSFER_ERROR:
                    nbd_list_post(&eth.send_list[i].root->free,
                                  request->payload, -1);
                    request_reset(request);
                    break;

                case DATA_TRANSFER_PENDING:
                    break;
                }
            }
        }
        os_thread_mutex_unlock(&peers_lock);
    }

    exa_select_delete_handle(sh);
}

/*
 * thread responsible for receiving data for a client or a server
 * note when we add client, this client is effectively added in the receive queue
 * only few second later due to the select timeout of 3 seconds
 * and there are the same problem for the deleteion of a client
 */
static void algopr_receive_thread(void *unused)
{
    struct pending_request pending_requests[EXA_MAX_NODES_NUMBER];
    exa_select_handle_t *sh = exa_select_new_handle();
    int i;
    int ret;
    payload_t *payload = NULL;
    struct nbd_root_list root_list_recv;

    /* FIXME: handle the case when we have more than 1024 open file (limit of fd_set) */
    fd_set fds;

    exalog_as(EXAMSG_ISCSI_ID);

    nbd_init_root(EXA_MAX_NODES_NUMBER, sizeof(payload_t), &root_list_recv);

    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        request_reset(&pending_requests[i]);

    while (algopr_run)
    {
	int nfds = 0;
        FD_ZERO(&fds);
        /* if one node is added or deleted, this deletion or addition are
           effective after this */
        os_thread_mutex_lock(&peers_lock);
        for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        {
            int fd_act = __get_peer_socket(i);
            if (fd_act < 0)
            {
                payload_t *temp_payload;
                temp_payload = request_reset(&pending_requests[i]);
                if (temp_payload != NULL)
                {
                    if (pending_requests[i].big_buffer)
                        nbd_list_post(&eth.root_list_big_recv.free,
                                      temp_payload->buffer, -1);

                    nbd_list_post(&root_list_recv.free, temp_payload, -1);
                }
                temp_payload = NULL;
                continue;
            }

            FD_SET(fd_act, &fds);
	    nfds = fd_act > nfds ? fd_act : nfds;
        }
        os_thread_mutex_unlock(&peers_lock);

        ret = exa_select_in(sh, nfds + 1, &fds);
        if (ret != 0 && ret != -EFAULT)
            exalog_error("Select upon receive failed: %s (%d)",
                         os_strerror(-ret), ret);

        os_thread_mutex_lock(&peers_lock);
        for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        {
            struct pending_request *req;
            int fd_act;

            fd_act = __get_peer_socket(i);
            if (fd_act < 0 || !FD_ISSET(fd_act, &fds))
                continue;

            req = &pending_requests[i];
            /* WARNING payload is kept from an iteration of while loop to
             * another, so the variable MUST be global. */
            /* FIXME Remove the nbdlist which is useless as we already know
             * that we NEED EXA_MAX_NODES_NUMBER payload_t elements to be able
             * to receive simultaneously from EXA_MAX_NODES_NUMBER nodes
             * FIXME the LISTWAIT flag below is WRONG because waiting here
             * would mean deadlock... hopefully there are enough elements, and
             * we never wait.... */
            if (payload == NULL)
            {
                payload = nbd_list_remove(&root_list_recv.free, NULL, LISTWAIT);
                EXA_ASSERT(payload != NULL);
            }
            if (request_init_transfer(payload, req) == 1)
                payload = NULL;

            ret = request_receive(fd_act, req);
            if (ret == DATA_TRANSFER_NEED_BIG_BUFFER)
            {
                req->payload->buffer = nbd_list_remove(&eth.root_list_big_recv.free,
                                                       NULL, LISTWAIT);
                EXA_ASSERT(req->payload->buffer != NULL);

                req->big_buffer = true;

                /* here we just continue because it is forbidden to call
                 * request_receive without passing into select (as sockets are
                 * blocking, we may remain blocked on the recv of nothing) */
                continue;
            }

            if (ret == DATA_TRANSFER_PENDING)
                continue;

            if (ret == DATA_TRANSFER_ERROR)
            {
                payload_t *temp_payload = request_reset(req);

                if (req->big_buffer)
                    nbd_list_post(&eth.root_list_big_recv.free,
                                  temp_payload->buffer, -1);

                nbd_list_post(&root_list_recv.free, temp_payload, -1);

                __disconnect_from_peer(i);

                if (!suspended)
                    exalog_warning("Failed receiving from peer %" PRInodeid
                                  " (socket %d): transfer error.", i, fd_act);
                continue;
            }

            if (ret == DATA_TRANSFER_COMPLETE)
            {
                payload_t *_payload = request_reset(req);

                /* update data network checking data */
                algopr_new_msg(_payload->payload, _payload->size1,
                               _payload->buffer,  _payload->size2);
                nbd_list_post(&root_list_recv.free, _payload, -1);
            }
        }
        os_thread_mutex_unlock(&peers_lock);
    }

    nbd_close_root(&root_list_recv);
    exa_select_delete_handle(sh);
}

int algopr_send_data(exa_nodeid_t node_id, void *buffer1, int size1,
                     void *buffer2, int size2)
{
    payload_t *payload;

    EXA_ASSERT(EXA_NODEID_VALID(node_id));

    /* FIXME see comment above definition of SIZEOF_ALGOPR_MSGNETWORK_T */
    EXA_ASSERT(size1 == SIZEOF_ALGOPR_MSGNETWORK_T);

    /* Send is local, just forwrd it */
    if (node_id == this_node_id)
    {
	algopr_new_msg(buffer1, size1, buffer2, size2);
	return 1;
    }

    /* Note: All nbd lists send_list[] share the same root so here we don't
     * care in which one is picked up the buffer */
    payload = nbd_list_remove(&eth.send_list[0].root->free, NULL, LISTWAIT);
    EXA_ASSERT(payload != NULL);

    memcpy(payload->payload, buffer1, size1);

    payload->size1 = size1;
    payload->size2 = size2;
    payload->buffer = buffer2;

    nbd_list_post(&eth.send_list[node_id], payload, -1);
    wq_wake(&eth.wq_send);

    return 1;
}

void __algpr_recv_msg_free(void *payload)
{
    nbd_list_post(&eth.root_list_big_recv.free, payload, -1);
}

void algopr_set_clients(const char addresses[][EXA_MAXSIZE_NICADDRESS + 1])
{
    set_peers(addresses);
}

void algopr_update_client_connections(const exa_nodeset_t *mship)
{
    exa_nodeid_t node_id;

    os_thread_mutex_lock(&peers_lock);

    for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
    {

        /* FIXME Handle errors */
        if (exa_nodeset_contains(mship, node_id) && !__peer_is_connected(node_id))
        {
            /* Don't connect to self nor to nodes with a higher node id */
            if (node_id < this_node_id)
                __connect_to_peer(node_id);
        }
        else if (!exa_nodeset_contains(mship, node_id) && __peer_is_connected(node_id))
            __disconnect_from_peer(node_id);
    }

    os_thread_mutex_unlock(&peers_lock);
}

int algopr_close_plugin(void)
{
    algopr_run = false;

    wq_wake(&eth.wq_send);

    os_thread_join(eth.send_thread);

    os_thread_join(eth.receive_thread);

    __close_socket(eth.accept_sock);
    os_thread_join(eth.accept_thread);

    /* Reset these last, threads may need it until joined */
    cleanup_peers();
    this_node_id = EXA_NODEID_NONE;

    /* FIXME What about resetting 'eth'? */

    return EXA_SUCCESS;
}

/**
 * init_plugin initialise internal data
 *
 * if we are server, try to bind and launch a thread to accept
 * launch the thread of receive data
 *
 * @param net_plugin  Info on the new instance that we will fill
 *
 * @return EXA_SUCCESS or error
 */
int algopr_init_plugin(exa_nodeid_t node_id, int max_buffer_size)
{
    struct sockaddr_in serv_addr;
    int retval = -NBD_ERR_MALLOC_FAILED;
    int i;

    EXA_ASSERT(EXA_NODEID_VALID(node_id));
    this_node_id = node_id;

    init_peers();

    eth.max_buffer_size = max_buffer_size + max_buffer_size;

    wq_init(&eth.wq_send);

    nbd_init_root(MAX_BIG_RECV_ELT, eth.max_buffer_size, &eth.root_list_big_recv);

    nbd_init_root(MAX_SEND_ELT, sizeof(payload_t), &eth.root_list_send);
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        nbd_init_list(&eth.root_list_send, &eth.send_list[i]);

    algopr_run = true;

    if (!exathread_create_named(&eth.receive_thread,
		                NBD_THREAD_STACK_SIZE +
				MIN_THREAD_STACK_SIZE_OF_THIS_PLUGIN,
				algopr_receive_thread, NULL, "AlgoPrRcv"))
	return -NBD_ERR_THREAD_CREATION;

    if (!exathread_create_named(&eth.send_thread,
		                NBD_THREAD_STACK_SIZE +
				MIN_THREAD_STACK_SIZE_OF_THIS_PLUGIN,
				algopr_send_thread, NULL, "AlgoPrSnd"))
        return -NBD_ERR_THREAD_CREATION;

    eth.accept_sock = os_socket(PF_INET, SOCK_STREAM, 0);
    if (eth.accept_sock < 0)
        return -EXA_ERR_CREATE_SOCKET;

    /* bind a socket to SERVERD_DATA_PORT port and make it listen for incoming
     * connections */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(algopr_network_port);

    retval = internal_setsock_opt(eth.accept_sock, SOCK_LISTEN_FLAGS);
    if (retval != EXA_SUCCESS)
        return retval;

    if (os_bind(eth.accept_sock, (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)) < 0)
        return -EXA_ERR_CREATE_SOCKET;

    if (os_listen(eth.accept_sock, EXA_MAX_NODES_NUMBER) < 0)
        return -EXA_ERR_CREATE_SOCKET;

    if (!exathread_create_named(&eth.accept_thread,
		                NBD_THREAD_STACK_SIZE +
				MIN_THREAD_STACK_SIZE_OF_THIS_PLUGIN,
				accept_thread, NULL, "servEthAccPlugin"))
        return -NBD_ERR_THREAD_CREATION;

    return EXA_SUCCESS;
}
