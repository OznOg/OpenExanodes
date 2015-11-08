/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file network.c
 * \brief Network related functions for messaging daemon.
 *
 * This file implements all the low level network functions of the
 * cluster messaging daemon.
 * \sa examsgd.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>


#include "common/include/exa_error.h"
#include "common/include/exa_nodeset.h"
#include "common/include/exa_select.h"
#include "common/include/exa_socket.h"
#include "examsg/include/examsg.h"
#include "examsg/src/mailbox.h"
#include "log/include/log.h"
#include "os/include/os_error.h"
#include "os/include/os_network.h"
#include "os/include/os_random.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"
#include "os/include/strlcpy.h"

#include "iface.h"
#include "network.h"

#ifdef USE_YAOURT
#include <yaourt/yaourt.h>
#endif

/** Max number of sent network messages to remember for retransmission */
#define EXAMSG_NETMSG_BUFFER  511

#if EXAMSG_NETMSG_BUFFER > 32767
# error "Message counter is 16 bits"
#endif

#define mseq_far(a, b)  (abs((a) - (b)) > (uint16_t)-1 / 2 ? 1 : 0)

/** mseq_after(a,b) return true if a is after b */
#define mseq_after(a, b) \
  (((mseq_t)(a) - (mseq_t)(b) > 0 ? 1 : 0) ^ mseq_far((mseq_t)(a), (mseq_t)(b)))

/** mseq_before(a,b) return true if a is before b */
#define mseq_before(a, b) mseq_after((b), (a))

/** mseq_after(a,b) return true if a is after b, or if a == b */
#define mseq_after_eq(a, b) \
  (((mseq_t)(a) - (mseq_t)(b) >= 0 ? 1 : 0) ^ mseq_far((mseq_t)(a), (mseq_t)(b)))

/** mseq_before(a,b) return true if a is before b, or if a == b */
#define mseq_before_eq(a, b)  mseq_after_eq((b), (a))

/** mseq_dist return the distance between two mseq */
static unsigned int
mseq_dist(mseq_t a, mseq_t b)
{
  mseq_t _a = a, _b = b;

  /* makes sure _a is older than _b */
  if (!mseq_before(_a, _b))
    {
      _a = b;
      _b = a;
    }

  return  _a <= _b ? _b - _a : _b + (uint16_t)-1 - _a + 1;
}

/** Ping message */
EXAMSG_DCLMSG(ExamsgPing, struct {
    int seq;			/**< sequence number */
});

/** structure to handle the resend requests : we try to be smart and lazy
 *  in order to retransmit messages only if we cannot do otherwise.
 */
static struct retransmit_sched
  {
    mseq_t seq;           /**< seq of oldest message scheduled for retransmission */
    int is_sched;         /**< whether the request is scheduled or deprecated */
    os_thread_mutex_t lock; /**< lock on this structure */
  } ret_sched;

/** State of a node on the multicast network */
struct mcastnodestate
  {
    /** Node id */
    exa_nodeid_t id;

    /** Node name */
    char host[EXA_MAXSIZE_HOSTNAME + 1];

    /** Last msg received from this node. Used only by the receiver thread. */
    mseq_t count;

    /** Retransmit request to the 'host' node from another anonymous node.
	Used by both threads and thus protected by the lock. */
    mseq_t retransmit_req_count;

    /** The date at which we received the previous request.
	Used by both threads and thus protected by the lock. */
    struct timeval retransmit_req_date;

    /** Tells if the receiver thread already asked the sender thread to request
	a retransmission to this node (boolean).
	Use by both threads and thus must be protected by the lock. */
    int retransmit_request_in_progress_from_me;

    /** Node incarnation */
    unsigned short incarnation;

    /** Whether the node was seen up at some point */
    bool seen_up;

    /** Is this node currently fenced ? (boolean) */
    int fenced;

    /** Have we received at least one message? (boolean) */
    int count_initialized;
  };

/** Version of network protocol */
#define NET_PROTOCOL  2

/** Network message */
typedef struct ExamsgNetMsg ExamsgNetMsg;
struct ExamsgNetMsg
  {
    int protocol;              /**< Examsg protocol version, *MUST* be first */
    ExamsgMID mid;             /**< message id */

    uint16_t incarnation;      /**< incarnation */
    mseq_t count;              /**< message number */
    uint8_t flags;             /**< delivery flags */
    uint8_t to;                /**< recipient mailbox id */
    exa_nodeset_t dest_nodes;  /**< Destination nodes */

    uint16_t size;             /**< msg size */
    char msg[sizeof(Examsg)];  /**< message body */
  } __attribute__((packed));

/** Size of the network message header */
#define NETMSG_HEADER_SIZE   (sizeof(ExamsgNetMsg) - sizeof(Examsg))

/** Get the actual size of a network message */
#define NETMSG_SIZE(netmsg)  (NETMSG_HEADER_SIZE + (netmsg)->size)

/** internal flag: retransmit request */
#define EXAMSGF_RTRANS		0x40
#define IS_RTRANS(netmsg) ((netmsg)->flags & EXAMSGF_RTRANS)

/** Special message flag */
#define EXAMSGF_SPECIAL		0x20
#define IS_SPECIAL(netmsg) ((netmsg)->flags & EXAMSGF_SPECIAL)

#define IS_PING(netmsg) (IS_SPECIAL((netmsg)) \
    && ((ExamsgAny*)(netmsg)->msg)->type == EXAMSG_PING)

static os_thread_mutex_t lock;		/**< shared data mutex */

static unsigned short incarnation;      /**< Incarnation */

static exa_select_handle_t *sh = NULL;  /**< For memory-free select() */

/* network data */
static int net_sock = -1;               /**< Network socket */
static struct in_addr ifaddr_in;	/**< interface address */
static struct sockaddr_in mcast_addr;   /**< Multicast address */
static ExamsgNetID netid;               /**< Our network ID */
static const char *this_node;           /**< Our node name */
static const char *this_host;           /**< Our host name */

static int net_status = -ENETDOWN;      /**< Network status */
static os_thread_mutex_t net_status_lock;

/* node states */
static struct mcastnodestate mcaststate[EXA_MAX_NODES_NUMBER];
static mseq_t mcastcount;		/**< local message counter */
static int fence_all = false;		/**< Should we fence ourself ? */

/* network congestion */

/* Min and max backoff, in us */
#define BACKOFF_MIN  0
#define BACKOFF_MAX  80000

/* Gain and penalty for decreasing and increasing backoff, in % */
#define BACKOFF_GAIN	 0.2
#define BACKOFF_PENALTY  0.2

/* Delay in us (microseconds) to keep a retransmit request as "valid"
   XXX this should probably be set dynamically */
#define RETR_REQ_LIFESPAN  400000

/* Maximum delay before asking for a message retransmission, in us */
#define RETR_MAX_DELAY  10000

/* Min and max retransmission backoff, in us */
#define RETR_BACKOFF_MIN  10
#define RETR_BACKOFF_MAX  80000

static unsigned backoff;       /**< Delay between messages */
static unsigned retr_backoff;  /**< Delay before retransmitting messages */

/** last sent messages */
static ExamsgNetMsg sentbuf[EXAMSG_NETMSG_BUFFER];
static int last_sent_index = -1;

/** last received message */
static ExamsgNetMsg recvbuf;

/**
 * Wrapper for os_get_monotonic_time().
 * This simplifies the call and allows to hide the timespec -> timeval
 * transformation.
 *
 * \param[out] tv   where to store the time we ask for.
 *
 * \return 0 on success, negative error code otherwise (errno also set)
 */
static int
exa_gettimeofday(struct timeval *tv)
{
  struct timespec ts;
  int retval;

  /* os_get_monotonic_time() does not depend on the system time; this is
   * mandatory not to bug in case the administrator changes the system time */
  retval = os_get_monotonic_time(&ts);
  if (retval < 0)
    return retval;

  tv->tv_sec  = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;

  OS_ASSERT(TIMEVAL_IS_VALID(tv));

  return 0;
}

/**
 * Apply a penalty on a backoff.
 * Ensures the backoff doesn't get greather than BACKOFF_MAX.
 *
 * \param[in,out] backoff Backoff on which to apply the penalty
 * \param[in] penalty Penalty to apply
 */
static inline void
backoff_apply_penalty(unsigned *backoff, double penalty)
{
  unsigned delta, new_backoff;

  delta = (unsigned)(*backoff * penalty);
  if (delta == 0)
    delta++;

  new_backoff = *backoff + delta;
  if (new_backoff < *backoff || new_backoff > BACKOFF_MAX)
    new_backoff = BACKOFF_MAX;

  *backoff = new_backoff;
}

/**
 * Apply a gain on a backoff.
 * Ensures the backoff doesn't get less than BACKOFF_MIN.
 *
 * \param[in,out] backoff Backoff on which to apply the gain
 * \param[in] gain Gain to apply
 */
static inline void
backoff_apply_gain(unsigned *backoff, double gain)
{
  unsigned delta;

  delta = (unsigned)(*backoff * gain);
  if (delta == 0)
    delta++;

  *backoff = delta > *backoff ? 0 : *backoff - delta;
  if ((int)*backoff < BACKOFF_MIN)
    *backoff = BACKOFF_MIN;
}

/**
 * Set the network status.
 * Does nothing if the network status is actually unchanged.
 *
 * \param[in] status  Network status
 *
 * The network status can be up (0), down (-ENETDOWN), out of memory (-ENOMEM),
 * broken (-EINVAL) or firewalled (-EPERM).
 */
void
network_set_status(int status)
{
  os_thread_mutex_lock(&net_status_lock);
  if (status == net_status)
    {
      os_thread_mutex_unlock(&net_status_lock);
      return;
    }
  net_status = status;
  os_thread_mutex_unlock(&net_status_lock);

  switch (status)
    {
    case 0:
      exalog_info("Control network is UP");
      break;

    case -ENETDOWN:
      exalog_error("Control network is DOWN");
      break;

    case -ENOMEM:
      exalog_error("Control network is OUT OF MEMORY");
      break;

    case -EPERM:
      exalog_error("Control network is PROBABLY FIREWALLED");
      break;

    case -EINVAL:
      exalog_error("Control network is BROKEN (undergoing DHCP reconfiguration?)");
      break;

    default:
      EXA_ASSERT_VERBOSE(false, "invalid network status: %d", status);
    }
}

/**
 * Get the network status.
 *
 * \return 0 if the network is up, negative error code otherwise
 */
int
network_status(void)
{
  int status;

  os_thread_mutex_lock(&net_status_lock);
  status = net_status;
  os_thread_mutex_unlock(&net_status_lock);

  return status;
}

/**
 * Tell whether the network is manageable despite the specified error.
 *
 * \param[in] err  Error code
 *
 * \return true if manageable, false otherwise
 */
bool
network_manageable(int err)
{
  return (err == -ENETDOWN || err == -ENOMEM || err == -EPERM || err == -EINVAL
	  || err == 0);
}

/**
 * Wait until the network is up.
 * Should be called iff the network status is -ENETDOWN.
 *
 * \return 0 if the network is up, negative error code otherwise
 */
int
network_waitup(void)
{
  exalog_info("Waiting for control network...");
  return examsgIfaceConfig(net_sock, this_host, &ifaddr_in);
}

/**
 * Reset a multicast node state.
 *
 * \param node_state  Mcast node state
 */
static void
reset_node_state(struct mcastnodestate *node_state)
{
  /* All fields must be 0 or false initially */
  memset(node_state, 0, sizeof(*node_state));
  node_state->id = EXA_NODEID_NONE;
}

/** Convert a timeval to a scalar */
#define TIMEVAL_TO_SCALAR(tv)  ((unsigned)((tv)->tv_sec * 1000000 + (tv)->tv_usec))

/** \brief Network initialization.
 *
 * \param[in] nid          Network ID
 * \param[in] node_name    Node name
 * \param[in] hostname     Host name
 * \param[in] mgroup       Multicast ip address.
 * \param[in] mport        Communication port number.
 * \param[in] inca         Incarnation
 *
 * \return 0 on success, negative error code otherwise
 */
int
network_init(const exa_uuid_t *cluster_uuid, const char *node_name,
	      const char *hostname,  exa_nodeid_t nodeid,
	      const char *mgroup, unsigned short mport, unsigned short inca)
{
  exa_nodeid_t node;
  struct in_addr _mcastip;
  in_addr_t mcastip;
  struct sockaddr_in bind_addr;
  struct ip_mreq mcastreq;
  int reuse, broadcast;
  int retval;

  EXA_ASSERT(cluster_uuid);
  netid.cluster = *cluster_uuid;
  netid.node    = nodeid;

  exalog_trace("Examsg protocol is %d", NET_PROTOCOL);

#ifdef WITH_TRACE
  {
    char hex[EXA_NODESET_HEX_SIZE + 1];

    exa_nodeset_to_hex(EXAMSG_ALLHOSTS, hex);
    exalog_trace("ALLHOSTS is %s", hex);
  }
#endif

  EXA_ASSERT(node_name);
  this_node = node_name;
  this_host = hostname;

  EXA_ASSERT(inca > 0);
  incarnation = inca;
  exalog_trace("incarnation set to %u", incarnation);

  if (net_sock >= 0)
    {
      exalog_error("socket already exists");
      return -EEXIST;
    }

  os_thread_mutex_init(&net_status_lock);
  os_thread_mutex_init(&lock);
  os_thread_mutex_init(&ret_sched.lock);

  ret_sched.seq      = 0;
  ret_sched.is_sched = false;

  /* Max backoff at the beginning, we will have time to speed up later */
  backoff = BACKOFF_MAX;

  /* retransmission backoff is set to a quite large period compared to
   * the backoff in order to maximize the chance to get all retransmit
   * requests before resending */
  retr_backoff = RETR_BACKOFF_MIN;

  /* initialize random generator */
  os_random_init();

  /* multicast state */
  for (node = 0; node < EXA_MAX_NODES_NUMBER; node++)
    reset_node_state(&mcaststate[node]);

#ifdef USE_YAOURT
  mcastcount = yaourt_event_wait(EXAMSG_CMSGD_ID, "examsg_net_init");
#else
  mcastcount = 0;
#endif

  memset(&sentbuf, 0, sizeof(sentbuf));
  memset(&recvbuf, 0, sizeof(recvbuf));

  /* get multicast ip */
  retval = os_host_addr(mgroup, &_mcastip);
  if (retval != 0)
  {
      exalog_error("os_host_addr() failed");
      goto error;
  }
  mcastip = ntohl(_mcastip.s_addr);

  /* check if dest address is multicast */
  if (!IN_MULTICAST(mcastip) && mcastip != INADDR_BROADCAST)
    {
      exalog_error("%s is not multicast", mgroup);
      retval = -EINVAL;
      goto error;
    }

  /* Build multicast address */
  mcast_addr.sin_family = AF_INET;
  mcast_addr.sin_addr.s_addr = htonl(mcastip);
  mcast_addr.sin_port = htons(mport);

  /* Descriptor used in memory-free select() */
  sh = exa_select_new_handle();

  /* create socket */
  net_sock = os_socket(AF_INET, SOCK_DGRAM, 0);
  if (net_sock < 0)
    {
      exalog_error("socket creation failed");
      return net_sock;
    }

#ifdef USE_EXA_COMMON_KMODULE
  if (exa_socket_set_atomic(net_sock))
    {
      exalog_error("failed to configure socket in no IO mode: %s", os_strerror(-errno));
      retval = -errno;
      goto error;
    }
#endif

  /* set reuse addr option */
  reuse = 1;
  retval = os_setsockopt(net_sock, SOL_SOCKET, SO_REUSEADDR,
                         &reuse, sizeof(reuse));
  if (retval < 0)
    {
      exalog_error("socket set reuse addr option failed");
      goto error;
    }

  if (mcastip == INADDR_BROADCAST)
    {
      exalog_info("Using broadcast address");

      /* set broadcast options */
      broadcast = 1;
      retval = os_setsockopt(net_sock, SOL_SOCKET, SO_BROADCAST,
			     &broadcast, sizeof(broadcast));
      if (retval < 0)
	  goto error;
    }
  else
    {
      exalog_info("Using multicast address %s", mgroup);

      /* Configure interface for multicast */
      retval = examsgIfaceConfig(net_sock, this_host, &ifaddr_in);
      if (retval < 0)
	  goto error;

      network_set_status(0);

      /* Configure socket for multicast. No need to set its TTL, as the
	 multicast TTL is 1 by default */
      retval = os_setsockopt(net_sock, IPPROTO_IP, IP_MULTICAST_IF,
                             &ifaddr_in, sizeof(ifaddr_in));
      if (retval < 0)
	{
	  exalog_error("set of outbound multicast traffic");
	  goto error;
	}

      mcastreq.imr_multiaddr.s_addr = htonl(mcastip);
      mcastreq.imr_interface = ifaddr_in;
      retval = os_setsockopt(net_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			     &mcastreq, sizeof(mcastreq));
      if (retval < 0)
	{
	  exalog_error("socket multicast options failed");
	  goto error;
	}
    }

  /* bind */
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind_addr.sin_port = htons(mport);
  retval = os_bind(net_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
  if (retval < 0)
    {
      exalog_error("socket bind failed");
      goto error;
    }

  return 0;

 error:

  if (net_sock >= 0)
    {
      os_closesocket(net_sock);
      net_sock = -1;
    }

  exa_select_delete_handle(sh);
  sh = NULL;

  return retval;
}

/**
 * Called on exit. Performs cleanup.
 */
void
network_exit(void)
{
  if (net_sock >= 0)
    {
      os_closesocket(net_sock);
      net_sock = -1;
    }

  exa_select_delete_handle(sh);
  sh = NULL;
}

/** \brief fence a node.
 * Careful Lock on mcaststate structure MUST be held by caller
 *
 * \param[in] m the mcaststate entry for the node to be fenced
 */
static void
network_fence(struct mcastnodestate *m)
{
  EXA_ASSERT(m);
  EXA_ASSERT(m->id != EXA_NODEID_NONE);

  exalog_info("fencing node %u", m->id);

  m->count_initialized = false;

  if (m->id == netid.node)
    fence_all = true;
  else
    m->fenced = true;

  m->seen_up = false;
}

/**
 * Add a node in the knwon nodes so that we can receive messages
 * from this node.
 *
 * \param[in] id    Node id
 * \param[in] name  Node name
 *
 * \return 0 on success, a negative error code otherwise
 */
int
network_add_node(exa_nodeid_t id, const char *name)
{
  struct mcastnodestate *m;

  exalog_debug("adding node %u:`%s'", id, name);

  os_thread_mutex_lock(&lock);

  m = &mcaststate[id];
  if (m->id != EXA_NODEID_NONE)
  {
      os_thread_mutex_unlock(&lock);
      return -EEXIST;
  }

  reset_node_state(m);
  m->id = id;

  /* Reset backoff to max in order not to flood the new comer
   * and/or handle slow new node */
  backoff = BACKOFF_MAX;

  /* New nodes are fenced (all but self) */
  if (netid.node != id)
    network_fence(m);
  else
    {
      /* The local node must set its incarnation right away, because otherwise,
       * when receiving its own messages, the incarnation carried by the
       * messages would be different from the local node's own state incarnation
       * and thus the message would be fenced */
      m->incarnation = incarnation;
    }

  os_thread_mutex_unlock(&lock);

  return 0;
}

/**
 * Remove a node from the known nodes.
 *
 * \param[in] id  Id of node to remove
 *
 * \return 0 on success, a negative error code otherwise
 */
int
network_del_node(exa_nodeid_t id)
{
  struct mcastnodestate *m;
  int err = 0;

  exalog_debug("removing node %u", id);

  os_thread_mutex_lock(&lock);

  m = &mcaststate[id];
  if (m->id == EXA_NODEID_NONE)
    err = -ENOENT;
  else
    reset_node_state(m);

  os_thread_mutex_unlock(&lock);

  return err;
}

/**
 * Mark a node has having been seen UP.
 *
 * \param[in] id  Node id
 */
void
network_saw_node_up(exa_nodeid_t id)
{
  struct mcastnodestate *m;

  exalog_trace("saw node %u UP", id);

  os_thread_mutex_lock(&lock);
  m = &mcaststate[id];
  EXA_ASSERT(m->id != EXA_NODEID_NONE);
  m->seen_up = true;
  os_thread_mutex_unlock(&lock);
}

/**
 * Tell whether a node was seen UP.
 *
 * \param[in] id  Node id
 *
 * \return true if the node was seen up, false otherwise
 */
bool
network_node_seen_up(exa_nodeid_t id)
{
  struct mcastnodestate *m;

  os_thread_mutex_lock(&lock);
  m = &mcaststate[id];
  EXA_ASSERT(m->id != EXA_NODEID_NONE);
  os_thread_mutex_unlock(&lock);

  return m->seen_up;
}

/**
 * Handle a fencing request (fence or unfence).
 *
 * \param[in] req  Message requesting the [un]fencing.
 */
void
network_handle_fence_request(const examsgd_fencing_req_t *req)
{
  exa_nodeid_t id;

  switch (req->order)
    {
    case FENCE:
      exa_nodeset_foreach(&req->node_set, id)
	{
	  exalog_debug("FENCE %d", id);
	  /* Only fence a node if it was seen up before, otherwise we
	   * may end up fencing nodes that come late and thus are seen
	   * down before being ever seen up, which will deem them as
	   * needing reboot whereas it is not actually needed */
	  /* FIXME is it really the role of examsg to handle need
	   * reboot ? */
	  if (network_node_seen_up(id))
	    {
		os_thread_mutex_lock(&lock);

		exalog_trace("node %u seen UP before, fencing", id);
		network_fence(&mcaststate[id]);

		os_thread_mutex_unlock(&lock);
	    }
	  else
	    exalog_trace("node %d never was UP, not fencing", id);
	}
      break;

    case UNFENCE:
      exa_nodeset_foreach(&req->node_set, id)
	{
	  exalog_debug("UNFENCE %d", id);
	  network_saw_node_up(id);
	}
      break;

    default:
      exalog_debug("unknown fencing op: %d", req->order);
    }
}

/**
 * Send a network message over the network socket.
 *
 * The network protocol is automatically set.
 *
 * \param     msg   Network message to send
 * \param[in] size  Size of message in bytes
 *
 * \return 0 if successfull, negative error code otherwise
 */
static int
sock_send(ExamsgNetMsg *msg, size_t size)
{
  int status = network_status();
  ssize_t n;

  if (status != 0)
    return status;

  msg->protocol = NET_PROTOCOL;
  n = os_sendto(net_sock, msg, size, 0, (struct sockaddr *)&mcast_addr,
	        sizeof(mcast_addr));

  if (n < 0)
    {
      if (n == -ENODEV || n == -ENOBUFS)
	{
	  network_set_status(-ENETDOWN);
	  return -ENETDOWN;
	}
      else if (n == -ENOMEM || n == -EPERM || n == -EINVAL)
	{
	  network_set_status(n);
	  return n;
	}
    }

  return n == size ? 0 : n;
}

/*
 * This sends a message on the network that is not stamped with a sequence
 * number. The message will bypass "ordered" messages so the sender MUST NOT
 * rely on the ordering of messages sent with this function. There is no
 * guaranty that the message will not be lost, so the sender MUST also be able
 * to tolerate lost messages.
 *
 * \param[in] mid	Message id.
 * \param[in] msg	Message.
 * \param[in] msgsize	size of message to send.
 *
 * \return 0 if successfull, a negative error code otherwise
 */
int
network_special_send(const ExamsgMID *mid, const Examsg *msg, size_t msgsize)
{
  ExamsgNetMsg netmsg;
  size_t msgbytes;
  int n;

  memset(&netmsg.mid, 0, sizeof(netmsg.mid));
  /* encode message */
  exa_nodeset_copy(&netmsg.dest_nodes, EXAMSG_ALLHOSTS);

  strlcpy(netmsg.mid.host, this_node, sizeof(netmsg.mid.host));
  uuid_copy(&netmsg.mid.netid.cluster, &netid.cluster);

  netmsg.mid.netid.node = netid.node;

  netmsg.mid.id  = mid->id;

  netmsg.incarnation = incarnation;
  netmsg.count       = 0; /* netmsg.count is set to 0 because it is ignored anyway */
  netmsg.to          = mid->id;
  netmsg.flags       = EXAMSGF_SPECIAL;
  netmsg.size        = msgsize;

  EXA_ASSERT(msgsize <= sizeof(netmsg.msg));
  memcpy(netmsg.msg, msg, msgsize);
  msgbytes = NETMSG_SIZE(&netmsg);

  /* send message */
  os_thread_mutex_lock(&lock);

#ifdef WITH_TRACE
  {
    char hex_nodes[EXA_NODESET_HEX_SIZE + 1];

    exa_nodeset_to_hex(&netmsg.dest_nodes, hex_nodes);
    exalog_trace("sending Special message from `%s' to `%s@%s'",
		 examsgIdToName(mid->id),
		 examsgIdToName(netmsg.to),
		 hex_nodes);
  }
#endif

  n = sock_send(&netmsg, msgbytes);
  if (n < 0)
    {
      /* Don't issue an error if failing while the network is not ok: failing
       * then is "normal" and an error has been logged when the network was
       * detected as not being ok */
      if (!network_manageable(n))
	exalog_error("cannot send %" PRIzd " bytes: %s\n", msgbytes, strerror(-n));
    }

  os_thread_mutex_unlock(&lock);

  return n;
}

/**
 * Send a ping to other hosts.
 * Doesn't send anything if there is actually no other hosts.
 */
void
send_ping(void)
{
  ExamsgPing msg_ping;
  ExamsgMID mid;

  exalog_trace("sending ping");

  memset(&msg_ping, 0, sizeof(msg_ping));

  msg_ping.any.type = EXAMSG_PING;
  msg_ping.seq      = mcastcount;

  memset(&mid, 0, sizeof(mid));
  mid.id = EXAMSG_CMSGD_ID;

  network_special_send(&mid, (const Examsg*)&msg_ping, sizeof(msg_ping));
}

/** \brief Send a network message
 *
 * \param[in] mid	Message id.
 * \param[in] msg	Message.
 *
 * \return 0 if successfull, a negative error code otherwise
 */
int
network_send(const ExamsgMID *mid, const ExamsgNetRqst *msg)
{
  static struct timeval last;
  struct timeval now, elapsed_tv;
  unsigned elapsed;

  ExamsgNetMsg netmsg;
  size_t msgbytes;
  int n;

  memset(&netmsg.mid, 0, sizeof(netmsg.mid));

  /* apply backoff timer */
  if (exa_gettimeofday(&now))
    return -errno;

  /* Apply backoff */
  if (backoff > 0)
    {
      elapsed_tv = os_timeval_diff(&now, &last);

      OS_ASSERT(TIMEVAL_IS_VALID(&elapsed_tv));

      elapsed = TIMEVAL_TO_SCALAR(&elapsed_tv);
      if (elapsed < backoff)
	{
	  unsigned delay = backoff - elapsed;

	  if ((int)delay >= BACKOFF_MIN)
	    {
	      exalog_debug("delaying send for %u us", delay);
	      os_microsleep(delay);

	      if (exa_gettimeofday(&now))
		return -errno;
	    }
	}
      else
	exalog_trace("no delay: %u us elapsed", elapsed);
    }

  /* encode message */
  exa_nodeset_copy(&netmsg.dest_nodes, &msg->dest_nodes);
  if (exa_nodeset_is_empty(&netmsg.dest_nodes))
    return msg->size;

  strlcpy(netmsg.mid.host, this_node, sizeof(netmsg.mid.host));
  uuid_copy(&netmsg.mid.netid.cluster, &netid.cluster);
  netmsg.mid.netid.node = netid.node;
  netmsg.mid.id = mid->id;
  netmsg.incarnation = incarnation;

  mcastcount++;

  netmsg.count = mcastcount;
  netmsg.to = msg->to;
  netmsg.flags = msg->flags;
  netmsg.size = msg->size;

  EXA_ASSERT(msg->size <= sizeof(netmsg.msg));
  memcpy(netmsg.msg, msg->msg, msg->size);
  msgbytes = NETMSG_SIZE(&netmsg);

  /* send message */
  os_thread_mutex_lock(&lock);

#ifdef WITH_TRACE
  {
    char hex_nodes[EXA_NODESET_HEX_SIZE + 1];

    exa_nodeset_to_hex(&netmsg.dest_nodes, hex_nodes);
    exalog_trace("sending message %u:%d from `%s' to `%s@%s'",
		 incarnation, mcastcount,
		 examsgIdToName(mid->id),
		 examsgIdToName(netmsg.to),
		 hex_nodes);
  }
#endif

  n = sock_send(&netmsg, msgbytes);
  if (n < 0 && network_manageable(n))
    {
      os_thread_mutex_unlock(&lock);
      return n;
    }

  EXA_ASSERT_VERBOSE(n == 0, "failed sending %" PRIzd " bytes: %s", msgbytes,
		     strerror(-n));

  /* increment message counter and save message */
  last_sent_index = (last_sent_index + 1) % EXAMSG_NETMSG_BUFFER;
  memcpy(&sentbuf[last_sent_index], &netmsg, msgbytes);

  /* update backoff timer */
  last = now;

  backoff_apply_gain(&backoff, BACKOFF_GAIN);
  exalog_trace("backoff timer set to %u us", backoff);

  os_thread_mutex_unlock(&lock);

  return 0;
}

/**
 * Schedule a retransmission request.
 * The retransmission request is forwarded to the sender thread.
 *
 * \param     mh           Examsg handle
 * \param[in] wanted_seq   Seq of oldest message requested for retransmission
 * \param[in] src_node_id  Id of requester
 */
static void
schedule_retransmission(ExamsgHandle mh, mseq_t wanted_seq,
			exa_nodeid_t src_node_id)
{
  ExamsgRetransmit msg_retransmit;
  int ret;

  os_thread_mutex_lock(&ret_sched.lock);

  /* If an older message is already scheduled, drop the request */
  if (ret_sched.is_sched && mseq_after_eq(wanted_seq, ret_sched.seq))
    {
      exalog_trace("dropping retransmission req %d from %u",
		   wanted_seq, src_node_id);

      os_thread_mutex_unlock(&ret_sched.lock);
      return;
    }

  /* No retransmission is scheduled yet or it is not starting from an
   * old enough message => schedule another request */
  exalog_trace("scheduling retransmission of message %d as requested by %u",
	       wanted_seq, src_node_id);

  ret_sched.seq = wanted_seq;
  ret_sched.is_sched = true;

  os_thread_mutex_unlock(&ret_sched.lock);

  msg_retransmit.any.type = EXAMSG_RETRANSMIT;
  msg_retransmit.count = wanted_seq;

  ret = examsgSend(mh, EXAMSG_CMSGD_ID, EXAMSG_LOCALHOST, &msg_retransmit,
		   sizeof(msg_retransmit));
  EXA_ASSERT(ret == sizeof(msg_retransmit));
}

/**
 * Update the retransmission info of a node.
 *
 * \param[in]     wanted_seq  Seq requested for retransmission
 * \param[in,out] requestee   State of requestee
 */
static void
update_retransmission_info(mseq_t wanted_seq, struct mcastnodestate *requestee)
{
  os_thread_mutex_lock(&lock);

  /* If I want a retransmit and this other node want something
     compatible with my request */
  if (requestee->retransmit_request_in_progress_from_me
      && mseq_before_eq(wanted_seq, requestee->count + 1))
    {
      int ret = exa_gettimeofday(&requestee->retransmit_req_date);
      EXA_ASSERT(ret == 0);
      requestee->retransmit_req_count = wanted_seq;
    }

  os_thread_mutex_unlock(&lock);
}

static void
send_retransmit_req(ExamsgHandle mh, exa_nodeid_t id, mseq_t missed)
{
/* Lost message. Forward the request to the sender thread */
  ExamsgRetransmitReq msg_retransmit_req;
  int ret;

  msg_retransmit_req.any.type = EXAMSG_RETRANSMIT_REQ;
  msg_retransmit_req.node_id  = id;
  msg_retransmit_req.count    = missed;


  ret = examsgSend(mh, EXAMSG_CMSGD_ID, EXAMSG_LOCALHOST,
      &msg_retransmit_req, sizeof(msg_retransmit_req));
  EXA_ASSERT(ret == sizeof(msg_retransmit_req));
}

/**
 * Process a retransmission request.
 *
 * \param     mh           Examsg handle
 * \param[in] src_node_id  Id of request sender
 * \param[in] netmsg       Retransmission request message
 */
static void
process_retransmission_request(ExamsgHandle mh, exa_nodeid_t src_node_id,
			       const ExamsgNetMsg *netmsg)
{
  mseq_t wanted_seq = *(mseq_t *)netmsg->msg;
  exa_nodeid_t requestee_id;
  struct mcastnodestate *requestee_state;

  os_thread_mutex_lock(&lock);

  requestee_id = exa_nodeset_first(&netmsg->dest_nodes);
  requestee_state = &mcaststate[requestee_id];
  if (requestee_state == NULL)
    {
      /* We cannot add dynamically the node in the hashtable because we don't
       * know what is its "count" (seq). We can safely ignore the request
       * because we will receive sooner or later a good message and we will
       * create the entry in the hashtable. And then we will be able to handle
       * retransmission requests
       */
      exalog_debug("Got a retransmission request of msg #%d from %u:`%s'"
		   " for unknown node %u. Ignored the request",
		   netmsg->count, src_node_id, netmsg->mid.host, requestee_id);

      os_thread_mutex_unlock(&lock);
      return;
    }

  os_thread_mutex_unlock(&lock);

  exalog_debug("node %u:`%s' want a retransmission of message %d by %u",
	       src_node_id, netmsg->mid.host, wanted_seq, requestee_id);

  /* Depending on whether the retransmission request is for us or not,
   * handle it (by "scheduling" the retransmission) or just update the
   * sender's state */
  if (requestee_id == netid.node)
    schedule_retransmission(mh, wanted_seq, src_node_id);
  else
    update_retransmission_info(wanted_seq, requestee_state);
}


/** \brief Check if a network message has been missed.
 *
 * As a side effect, this function updates message count for the sender.
 * ^^^ FIXME: THIS IS REALLY WRONG !
 *
 * \param[in] count	Sender count.
 * \param     m         Source node state
 *
 * \return
 *	\li 0 if message is next in sequence and should be processed,
 *	\li 1 if message is old and should be discarded or a message has
 *	been lost (in both cases, nothing to process),
 *	\li -1 on fatal errors.
 */
static int
checkseq(ExamsgHandle mh, mseq_t count, struct mcastnodestate *m)
{
  os_thread_mutex_lock(&lock);

  /* next in sequence ? */
  if (mseq_dist(count, m->count) == 1
      && mseq_before(m->count, count))
    {
      m->count++;
      m->retransmit_request_in_progress_from_me = false;
      os_thread_mutex_unlock(&lock);
      return 0;
    }

  /* old one, retransmitted */
  if (mseq_before(count, m->count + 1))
    {
      os_thread_mutex_unlock(&lock);
      return 1;
    }

  /* lost message ! */
  exalog_debug("lost message from %u: count=%d != ref=%d",
	       m->id, count, m->count);

  /* if the lost message is recoverable */
  if (mseq_before(count, m->count + EXAMSG_NETMSG_BUFFER))
    {
      /* if the request has not already been sent the sender thread */
      if (!m->retransmit_request_in_progress_from_me)
	{
	  exa_nodeid_t id = m->id;
	  mseq_t missed = (mseq_t)(m->count + 1);
	  m->retransmit_request_in_progress_from_me = true;

	  os_thread_mutex_unlock(&lock);

	  send_retransmit_req(mh, id, missed);
	  return 1;
	}

      os_thread_mutex_unlock(&lock);

      return 1;
    }

  exalog_error("too many messages lost, aborting");
  EXA_ASSERT_VERBOSE(false, "lost too many messages from %u, aborting. count=%d",
		     m->id, count);

  os_thread_mutex_unlock(&lock);

  return -1;
}

/**
 * Check validity of a network message.
 *
 * \param[in] netmsg  Received network message
 * \param[in] size    Size received
 *
 * \return true if valid, false otherwise
 */
static bool
network_msg_valid(const ExamsgNetMsg *netmsg, int size)
{
  exa_uuid_t uuid;

  if (size < NETMSG_HEADER_SIZE)
    {
      exalog_trace("Dropping message, smaller than header:"
		   " received=%d header=%" PRIzu, size, NETMSG_HEADER_SIZE);
      return false;
    }

  if (netmsg->protocol != NET_PROTOCOL)
    return false;

  uuid_copy(&uuid, &netmsg->mid.netid.cluster);
  if (!uuid_is_equal(&uuid, &netid.cluster))
    {
      exalog_trace("filtered bad cid `" UUID_FMT "' from `%s'",
		   UUID_VAL(&uuid), netmsg->mid.host);
      return false;
    }

  if (netmsg->size > sizeof(netmsg->msg))
    {
      exalog_trace("Dropping message, advertised size too big:"
		   " size=%u max=%" PRIzu, netmsg->size, sizeof(netmsg->msg));
      return false;
    }

  if (size != NETMSG_SIZE(netmsg))
    {
      exalog_trace("Dropping message, bad advertised size:"
		   " received=%d != header=%" PRIzu " + advertised=%u",
		   size, NETMSG_HEADER_SIZE, netmsg->size);
      return false;
    }

  EXA_ASSERT_VERBOSE(!exa_nodeset_is_empty(&netmsg->dest_nodes),
		     "Receive a message for localhost from the network");

  return true;
}

/** \brief Receive a network message.
 *
 * \param[in] mh	ExamshHandle.
 * \param[in] mid	Message id.
 * \param[out] msg	Message buffer.
 * \param[out] nbytes	Size of message \a buffer.
 * \param[out] to	Final recipent.
 *
 * \return 1 if we received one message, 0 if we didn't receive a message
 * and a negative error code otherwise.
 */
int
network_recv(ExamsgHandle mh, ExamsgMID *mid, char **msg, size_t *nbytes, int *to)
{
  ExamsgNetMsg *netmsg = &recvbuf;
  exa_nodeid_t src_node_id;
  ExamsgType msg_type;
  struct mcastnodestate *src_state;
  int n;
  fd_set rset;

#ifdef USE_YAOURT__0
  if (yaourt_event_wait(EXAMSG_CMSGD_ID, "examsg_net_recv") == 1)
    {
      errno = ENOMEM;
      network_set_status(-errno);
      return -errno;
    }
#endif

  /* We *have* to have a timeout. Fortunately (sort of), exa_select_in() has
   * an implicit timeout of 1/2 second. Unfortunately, exa_select_in() does
   * not distinguish intr from timeout, but it doesn't matter much here */
  FD_ZERO(&rset);
  FD_SET(net_sock, &rset);
  n = exa_select_in(sh, net_sock + 1, &rset);
  if (n == -EFAULT) /* Implicit exa_select_in timeout ; see doxygen */
    return 0;

  if (n < 0)
  {
      exalog_error("wait for data failed: %s (%d)", exa_error_msg(n), n);
      return n;
  }

  /* read new message */
  n = os_recvfrom(net_sock, (char *)netmsg, sizeof(*netmsg), 0, NULL, NULL);
  if (n == 0)
    return 0;

  if (n < 0)
    {
      if (n == -ENODEV)
	{
	  network_set_status(-ENETDOWN);
	  return -ENETDOWN;
	}
      else if (n == -ENOMEM || n == -EPERM || n == -EINVAL)
	{
	  network_set_status(n);
	  return n;
	}
      else
	return n;
    }

  if (!network_msg_valid(netmsg, n))
    return 0;

#ifdef USE_YAOURT
  if (!(netmsg->flags & EXAMSGF_SPECIAL)
      && yaourt_event_wait(EXAMSG_CMSGD_ID, "network_Recv %s %d",
			   netmsg->mid.host, netmsg->count))
    {
      exalog_error("Yaourt: network_Recv dropped message with seq %d from %s",
		   netmsg->count, netmsg->mid.host);
      return 0;
    }
#endif

  src_node_id = netmsg->mid.netid.node;
  msg_type = ((ExamsgAny *)netmsg->msg)->type;

#ifdef WITH_TRACE
  {
    char hex_nodes[EXA_NODESET_HEX_SIZE + 1];

    exa_nodeset_to_hex(&netmsg->dest_nodes, hex_nodes);
    exalog_trace("received message %u:%d of type %s from %u:`%s' to %s",
		 netmsg->incarnation, netmsg->count,
		 examsgTypeName(((ExamsgAny*)netmsg->msg)->type),
		 src_node_id, netmsg->mid.host, hex_nodes);
  }
#endif

  /* check where the message comes from and if we have to care about it */
  os_thread_mutex_lock(&lock);
  src_state = &mcaststate[src_node_id];
  os_thread_mutex_unlock(&lock);

  if (src_state->id == EXA_NODEID_NONE)
    {
      exalog_trace("spurious message %d:'%s' from %u:'%s'",
		   msg_type, examsgTypeName(msg_type),
		   src_node_id, netmsg->mid.host);
      return 0;
    }

  EXA_ASSERT_VERBOSE(netmsg->incarnation > 0,
		     "message has invalid incarnation: %u", netmsg->incarnation);

  /* If the received incarnation differs from the locally maintained
   * incarnation of the sender node, it means the sender have rebooted and we
   * just received its first message after its reboot. As the numbering
   * sequence was broken (due to node reboot or Exanodes restart), the node
   * must have been marked fenced by upper layer before being unfenced or not
   * having been seen at all. */
  if (netmsg->incarnation != src_state->incarnation
      && (src_state->fenced || !src_state->seen_up))
    {
      exalog_trace("updating incarnation for %u:'%s': %u -> %u",
		   src_node_id, netmsg->mid.host,
		   src_state->incarnation, netmsg->incarnation);

      exalog_info("%s node %u:'%s' incarnation %u -> %u",
	  src_state->fenced ? "unfencing" : "re-engaging", src_node_id,
	  netmsg->mid.host, src_state->incarnation, netmsg->incarnation);

      src_state->incarnation = netmsg->incarnation;

      src_state->fenced = false;
      src_state->count_initialized = false;
    }

  /* if node is either fenced or changed incarnation without having
   * been fenced, we just throw it away */
  if (src_state->fenced || fence_all
      || netmsg->incarnation != src_state->incarnation)
    {
      exalog_trace("fenced a message from %u:`%s'", src_node_id,
		   netmsg->mid.host);
      return 0;
    }

  /* Now on, source is not fenced, examine message */

  if (!src_state->count_initialized)
    {
      /* The sequence can only be initialized by an EXAMSG_PING. While
       * count_initialized is false, _ONLY_ EXAMSG_PING can be received,
       * any other incoming message is dropped. This is mandatory to make
       * sure the count sequence is actually initialized _BEFORE_ any
       * client message (i.e., carrying a payload) is received. */
      if (IS_PING(netmsg))
        {
	  ExamsgAny nc_msg;
	  int ret;

	  src_state->count = ((ExamsgPing *)netmsg->msg)->seq;
	  src_state->count_initialized = true;

	  exalog_trace("count for %u:'%s' initialized to %d by %s",
	      src_node_id, netmsg->mid.host, src_state->count,
	      examsgTypeName(((ExamsgAny*)netmsg->msg)->type));

	  /* inform the other thread that a new node was detected. */
	  nc_msg.type = EXAMSG_NEW_COMER;
	  ret = examsgSend(mh, EXAMSG_CMSGD_ID, EXAMSG_LOCALHOST,
	                   &nc_msg, sizeof(nc_msg));
	  EXA_ASSERT(ret == sizeof(nc_msg));
	}
      else
	exalog_trace("dropping message from %u:`%s' type %s "
		     "since count is not yet initialized",
		     src_node_id, netmsg->mid.host,
		     examsgTypeName(((ExamsgAny*)netmsg->msg)->type));

      return 0;
    }

  /* Now on, source sequence count is initialized */

  /* ping messages are used to check if some messages were lost */
  if (IS_PING(netmsg))
    {
      /* if the request has not already been sent the sender thread */
      if (src_state->count != ((ExamsgPing *)&netmsg->msg)->seq
	  && !src_state->retransmit_request_in_progress_from_me)
	{
	  src_state->retransmit_request_in_progress_from_me = true;
	  exalog_trace("missed message %d (local=%d) from %u:`%s'",
		       ((ExamsgPing *)&netmsg->msg)->seq, src_state->count,
		       src_node_id, netmsg->mid.host);

	  send_retransmit_req(mh, src_state->id, src_state->count + 1);

	  return 0;
	}
    }

  if (IS_RTRANS(netmsg))
    {
      process_retransmission_request(mh, src_node_id, netmsg);
      return 0;
    }

  /* Skip sequence checks for special messages */
  if (!IS_SPECIAL(netmsg))
    {
      /* the message MUST be a standard message (no flag) */
      EXA_ASSERT(!netmsg->flags);

      /* check message loss */
      n = checkseq(mh, netmsg->count, src_state);
      /* checkseq asserts if too many messages lost */
      /* if n != 0 the message should not be processed */
      if (n)
	return 0;
    }

  /* filter messages not for us */
  if (!exa_nodeset_contains(&netmsg->dest_nodes, netid.node))
    {
#ifdef WITH_TRACE
      char hex_nodes[EXA_NODESET_HEX_SIZE + 1];

      exa_nodeset_to_hex(&netmsg->dest_nodes, hex_nodes);
      exalog_trace("filtered message not for us from %u:`%s' to `%s'",
		   src_node_id, netmsg->mid.host, hex_nodes);
#endif
      return 0;
    }

  /* decode */
  strlcpy(mid->host, netmsg->mid.host, sizeof(mid->host));
  uuid_copy(&mid->netid.cluster, &netmsg->mid.netid.cluster);
  mid->netid.node = netmsg->mid.netid.node;
  mid->id = netmsg->mid.id;

  *msg = netmsg->msg;
  *nbytes = netmsg->size;
  *to = netmsg->to;

  return 1;
}

/** \brief schedule a retransmit request
 *
 * \param[in] node_id   Id of node to send the request to
 * \param[in] count	Seq of message we want retransmitted
 */
int
network_send_retransmit_req(exa_nodeid_t node_id, mseq_t count)
{
  struct timeval elapsed_tv;
  unsigned delay, elapsed;
  double random_delay_factor;

  ExamsgNetMsg rtrans;
  size_t msgbytes;
  int n;
  struct mcastnodestate *m;
  struct timeval now;
  int ret;

  /* if we actually received the message we are about to ask
   * a retransmission for, we just do not ask anything */
  os_thread_mutex_lock(&lock);

  m = &mcaststate[node_id];
  if (m->id == EXA_NODEID_NONE || !m->retransmit_request_in_progress_from_me)
    {
      os_thread_mutex_unlock(&lock);
      return 0;
    }

  os_thread_mutex_unlock(&lock);

  /* set a random value: we do not want all nodes to send the same retransmit
   * request at the same time but when we really get in a hurry, try to send
   * it sooner.
   * random_delay_factor = random * (1 - (n / N) ^ 2) where n is the number of
   * messages lost and N the size of the history of messages (the maximum
   * number of messages we can loose)
   */
#define SQUARE(x) ((x) * (x))

  random_delay_factor =
    os_drand() * (1 - SQUARE(mseq_dist(count, m->count) / (double)EXAMSG_NETMSG_BUFFER));
  delay = (unsigned)(RETR_MAX_DELAY * random_delay_factor);

  exalog_trace("count diff = %d (%d - %d)"
	      " random_delay_factor = %e", m->count - count, m->count, count,
	      random_delay_factor);

  if (delay > 0)
    {
      exalog_debug("scheduling retransmit for %d from node %u in %u us",
		   count, node_id, delay);
      os_microsleep(delay);
    }

  /* get time */
  ret = exa_gettimeofday(&now);
  EXA_ASSERT(ret == 0);

  os_thread_mutex_lock(&lock);

  if (timercmp(&now, &m->retransmit_req_date, <))
    elapsed = 0;
  else
    {
      elapsed_tv = os_timeval_diff(&now, &m->retransmit_req_date);

      OS_ASSERT(TIMEVAL_IS_VALID(&elapsed_tv));

      elapsed = TIMEVAL_TO_SCALAR(&elapsed_tv);
    }

  /* if the same request has been received in the meantime, forget it */
  if (elapsed < RETR_REQ_LIFESPAN
      && mseq_before_eq(m->retransmit_req_count, count))
    {
      /* the request from the other node is enough,
       * we do not need to send our own request */
      exalog_trace("cancel scheduled request for %d from %u:"
		   " a node had already requested message %d",
		   count, node_id, m->retransmit_req_count);
      m->retransmit_request_in_progress_from_me = false;

      os_thread_mutex_unlock(&lock);

      return 0;
    }

  /* if we actually received the message we are about to ask
   * a retransmission for, we just do not ask anything */
  if (!m->retransmit_request_in_progress_from_me)
    {
      os_thread_mutex_unlock(&lock);
      return 0;
    }

  os_thread_mutex_unlock(&lock);

  /* create request */
  exalog_debug("asking for retransmit of %d from %u because there is no valid"
               " request from another node (elapsed=%u - count=%d)",
               count, node_id, elapsed, m->retransmit_req_count);

  rtrans.flags = EXAMSGF_RTRANS;
  rtrans.incarnation = incarnation;
  rtrans.count = mcastcount;

  exa_nodeset_single(&rtrans.dest_nodes, node_id);

  strlcpy(rtrans.mid.host, this_node, sizeof(rtrans.mid.host));
  uuid_copy(&rtrans.mid.netid.cluster, &netid.cluster);
  rtrans.mid.netid.node = netid.node;

  /* Store requested seq in the message body */
  *(mseq_t *)rtrans.msg = count;
  rtrans.size = sizeof(mseq_t);
  msgbytes = NETMSG_SIZE(&rtrans);

  /* reset flag before sending in order to prevent races */
  os_thread_mutex_lock(&lock);
  m->retransmit_request_in_progress_from_me = false;
  os_thread_mutex_unlock(&lock);

  n = sock_send(&rtrans, msgbytes);

  EXA_ASSERT_VERBOSE(network_manageable(n), "failed sending %" PRIzd " bytes: %s",
		     msgbytes, strerror(-n));
  return 0;
}

/** \brief Resend an old message
 *
 * \param[in] count	Message seq
 *
 * \return 0 on success or a negative error code
 */
int
retransmit(mseq_t count)
{
  int i;
  int n;
  size_t msgbytes;

  /* Update backoff timer */
  backoff_apply_penalty(&backoff, BACKOFF_PENALTY);
  exalog_trace("backoff timer set to %u us", backoff);

  /* Wait for other retransmission requests to arrive */
  os_microsleep(retr_backoff);

  os_thread_mutex_lock(&ret_sched.lock);

  EXA_ASSERT(ret_sched.is_sched); /* if we are here it is for a good reason */

  /* check that no retranmit request was received while we were waiting */
  /* we drop the request if an older retransmission is scheduled */
  if (mseq_before(ret_sched.seq, count) && ret_sched.is_sched)
    {
      exalog_trace("dropping retransmit req %d because %d is scheduled" ,
		   count, ret_sched.seq);

      os_thread_mutex_unlock(&ret_sched.lock);
      return 0;
    }

  ret_sched.seq = 0;
  ret_sched.is_sched = false;

  os_thread_mutex_unlock(&ret_sched.lock);

  exalog_debug("retransmit request for message %d, %d messages lost",
	       count, mseq_dist(mcastcount, count));

  /* look for message */
  for(i = 0; i < EXAMSG_NETMSG_BUFFER; i++)
    if (sentbuf[i].count == count)
      break;

  if (i>= EXAMSG_NETMSG_BUFFER)
    {
      /* lost message, someone will die soon... */
      exalog_error("cannot retransmit %d - message lost", count);
      return -EINVAL;
    }

  do
    {
      os_thread_mutex_lock(&ret_sched.lock);

      if (mseq_before(ret_sched.seq, count) && ret_sched.is_sched)
	{
	  /* we received a new retransmission request meanwhile
	   * we can abort this one because another one is scheduled */
	  exalog_debug("aborting retransmission of %d older scheduled %d",
		       count, ret_sched.seq);
	  os_thread_mutex_unlock(&ret_sched.lock);

	  /* We haven't waited long enough before starting to retransmit,
	     so next time we'll wait a bit longer. Also cancel the penalty
	     applied to the backoff upon entering this function */
	  backoff_apply_penalty(&retr_backoff, BACKOFF_PENALTY);
	  backoff_apply_gain(&backoff, 1 - BACKOFF_PENALTY);

	  exalog_trace("retr_backoff set to %u us,"
		       " backoff set back to %u us", retr_backoff, backoff);
	  break;
	}

      os_thread_mutex_unlock(&ret_sched.lock);

      os_microsleep(backoff);

      msgbytes = NETMSG_SIZE(&sentbuf[i]);
      exalog_debug("resending message %d of type %s",
		   sentbuf[i].count, examsgTypeName(((ExamsgAny *)sentbuf[i].msg)->type));

      n = sock_send(&sentbuf[i], msgbytes);
      if (n < 0)
	{
	  EXA_ASSERT_VERBOSE(network_manageable(n),
			     "failed retransmitting %" PRIzd " bytes: %s",
			     msgbytes, strerror(-n));
	  /* try again until it works */
	  i--;
	}

      i = (i + 1) % EXAMSG_NETMSG_BUFFER;
    }
  while (i != (last_sent_index + 1) % EXAMSG_NETMSG_BUFFER);

  backoff_apply_gain(&retr_backoff, BACKOFF_GAIN);
  exalog_trace("retrbackoff timer set to %u us", retr_backoff);

  return 0;
}

/**
 * Decrement the highest received seq (count) of the specified node,
 * which means the last message received from this node is ignored.
 *
 * XXX Ugly, should not be necessary if checkseq() wasn't doing thing it
 * should not.
 *
 * \param[in] node_id  Id of node whost last last message is to be ignored
 */
void network_ignore_message(exa_nodeid_t node_id)
{
  struct mcastnodestate *m;

  os_thread_mutex_lock(&lock);

  m = &mcaststate[node_id];
  EXA_ASSERT(m->id != EXA_NODEID_NONE);

  os_thread_mutex_unlock(&lock);

  m->count--;
}
