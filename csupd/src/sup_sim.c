/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/*
 * Supervision Daemon simulation library.
 */

#include "sup_cluster.h"
#include "sup_ping.h"
#include "sup_debug.h"

#include "common/include/exa_constants.h"

#include "os/include/os_network.h"
#include "os/include/os_error.h"

#include <stdio.h>


/** Simulated instance address */
#define SIM_ADDR  "127.0.0.1"

/** Base port number for simulated instances */
#define SIM_BASE_PORT  29800

static short sim_port = -1;          /**< Simulation port */
static int sim_sock = -1;            /**< Simulation socket */

/* Fix the behaviour of connection resets on UDP sockets for Windows.
 * Windows incorrectly causes UDP sockets using recvfrom() to not work
 * correctly, returning a "connection reset" error when a sendto()
 * fails with an "ICMP port unreachable" response and preventing the
 * socket from using the recvfrom() in subsequent operations.
 *
 * See KB Q263823: http://support.microsoft.com/?scid=kb;en-us;263823
 */
#ifdef WIN32
#include <MSWSock.h> /* Definition of SIO_UDP_CONNRESET */

/**
 * Fix annoying connection resets on Windows when using a UDP socket.
 *
 * @param fd  Sock descriptor
 *
 * @return 0 if successful, -1 in case of error.
 */
static int win32_udp_connection_reset_fix(int fd)
{
    DWORD dwBytesReturned = 0;
    BOOL  bNewBehavior = false;
    DWORD status;

    status = WSAIoctl(fd, SIO_UDP_CONNRESET, &bNewBehavior,
                      sizeof(bNewBehavior), NULL, 0,
                      &dwBytesReturned, NULL, NULL);
    if (status != SOCKET_ERROR)
        return 0;
    else
        return -1;
}
#endif

/** Empty shell */
void sup_monitor_check(void) { }

/**
 * Empty shell.
 *
 * \param[in] gen    Generation number
 * \param[in] mship  Membership to deliver
 */
int
sup_deliver(sup_gen_t gen, const exa_nodeset_t *mship)
{
  return 0;
}

/**
 * Simulate sending a ping to all instances of Csupd.
 *
 * \param[in] cluster  Cluster
 * \param[in] view     View to send
 */
void
sup_send_ping(const sup_cluster_t *cluster, const sup_view_t *view)
{
  sup_ping_t ping;
  struct sockaddr_in addr;
  exa_nodeid_t node_id;
  int retval;

  addr.sin_family = AF_INET;
  os_inet_aton(SIM_ADDR, &addr.sin_addr);

  ping.sender = cluster->self->id;
  ping.incarnation = cluster->self->incarnation;
  sup_view_copy(&ping.view, view);

  __debug("sup_send_ping: ");
  sup_view_debug(&ping.view);

  EXA_ASSERT(sup_check_ping(&ping, 'S'));

  for (node_id = 0; node_id < view->num_seen; node_id++)
    {
      addr.sin_port = htons((short)(SIM_BASE_PORT + node_id));

      retval = os_sendto(sim_sock, (char *) &ping, sizeof(ping), 0,
                    	(struct sockaddr *) &addr, sizeof(addr));
      if (retval < 0)
	fprintf(stderr, "failed sending: %s\n", os_strerror(-retval));
    }
}

/**
 * Simulate receiving an event.
 *
 * \param[out] ping  Ping received
 *
 * \return true if a ping was received, false otherwise
 */
bool
sup_recv_ping(sup_ping_t *ping)
{
  fd_set rset;
  int r;
  struct sockaddr_in other_addr;
  socklen_t addr_size;
  struct timeval timeout;

  FD_ZERO(&rset);
  FD_SET(sim_sock, &rset);

  timeout.tv_sec  = ping_period;
  timeout.tv_usec = 0;

  do
    r = os_select(sim_sock + 1, &rset, NULL, NULL, &timeout);
  while (r == -EINTR && !do_ping);

  if (r < 0)
    {
      if (!do_ping)
	fprintf(stderr, "sup_recv_ping: select failed: %s\n", os_strerror(-r));
      return false;
    }

  /* The timeout has expired */
  if (r == 0)
  {
      do_ping = true;
      return false;
  }

  if (!FD_ISSET(sim_sock, &rset))
    {
      fprintf(stderr, "socket %d not in recv set\n", sim_sock);
      return false;
    }

  addr_size = sizeof(other_addr);
  do
      r = os_recvfrom(sim_sock, (char *) ping, sizeof(*ping), 0,
                      (struct sockaddr *) &other_addr, &addr_size);
  while (r == -EINTR);

  if (r < 0)
  {
      fprintf(stderr, "sup_recv_ping: recv failed: %s\n", os_strerror(-r));
      return false;
  }

  return true;
}

/**
 * Set up the messaging.
 *
 * \param[in] local_id  Local node's id
 *
 * \return true if successfull, false otherwise
 */
bool
sup_setup_messaging(exa_nodeid_t local_id)
{
  struct sockaddr_in addr;
  int reuse = 1;
  int retval;

  sim_port = SIM_BASE_PORT + local_id;

  os_net_init();

  sim_sock = os_socket(AF_INET, SOCK_DGRAM, 0);
  if (sim_sock < 0)
    {
      fprintf(stderr, "failed creating socket: %s\n", os_strerror(-sim_sock));
      return false;
    }

#ifdef WIN32
  win32_udp_connection_reset_fix(sim_sock);
#endif

  if (os_setsockopt(sim_sock, SOL_SOCKET, SO_REUSEADDR,
                    &reuse, sizeof(reuse)) < 0)
    {
      fprintf(stderr, "failed setting socket reuse option\n");
      exit(1);
    }

  addr.sin_family = AF_INET;
  if (!os_inet_aton(SIM_ADDR, &addr.sin_addr))
    {
      fprintf(stderr, "invalid address: %s\n", SIM_ADDR);
      exit(1);
    }
  addr.sin_port = htons(sim_port);

  retval = os_bind(sim_sock, (struct sockaddr *) &addr, sizeof(addr));
  if (retval < 0)
    {
      fprintf(stderr, "failed binding socket to %s:%hd: %s\n",
	      SIM_ADDR, sim_port, os_strerror(-retval));
      return false;
    }

  return true;
}

void
sup_cleanup_messaging(void)
{
  os_closesocket(sim_sock);
  sim_sock = -1;
  sim_port = -1;
  os_net_cleanup();
}

