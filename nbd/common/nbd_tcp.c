/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/**
 * \file
 * General documentation :
 * all netplugins must export these functions :
 *
 * netplugin structure get some variable function from user:
 *
 * - plugin->get_buffer() function returns the address of one buffer, returns
 *                        plugin->NULL if the number is invalid for the request.
 *
 * - plugin->end_sending() function called when the send of data ends, and so
 *                         user can release the buffe it's an interesting
 *                         function with asynchronous write or datagram
 *                         plugin, to keep a buffer after the send_data, and only
 *                         release it when data are successfully transfered (this
 *                         functionnality was not really used with tcp, because
 *                         data are copied in kernel buffer).
 *
 * - plugin->List list when plugin will put all received header.
 *
 *
 * netplugin "specification"
 *
 * - the plugin must enforce data message integrity : two message cannont be
 *   mixed with another and so the plugin must avoid data corruption
 *
 * - the plugin put all receiving header in the plugin->List of the plugin
 *   structure, and he directly write data to the buffer if needed
 *
 * - the plugin must add the client_id of the remote node in received header,
 *   this is the local client_id and so it not unique in the cluster.
 *
 * - plugin->get_buffer() function of the user can return NULL, in this case,
 *   the plugin must do fake receive and don't break the connection, it must
 *   contiunue to work.
 */
#include "nbd/common/nbd_common.h"
#include "nbd/common/nbd_tcp.h"

#include "log/include/log.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_select.h"
#include "common/include/exa_socket.h"
#include "common/include/threadonize.h"

#include "os/include/os_compiler.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_network.h"
#include "os/include/os_semaphore.h"
#include "os/include/os_string.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"

#define MIN_THREAD_STACK_SIZE_OF_THIS_PLUGIN (4096*14)

#define SOCK_LISTEN_FLAGS 1
#define SOCK_FLAGS 2

#define DATA_TRANSFER_COMPLETE  1
#define DATA_TRANSFER_PENDING   0
#define DATA_TRANSFER_ERROR    -1

struct tcp_plugin
{
    struct nbd_root_list send_list;

    /** address which is used by the lib to bind its sockets */
    struct in_addr data_addr;

    struct {
        os_thread_t tid;
        bool        run;
    } receive_thread;

    struct {
        os_thread_t tid;
        bool        run;
        os_sem_t    semaphore;
    } send_thread;

    struct {
        os_thread_t tid;
        bool        run;
        int         socket;
    } accept_thread;

    os_thread_rwlock_t peers_lock; /** lock of the peer array */
    struct peer {
        int sock;             /**< sock fd of peer */
        exa_nodeid_t node_id; /**< node_id of peer */
        char ip_addr[EXA_MAXSIZE_NICADDRESS + 1];

        /* Internal structure initialised before calling init_plugin used to send data */
        struct nbd_list send_list;
    } peers[EXA_MAX_NODES_NUMBER];
    int last_peer_idx; /*< keep record of the last peer idx that was
                           registered in order not to go through the whole
                           array when there are just a few nodes. */
};

static int TCP_buffers;

struct pending_request
{
    nbd_io_desc_t *io_desc;
    char *buffer;
    size_t buf_size;
    int nb_readwrite;
};

static void close_socket(int socket)
{
    os_shutdown(socket, SHUT_RDWR);
    os_closesocket(socket);
}

static int internal_setsock_opt(int sock, int islisten)
{
  int autorisation;
  struct linger linger;
  int recv_buf_size;

  /* make it possible to reuse the socket */
  if (islisten & SOCK_LISTEN_FLAGS)
    {
       autorisation = 1;
       if (os_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &autorisation,
                         sizeof(autorisation)) < 0)
        goto error;
    }

  if (islisten & SOCK_FLAGS)
    {
      /* tcp_nodelay : deactivate the nagle algorithm + linger to immediatly
         shutdown a socket + setting the good send/recv size of tcp buffer */
      autorisation = 1;
      if (os_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &autorisation,
                        sizeof(autorisation)) < 0)
	  goto error;

      /* Set the socket kernel allocation to GFP_ATOMIC */
      if (exa_socket_set_atomic(sock) < 0)
	  goto error;

      /**********************************************************************/
      /* FIXME WIN32 SO_SNDBUF SO_RCVBUF
       * The following sockopt values where found experimentally on linux BUT
       * for some reasons make scalability on windows impossible with the
       * default values (for now 128k): the first 14 nodes are ok, but from
       * the 15th, new nodes make the global througthput completely colapse to
       * 4k/s. The global CPU also reaches 0 when performing from a > 14 node,
       * even under heavy loads. */
      /* FIXME WIN32 what is the SO_SNDBUF limitation ? this really seems to
       * be a kernel limitation, but did not have enougth time to see why.
       * It could be very nice to find out WHY this causes pb... */

      /* Set the size of the socket TCP send buffers */
      if (os_setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &TCP_buffers,
                        sizeof(TCP_buffers)) < 0)
          goto error;

      /* Set the size of the socket TCP receive buffers: 128K */
      recv_buf_size = 128 * 1024;
      if (os_setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_buf_size,
		        sizeof(recv_buf_size)) < 0)
	  goto error;
      /**********************************************************************/

      /* Fix the delay in socket shutdown */
      linger.l_onoff = 1;
      linger.l_linger = 0;
      if (os_setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger,
                        sizeof(linger)) < 0)
	goto error;

    }

  return EXA_SUCCESS;

error:
  exalog_error("Cannot set opt of socket %d", sock);

  return -EXA_ERR_CREATE_SOCKET;
}


/**
 * Accept a connection from a peer and inform it
 *
 * @param tcp     Tcp plugin info
 * @param socket  Socket id of the connecting peer
 * @param addr    Network address of the connecting peer
 */
static void server_accept_peer(tcp_plugin_t *tcp, int socket,
				 struct in_addr *addr)
{
  int idx;

  os_thread_rwlock_wrlock(&tcp->peers_lock);

  /* Is the peer already regitered with this server ? */
  for (idx = 0; idx < EXA_MAX_NODES_NUMBER; idx++)
      if (!strcmp(tcp->peers[idx].ip_addr, os_inet_ntoa(*addr)))
      {
          tcp->peers[idx].sock = socket;
	  break;
      }

  os_thread_rwlock_unlock(&tcp->peers_lock);

  if (idx >= EXA_MAX_NODES_NUMBER)
  {
      /* The peer is not registered, send it an error message */
      exalog_error("Node with address %s is  not registered with this server",
		   os_inet_ntoa(*addr));
      close_socket(socket);
  }
}

/**
 * thread responsible for accpeting connection
 * it's a separate thread because we accept do some memory allocation and we
 * must avoid that in recv thread
 *
 * @param p net plugin info
 */
static void accept_thread(void *p)
{
  tcp_plugin_t *tcp = p;
  int newsockfd;
  struct sockaddr_in peer_address;
  int len;

  exalog_as(EXAMSG_NBD_SERVER_ID);

  while (tcp->accept_thread.run)
  {
      int err;

      len = sizeof(struct sockaddr_in);

      newsockfd = os_accept(tcp->accept_thread.socket,
                            (struct sockaddr *)&peer_address, &len);

      /* Continue if it is a false accept */
      if (newsockfd < 0)
	  continue;

      err = internal_setsock_opt(newsockfd, SOCK_FLAGS);

      if (err != EXA_SUCCESS)
      {
          exalog_error("Failed to setup socket %d: %s(%d)",
                       newsockfd, os_strerror(err), err);
          close_socket(newsockfd);
          continue;
      }

      /* Accept this peer if it was already registered */
      server_accept_peer(tcp, newsockfd, &peer_address.sin_addr);
  }
}

static void request_reset(struct pending_request *request)
{
  request->nb_readwrite = 0;
  request->io_desc = NULL;
  request->buffer = NULL;
}

/* send header and buffer if any
 *
 * return value:
 *   DATA_TRANSFER_ERROR     socket invalid or closed.
 *   DATA_TRANSFER_COMPLETE  if successfully transferred all pending data
 *                           (header and buffer if any)
 *   DATA_TRANSFER_PENDING   if some remaining data to transfer
 */
static int request_send(int fd, struct pending_request *request, nbd_tcp_t *nbd_tcp)
{
    int ret;

    if (request->nb_readwrite < NBD_HEADER_NET_SIZE)
    {
        do {
            ret = os_send(fd, (char *)request->io_desc + request->nb_readwrite,
                          NBD_HEADER_NET_SIZE - request->nb_readwrite);
        } while (ret == -EINTR);

        if (ret < 0)
            return DATA_TRANSFER_ERROR;

        request->nb_readwrite += ret;

        if (request->nb_readwrite < NBD_HEADER_NET_SIZE)
            return DATA_TRANSFER_PENDING;

            /* no buffer associated, so we can put immediately the header */
        if (request->io_desc->sector_nb == 0)
            return DATA_TRANSFER_COMPLETE;

        /* Buffer MUST exist as the caller requested to send buffer's data...
         * Having buffer == NULL would mean we want to send data, but caller does
         * not know which... */
        request->buffer = nbd_tcp->get_buffer(request->io_desc);
        EXA_ASSERT(request->buffer != NULL);

        return DATA_TRANSFER_PENDING;
    }

    do {
        ret = os_send(fd, request->buffer,
                      NBD_HEADER_NET_SIZE + (request->io_desc->sector_nb << 9) - request->nb_readwrite);
    } while (ret == -EINTR);

    if (ret < 0)
        return DATA_TRANSFER_ERROR;

    request->nb_readwrite += ret;
    request->buffer += ret;

    if (request->nb_readwrite < NBD_HEADER_NET_SIZE + (request->io_desc->sector_nb << 9))
        return DATA_TRANSFER_PENDING;

    return DATA_TRANSFER_COMPLETE;
}

/* receive header and buffer if any
 *
 * return value:
 *   DATA_TRANSFER_ERROR     socket invalid or closed.
 *   DATA_TRANSFER_COMPLETE  if successfully transferred all pending data
 *                           (header and buffer if any)
 *   DATA_TRANSFER_PENDING   if some remaining data to transfer
 */
static int request_recv(int fd, struct pending_request *request, nbd_tcp_t *nbd_tcp)
{
    int ret;

    if (request->nb_readwrite < NBD_HEADER_NET_SIZE)
    {
        do {
            ret = os_recv(fd, (char *)request->io_desc + request->nb_readwrite,
                          NBD_HEADER_NET_SIZE - request->nb_readwrite, 0);
        } while (ret == -EINTR);

        if (ret <= 0) /* 0 means peer disconnected */
            return DATA_TRANSFER_ERROR;

        request->nb_readwrite += ret;

        if (request->nb_readwrite < NBD_HEADER_NET_SIZE)
            return DATA_TRANSFER_PENDING;

            /* no buffer associated, so we can put immediately the header */
        if (request->io_desc->sector_nb == 0)
            return DATA_TRANSFER_COMPLETE;

        /* There MUST be a free buffer at this point or something went wrong */
        request->buffer = nbd_tcp->get_buffer(request->io_desc);
        EXA_ASSERT(request->buffer != NULL);

        return DATA_TRANSFER_PENDING;
    }

    do {
        ret = os_recv(fd, request->buffer,
                      NBD_HEADER_NET_SIZE + (request->io_desc->sector_nb << 9)
                      - request->nb_readwrite, 0);
    } while (ret == -EINTR);

    if (ret <= 0) /* 0 means peer disconnected */
        return DATA_TRANSFER_ERROR;

    request->nb_readwrite += ret;
    request->buffer += ret;

    if (request->nb_readwrite < NBD_HEADER_NET_SIZE + (request->io_desc->sector_nb << 9))
        return DATA_TRANSFER_PENDING;

    return DATA_TRANSFER_COMPLETE;
}

static void request_processed(struct pending_request *pending_req,
                              int client_id, nbd_tcp_t *nbd_tcp, int error)
{
    tcp_plugin_t *tcp = nbd_tcp->tcp;
    nbd_io_desc_t *io_desc = pending_req->io_desc;

    if (io_desc == NULL)
        return;

    if (nbd_tcp->end_sending)
        nbd_tcp->end_sending(io_desc, error);

    io_desc->buf = NULL;

    nbd_list_post(&tcp->send_list.free, io_desc, -1);

    request_reset(pending_req);
}

/* thread for asynchronously sending data for a peer or a server */
static void send_thread(void *p)
{
  nbd_tcp_t *nbd_tcp = p;
  tcp_plugin_t *tcp = nbd_tcp->tcp;
  struct pending_request pending_requests[EXA_MAX_NODES_NUMBER];
  struct pending_request *request = NULL;
  int i;
  int ret;
  fd_set fds;
  exa_select_handle_t *sh = exa_select_new_handle();
  int fd_act;
  bool active_sock;

  for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
    request_reset(&pending_requests[i]);

  while (tcp->send_thread.run)
  {
      FD_ZERO(&fds);

      /* if one node is added or deleted, this deletion or addition are
         effective after this */
      os_thread_rwlock_rdlock(&tcp->peers_lock);
      active_sock = false;
      for (i = 0; i <= tcp->last_peer_idx; i++)
      {
          request = &pending_requests[i];

	  fd_act = tcp->peers[i].sock;
	  if (fd_act < 0)
	  {
	      request_processed(request, i, nbd_tcp, -1);
	      continue;
	  }
          if (request->io_desc == NULL)
              request->io_desc = nbd_list_remove(&tcp->peers[i].send_list,
                                                NULL, LISTNOWAIT);
          if (request->io_desc != NULL)
          {
	      FD_SET(fd_act, &fds);
              active_sock = true;
          }
      }
      os_thread_rwlock_unlock(&tcp->peers_lock);

      if (active_sock)
	  ret = exa_select_out(sh, &fds);
      else
      {
	  os_sem_wait(&tcp->send_thread.semaphore);
          continue;
      }

      os_thread_rwlock_rdlock(&tcp->peers_lock);
      for (i = 0; i <= tcp->last_peer_idx; i++)
      {
          struct peer *peer = &tcp->peers[i];
	  if (pending_requests[i].io_desc != NULL
              /* Also check the socket is still valid because there is a race
               * with remove_peer function, thus the socket may be -1 even
               * if it was valid in the first part of loop. see bug #4581 and
               * #4607 */
	      && peer->sock >= 0 && FD_ISSET(peer->sock, &fds))
	  {
		  /* there is a pending io_desc */
		  request = &pending_requests[i];
		  /* send remaining data if any */
		  ret = request_send(peer->sock, request, nbd_tcp);
		  switch(ret)
		  {
		  case DATA_TRANSFER_COMPLETE:
                      request_processed(request, i, nbd_tcp, 0);
                      break;

		  case DATA_TRANSFER_ERROR:
                      exalog_error("Failed to sending data to '%s' id=%d "
                                   "socket=%d", peer->ip_addr,
                                   peer->node_id, peer->sock);
                      request_processed(request, i, nbd_tcp, -1);
                      close_socket(peer->sock);
                      peer->sock = -1;
                      break;

		  case DATA_TRANSFER_PENDING:
		    break;
		  }

          }
      }

      os_thread_rwlock_unlock(&tcp->peers_lock);
  }

  exa_select_delete_handle(sh);
}

/*
 * thread responsible for receiving data for a client or a server
 * note when we add peer, this peer is effectively added in the receive queue
 * only few second later due to the select timeout of 3 seconds
 * and there are the same problem for the deleteion of a peer
 */
static void receive_thread(void *p)
{
  struct nbd_root_list recv_list;
  nbd_tcp_t *nbd_tcp = p;
  tcp_plugin_t *tcp = nbd_tcp->tcp;
  struct pending_request pending_requests[EXA_MAX_NODES_NUMBER];
  nbd_io_desc_t *temp_io_desc;
  int i;
  int ret;
  int fd_act;
  exa_select_handle_t *sh = exa_select_new_handle();
  /* FIXME: handle the case when we have more than 1024 open file (limit of fd_set) */
  fd_set fds;

  nbd_init_root(EXA_MAX_NODES_NUMBER, sizeof(nbd_io_desc_t), &recv_list);

  for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
    request_reset(&pending_requests[i]);

  while (tcp->receive_thread.run)
  {
      FD_ZERO(&fds);
      /* if one node is added or deleted, this deletion or addition are effective after this */
      os_thread_rwlock_rdlock(&tcp->peers_lock);
      for (i = 0; i <= tcp->last_peer_idx; i++)
      {
	  fd_act = tcp->peers[i].sock;
	  if (fd_act < 0)
	  {
              temp_io_desc = pending_requests[i].io_desc;
              if (temp_io_desc != NULL)
              {
		  nbd_list_post(&recv_list.free,
				temp_io_desc, -1);
                  request_reset(&pending_requests[i]);
              }
	      continue;
	  }
	  FD_SET(fd_act,&fds);
      }
      os_thread_rwlock_unlock(&tcp->peers_lock);

      ret = exa_select_in(sh, &fds);
      if (ret == -EFAULT)
          continue;

      os_thread_rwlock_rdlock(&tcp->peers_lock);

      for (i = 0; i <= tcp->last_peer_idx; i++)
      {
          struct peer *peer = &tcp->peers[i];
	  if (peer->sock >= 0 && FD_ISSET(peer->sock ,&fds))
          {
              struct pending_request *request = &pending_requests[i];
              if (request->io_desc == NULL)
              {
                  request->io_desc = nbd_list_remove(&recv_list.free,
                                                    NULL, LISTNOWAIT);

                  if (request->io_desc == NULL)
                      continue;
              }

              ret = request_recv(peer->sock, request, nbd_tcp);
              if (ret == DATA_TRANSFER_PENDING)
                  continue;

              temp_io_desc = request->io_desc;
              request_reset(request);

              if (ret == DATA_TRANSFER_ERROR)
              {
                  nbd_list_post(&recv_list.free,
                                temp_io_desc, -1);

                  /* FIXME this should be an exalog_error but is debug for now
                   * because when stopping clientd, serverd close its socket
                   * by this place... TODO could be nice to indicate to serverd
                   * that the clientd is about to cut connections and thus it
                   * is normal to have its socket closed. */
                  exalog_debug("Failed to receive data from '%s' id=%d "
                               "socket=%d", peer->ip_addr,
                               peer->node_id, peer->sock);
                  close_socket(peer->sock);
                  peer->sock = -1;
                  continue;
              }

              /* the netplugin must set this id at reception */
              nbd_tcp->end_receiving(i, temp_io_desc, 0);
              temp_io_desc->buf = NULL;
              nbd_list_post(&recv_list.free, temp_io_desc, -1);
          }
      }

      os_thread_rwlock_unlock(&tcp->peers_lock);
  }

  nbd_close_root(&recv_list);

  exa_select_delete_handle(sh);
}

void tcp_send_data(struct nbd_tcp *nbd_tcp, exa_nodeid_t to, const nbd_io_desc_t *io)
{
    tcp_plugin_t *tcp = nbd_tcp->tcp;
    nbd_io_desc_t *io_desc = nbd_list_remove(&tcp->send_list.free, NULL, LISTWAIT);
    EXA_ASSERT(io_desc != NULL);

    *io_desc = *io;

    os_thread_rwlock_rdlock(&tcp->peers_lock);
    /* we send no more data to a removed connection */
    if (tcp->peers[to].sock < 0)
    {
        os_thread_rwlock_unlock(&tcp->peers_lock);

        if (nbd_tcp->end_sending)
            nbd_tcp->end_sending(io_desc, -NBD_ERR_NO_CONNECTION);

        nbd_list_post(&tcp->send_list.free, io_desc, -1);
        return;
    }

    nbd_list_post(&tcp->peers[to].send_list, io_desc, -1);

    os_sem_post(&tcp->send_thread.semaphore);

    os_thread_rwlock_unlock(&tcp->peers_lock);
}

static int client_connect_to_server(struct in_addr *inaddr,
                                    struct in_addr *local_addr)
{
  int sock;
  struct sockaddr_in serv_addr;
  struct sockaddr_in bind_addr;
  int retval;

  exalog_debug("Connecting to the server %s", os_inet_ntoa(*inaddr));

  sock = os_socket(PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
      exalog_error("Failed to create a socket: %s(%d)", os_strerror(-sock), sock);
      return -EXA_ERR_CREATE_SOCKET;
  }

  retval = internal_setsock_opt(sock, SOCK_FLAGS);
  if (retval != EXA_SUCCESS)
  {
      exalog_error("Failed to set socket options: %s(%d)",
                   os_strerror(-retval), retval);
      close_socket(sock);
      return retval;
  }

  /* FIXME The SO_SNDTIMEO and SO_RCVTIMEO specify the receiving or sending
   * timeouts until reporting an error.If an input or output function blocks for
   * this period of time, and data has been sent or received, the return value
   * of that function will be the amount of data transferred; if no data has
   * been transferred and the timeout has been reached then -1 is returned with
   * errno set to EAGAIN or EWOULDBLOCK just as if the socket was specified to
   * be nonblocking. If the timeout is set to zero (the default) then the
   * operation will never timeout.
   *
   * The question is if setting these parameters for connect changes the behavior
   * of this function or not as it is not strcitly an IO function ?
   */
  os_sock_set_timeouts(sock, 4000);

  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = local_addr->s_addr;
  bind_addr.sin_port = htons(0);

  retval = os_bind(sock,(struct sockaddr *)&bind_addr, sizeof(bind_addr));
  if (retval < 0)
  {
      exalog_error("Failed to bind to %s: %s(%d)",
                   os_inet_ntoa(bind_addr.sin_addr), os_strerror(-retval), retval);
      close_socket(sock);
      return retval;
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inaddr->s_addr;
  serv_addr.sin_port = htons(SERVERD_DATA_PORT);

  retval = os_connect(sock,(struct sockaddr *)  &serv_addr, sizeof(serv_addr));
  if (retval < 0)
  {
      exalog_error("Failed to connect to %s: %s(%d)", os_inet_ntoa(*inaddr),
                   os_strerror(-retval), retval);
      close_socket(sock);
      return -NBD_ERR_SERVER_REFUSED_CONNECTION;
  }

  /* FIXME don't forget to fix the code when bug 4607 is investigated */
#if 0
  os_sock_set_timeouts(sock, 0);
#endif

  return sock;
}

int tcp_connect_to_peer(nbd_tcp_t *nbd_tcp, exa_nodeid_t nid)
{
    tcp_plugin_t *tcp = nbd_tcp->tcp;
    struct peer *peer = &tcp->peers[nid];
    struct in_addr node_addr;
    int sock;

    /* FIXME I dislike the fact to lock the whole array just because
     * I change/access a field of the structure... maybe a lock per
     * entry would be better. */
    os_thread_rwlock_wrlock(&tcp->peers_lock);

    if (os_inet_aton(peer->ip_addr, &node_addr) == 0)
    {
        exalog_error("can't get IP address of %s", peer->ip_addr);
        return -NET_ERR_INVALID_HOST;
    }

    sock = client_connect_to_server(&node_addr, &tcp->data_addr);
    if (sock >= 0)
        peer->sock = sock;
    else
        peer->sock = -1;

    os_thread_rwlock_unlock(&tcp->peers_lock);

    return sock >= 0 ? EXA_SUCCESS : sock;
}

/* connect to the server and return connection id */
/* TODO: avoid ultiple connections between one server and one client */
int tcp_add_peer(exa_nodeid_t nid, const char *ip_addr, struct nbd_tcp *nbd_tcp)
{
    tcp_plugin_t *tcp = nbd_tcp->tcp;
    struct peer *peer;

    /* Check peer id is valid. This MUST be the case because we get it from
     * admind */
    EXA_ASSERT(EXA_NODEID_VALID(nid));

    peer = &tcp->peers[nid];

    os_thread_rwlock_wrlock(&tcp->peers_lock);

    if (EXA_NODEID_VALID(peer->node_id))
    {
        EXA_ASSERT_VERBOSE(peer->node_id == nid
                           && !strcmp(peer->ip_addr, ip_addr),
                "node_id %d (%s) provided is not the same than %d (%s) already"
                " registered", nid, ip_addr,
                peer->node_id, peer->ip_addr);

        os_thread_rwlock_unlock(&tcp->peers_lock);

        return EXA_SUCCESS;
    }

    peer->node_id = nid;

    if (nid > tcp->last_peer_idx)
        tcp->last_peer_idx = nid;

    os_strlcpy(peer->ip_addr, ip_addr, sizeof(peer->ip_addr));

    os_thread_rwlock_unlock(&tcp->peers_lock);

    return EXA_SUCCESS;
}

int tcp_remove_peer(uint64_t peer_id, struct nbd_tcp *nbd_tcp)
{
  int sock;
  nbd_io_desc_t *header;
  tcp_plugin_t *tcp = nbd_tcp->tcp;
  struct peer *peer = &tcp->peers[peer_id];

   if (!EXA_NODEID_VALID(peer->node_id))
       return -NBD_ERR_SNODE_NET_CLEANUP;

  peer->node_id    = EXA_NODEID_NONE;
  peer->ip_addr[0] = '\0';

  if (peer->node_id <= tcp->last_peer_idx)
  {
      /* Find the last valid clientd idx */
      int idx;
      for (idx = tcp->last_peer_idx; idx >= 0; idx--)
          if (tcp->peers[idx].node_id != EXA_NODEID_NONE)
              tcp->last_peer_idx = tcp->peers[idx].node_id;
  }

  os_thread_rwlock_wrlock(&tcp->peers_lock);
  sock = peer->sock;
  peer->sock = -1;

  /* remove waiting requests to be sent from the nbd_tcp list */
  /* FIXME This SHOULD NOT be done here...
   * The plugin should not drop nbd_io_desc_t when the connection is found broken
   * but should just return IO error to caller in the send_thread itself. */
  while ((header = nbd_list_remove(&peer->send_list, NULL, LISTNOWAIT)) != NULL)
      nbd_list_post(&peer->send_list.root->free, header, -1);

  os_thread_rwlock_unlock(&tcp->peers_lock);

  if (sock != -1)
    os_shutdown(sock, SHUT_RDWR);
  else
    return -NBD_ERR_NO_CONNECTION;
  /* we must avoid race with del_peer and other send_data , so we must get the semaphore */
  os_thread_rwlock_wrlock(&tcp->peers_lock);

  os_closesocket(sock);

  os_thread_rwlock_unlock(&tcp->peers_lock);
  /* this remove will have some result in more than 3-6 seconds due to the select timeout */
  return EXA_SUCCESS;
}


int tcp_start_listening(nbd_tcp_t *nbd_tcp)
{
    tcp_plugin_t *tcp = nbd_tcp->tcp;
    struct sockaddr_in serv_addr;
    int err, sock;

    sock = os_socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        exalog_error("cannot create socket: %s(%d)", exa_error_msg(sock), sock);
        return -EXA_ERR_CREATE_SOCKET;
    }

    /* bind a socket to SERVERD_DATA_PORT port and make it listen for incoming
     * connections */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = tcp->data_addr.s_addr;
    serv_addr.sin_port = htons(SERVERD_DATA_PORT);

    err = internal_setsock_opt(sock, SOCK_LISTEN_FLAGS);
    if (err != EXA_SUCCESS)
    {
        exalog_error("cannot set socket option");
        close_socket(sock);
        return err;
    }

    err = os_bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (err < 0)
    {
        exalog_error("cannot bind socket on %s: %s(%d)",
                     os_inet_ntoa(tcp->data_addr),
                     exa_error_msg(err), err);
        close_socket(sock);
        return -EXA_ERR_CREATE_SOCKET;
    }

    err = os_listen(sock, EXA_MAX_NODES_NUMBER);
    if (err < 0)
    {
        exalog_error("cannot listen on socket: %s(%d)", exa_error_msg(err), err);
        close_socket(sock);
        return -EXA_ERR_CREATE_SOCKET;
    }

    nbd_tcp->tcp->accept_thread.run = true;
    if (!exathread_create_named(&nbd_tcp->tcp->accept_thread.tid,
                NBD_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE_OF_THIS_PLUGIN,
                accept_thread, nbd_tcp->tcp, "servTcpAccPlugin"))
    {
        exalog_error("cannot create accept thread: %s(%d)", exa_error_msg(err), err);
        close_socket(sock);
        return -EXA_ERR_CREATE_SOCKET;
    }

    nbd_tcp->tcp->accept_thread.socket = sock;

    return EXA_SUCCESS;
}

void tcp_stop_listening(nbd_tcp_t *nbd_tcp)
{
    if (nbd_tcp == NULL || nbd_tcp->tcp == NULL)
        return;

    nbd_tcp->tcp->accept_thread.run = false;
    close_socket(nbd_tcp->tcp->accept_thread.socket);
    os_thread_join(nbd_tcp->tcp->accept_thread.tid);
}

static void stop_send_thread(tcp_plugin_t *tcp)
{
    tcp->send_thread.run = false;
    os_sem_post(&tcp->send_thread.semaphore);
    os_thread_join(tcp->send_thread.tid);
}

static bool spawn_send_thread(nbd_tcp_t *nbd_tcp)
{
    tcp_plugin_t *tcp = nbd_tcp->tcp;
    os_sem_init(&tcp->send_thread.semaphore, 0);

    tcp->send_thread.run = true;
    if (!exathread_create_named(&tcp->send_thread.tid,
                NBD_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE_OF_THIS_PLUGIN,
                send_thread, nbd_tcp, "TcpSndPlugin"))
    {
        exalog_error("Cannot create send thread.");

        tcp->send_thread.run = false;
        os_sem_destroy(&tcp->send_thread.semaphore);
        return false;
    }

    return true;
}

static void stop_receive_thread(tcp_plugin_t *tcp)
{
    tcp->receive_thread.run = false;
    os_thread_join(tcp->receive_thread.tid);
}

static bool spawn_receive_thread(nbd_tcp_t *nbd_tcp)
{
    tcp_plugin_t *tcp = nbd_tcp->tcp;
    tcp->receive_thread.run = true;

    if (!exathread_create_named(&tcp->receive_thread.tid,
                                NBD_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE_OF_THIS_PLUGIN,
                                receive_thread, nbd_tcp, "TcpRcvPlugin"))
    {
        exalog_error("Cannot create receive thread.");

        tcp->receive_thread.run = false;
        return false;
    }

    return true;
}

static void __cleanup_data(struct nbd_tcp *nbd_tcp)
{
    tcp_plugin_t *tcp = nbd_tcp->tcp;
    int i;

    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        if (tcp->peers[i].sock >= 0)
            close_socket(tcp->peers[i].sock);

    os_thread_rwlock_destroy(&tcp->peers_lock);

    os_net_cleanup();

    nbd_close_root(&tcp->send_list);

    os_free(tcp);
    nbd_tcp->tcp = NULL;
}

void cleanup_tcp(struct nbd_tcp *nbd_tcp)
{
    stop_receive_thread(nbd_tcp->tcp);

    stop_send_thread(nbd_tcp->tcp);

    __cleanup_data(nbd_tcp);
}

/**
 * tcp_init initialise internal data,
 * launch the thread of receive data
 * @param nbd_tcp info on the new instance that we will fill
 * @return EXA_SUCCESS or error
 */
int init_tcp(nbd_tcp_t *nbd_tcp, const char *hostname, const char *net_type,
             int num_receive_headers)
{
  tcp_plugin_t *tcp;
  int err, i;

  if (sscanf(net_type, "TCP=%d", &TCP_buffers) == 1)
    TCP_buffers = TCP_buffers * 1024;
  else
  {
      exalog_error("Bad network type '%s'", net_type);
      return -EINVAL;
  }

  tcp = os_malloc(sizeof(tcp_plugin_t));

  if (tcp == NULL)
    return -NBD_ERR_MALLOC_FAILED;

  nbd_tcp->tcp = tcp;

  os_thread_rwlock_init(&tcp->peers_lock);

  /* init queue of receivable headers */
  nbd_init_root(num_receive_headers, sizeof(nbd_io_desc_t), &tcp->send_list);

  for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
  {
      struct peer *peer = &tcp->peers[i];
      peer->sock       = -1;
      peer->node_id    = EXA_NODEID_NONE;
      peer->ip_addr[0] = '\0';
      nbd_init_list(&tcp->send_list, &peer->send_list);
  }
  tcp->last_peer_idx = 0;

  os_net_init();

  err = os_host_addr(hostname, &tcp->data_addr);
  if (err != 0)
  {
      exalog_error("Cannot resolve %s: %s (%d)\n", hostname,
                   exa_error_msg(err), err);
      __cleanup_data(nbd_tcp);
      return err;
  }

  if (!spawn_receive_thread(nbd_tcp))
  {
      __cleanup_data(nbd_tcp);
      return -NBD_ERR_THREAD_CREATION;
  }

  if (!spawn_send_thread(nbd_tcp))
  {
      stop_receive_thread(tcp);
      __cleanup_data(nbd_tcp);
      return -NBD_ERR_THREAD_CREATION;
  }

  return EXA_SUCCESS;
}

