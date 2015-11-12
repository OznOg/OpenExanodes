/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/** \file
 * \brief Network messaging daemon.
 *
 * Main routines of the cluster messaging daemon.
 * \sa libexamsg.a, examsg.o
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "os/include/os_daemon_child.h"
#include "os/include/os_error.h"
#include "os/include/os_getopt.h"
#include "os/include/os_mem.h"
#include "os/include/os_network.h"
#include "os/include/os_thread.h"
#include "os/include/os_time.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/threadonize.h"
#include "os/include/strlcpy.h"

#include "common/include/exa_names.h"
#include "examsg/include/examsg.h"
#include "log/include/log.h"

#include "examsg/src/objpoolapi.h"
#include "examsg/src/mailbox.h"
#include "network.h"

/** examsg stack size */
#define EXAMSGD_STACK_SIZE	65536

/* --- local definitions --------------------------------------------- */

/** The base time after which we will send ping messages to other
    hosts, so that they can detect the loss of messages. This period
    will increase over time, up to EXAMSG_MAX_KEEPALIVE_PERIOD_SEC. */
#define EXAMSG_BASE_KEEPALIVE_PERIOD_SEC  0.04   /* in seconds */
#define EXAMSG_MAX_KEEPALIVE_PERIOD_SEC   40     /* in seconds */

/** Info for ping management */
typedef struct ping_info
  {
    struct timeval delay;  /**< Delay between pings */
    unsigned int factor;   /**< Factor used to calculate delay */
  } ping_info_t;

/* --- local data ---------------------------------------------------- */

static ExamsgHandle local_mh;	/**< Handle for local events */
static ExamsgHandle net_mh;     /**< Handle for network events */

/** Unsent network message */
static struct
  {
    bool pending;                                     /**< Unsent pending ? */
    ExamsgMID mid;                                    /**< Message id */
    char msg[sizeof(ExamsgNetRqst) + sizeof(Examsg)]; /**< Message */
  } unsent;

static bool quit;        /**< Should quit ? */

/**
 * Cleanup mailboxes & stuff.
 */
static void
cleanup(void)
{
  network_exit();

  examsgDelMbox(local_mh, EXAMSG_NETMBOX_ID);
  examsgDelMbox(local_mh, EXAMSG_CMSGD_ID);

  examsgExit(net_mh);
  net_mh = NULL;

  examsgExit(local_mh);
  local_mh = NULL;

  os_net_cleanup();
}

/**
 * Set up network communication.
 *
 * \param[in] cluster_uuid     	iThe cluster uuid we bellong to
 * \param[in] node_name  	Node name
 * \param[in] hostname   	Host name
 * \param[in] nodeid            This node id
 * \param[in] mgroup     	Multicast group
 * \param[in] mport             Multicast port
 * \param[in] inca              Incarnation
 *
 * \return 0 on success, negative error code otherwise
 */
static int
startup(const exa_uuid_t *cluster_uuid, const char *node_name,
	const char *hostname, exa_nodeid_t nodeid,
	const char *mgroup, unsigned short mport, unsigned short inca)
{
  const char *err_msg;
  int s;

  /* Initialize os_network */
  if (os_net_init() != 0)
  {
      err_msg = "Failed initializing os_network";
      s = -EFAULT;
      goto failed;
  }

  /* Initialize examsg framework */
  local_mh = examsgInit(EXAMSG_CMSGD_ID);
  if (local_mh == NULL)
    {
      err_msg = "Failed initializing local handle";
      s = -EFAULT;
      goto failed;
    }

  net_mh = examsgInit(EXAMSG_CMSGD_RECV_ID);
  if (net_mh == NULL)
    {
      err_msg = "Failed initializing network handle";
      s = -EFAULT;
      goto failed;
    }

  /* Create local (regular) mailbox */
  s = examsgAddMbox(local_mh, EXAMSG_CMSGD_ID, 3, EXAMSG_MSG_MAX);
  if (s)
    {
      err_msg = "Failed creating local mailbox";
      goto failed;
    }

  /* Create special (reserved) network mailbox */
  s = examsgAddMbox(local_mh, EXAMSG_NETMBOX_ID, 3, EXAMSG_MSG_MAX);
  if (s)
    {
      err_msg = "Failed creating network mailbox";
      goto failed;
    }

  /* Create network channel */
  s = network_init(cluster_uuid, node_name, hostname, nodeid,
	            mgroup, mport, inca);
  if (s)
    {
      err_msg = "Failed initializing network";
      goto failed;
    }

  return 0;

 failed:

  exalog_error("%s, error %d", err_msg, s);
  cleanup();

  return s;
}

/**
 * Remember unsent network message.
 *
 * \param[in] mid   Message id
 * \param[in] msg   Message body
 * \param[in] size  Message size
 */
static void
remember_unsent(const ExamsgMID *mid, void *msg, size_t size)
{
  EXA_ASSERT(!unsent.pending);

  exalog_trace("remembering message from %d.%s",
	       mid->netid.node, examsgIdToName(mid->id));

  unsent.pending = true;
  unsent.mid = *mid;
  memcpy(unsent.msg, msg, size);
}

/**
 * Send unsent network message.
 *
 * \return true if sent, false otherwise
 */
static bool
send_unsent(void)
{
  EXA_ASSERT(unsent.pending);

  exalog_trace("sending remembered message from %d.%s",
	       unsent.mid.netid.node, examsgIdToName(unsent.mid.id));

  if (network_send(&unsent.mid, (ExamsgNetRqst *)unsent.msg) != 0)
    return false;

  exalog_trace("remembered message from %d.%s sent",
	       unsent.mid.netid.node,
	       examsgIdToName(unsent.mid.id));

  unsent.pending = false;

  return true;
}

/**
 * Adjust ping settings.
 *
 * \param[in,out] pi  Ping info
 */
static void
adjust_ping(ping_info_t *pi)
{
  unsigned new_factor;
  float time;

  /* Adjust the delay factor in order to slow down when no network
   * messages are sent. The check on new_factor is to ensure there
   * is no overflow */
  new_factor = pi->factor * 2;
  if (new_factor > pi->factor)
    pi->factor = new_factor;

  time = EXAMSG_BASE_KEEPALIVE_PERIOD_SEC * pi->factor;
  time = (time > EXAMSG_MAX_KEEPALIVE_PERIOD_SEC
         ? EXAMSG_MAX_KEEPALIVE_PERIOD_SEC : time);

  pi->delay.tv_sec  = time;
  pi->delay.tv_usec = (time - pi->delay.tv_sec) * 1.0e6;
  EXA_ASSERT(TIMEVAL_IS_VALID(&pi->delay));
}

/**
 * Reset ping settings.
 *
 * \param[in,out] pi  Ping info
 */
static inline void
reset_ping(ping_info_t *pi)
{
  pi->delay.tv_sec  = (long)EXAMSG_BASE_KEEPALIVE_PERIOD_SEC;
  pi->delay.tv_usec =
    (EXAMSG_BASE_KEEPALIVE_PERIOD_SEC - pi->delay.tv_sec) * 1.0e6;

  EXA_ASSERT(TIMEVAL_IS_VALID(&pi->delay));
  pi->factor = 1;
}

/**
 * Process an event from a local mailbox.
 *
 * \param[in,out] pi  Ping info
 *
 * \return 1 if processed an event 0 if not, negative error code otherwise
 */
static int
local_mbox_event(ping_info_t *pi)
{
  ExamsgMID mid;
  static Examsg ev;
  int ev_size;
  int s;

  ev_size = examsgRecv(local_mh, &mid, &ev, sizeof(ev));
  if (ev_size <= 0)
    return 0;

  switch (ev.any.type)
  {
    case EXAMSG_EXIT:
      {
         exa_nodeset_t dest_nodes;
         quit = true;
	 exa_nodeset_single(&dest_nodes, mid.netid.node);
         examsgAckReply(local_mh, &ev, EXA_SUCCESS, mid.id, &dest_nodes);
      }
      /* Return 0 because, despite we received a message, we do not want the
       * main loop to continue to process messages as it is asked to stop now.
       * So returning 0 juste means here "no need to read anymore messages" */
      return 0;

    case EXAMSG_SUP_PING:
      if (network_status() == 0)
	network_special_send(&mid, &ev, ev_size);
      break;

    case EXAMSG_ADDNODE:
      {
	exa_nodeset_t dest_nodes;
	examsg_node_info_msg_t *info = (examsg_node_info_msg_t *)&ev;

	s = network_add_node(info->node_id, info->node_name);

	exa_nodeset_single(&dest_nodes, mid.netid.node);
	examsgAckReply(local_mh, &ev, s, mid.id, &dest_nodes);
      }
      break;

    case EXAMSG_DELNODE:
      {
	exa_nodeset_t dest_nodes;

	s = network_del_node(((examsg_node_info_msg_t *)&ev)->node_id);

	exa_nodeset_single(&dest_nodes, mid.netid.node);
	examsgAckReply(local_mh, &ev, s, mid.id, &dest_nodes);
      }
      break;

    case EXAMSG_RETRANSMIT_REQ:
      if (network_status() == 0)
	{
	  ExamsgRetransmitReq *retreq = (ExamsgRetransmitReq *)&ev;

	  exalog_debug("will send a retransmit request for %d to %u",
		       retreq->count, retreq->node_id);

	  s = network_send_retransmit_req(retreq->node_id, retreq->count);
	  EXA_ASSERT(s == 0);
	  reset_ping(pi);
	}
      break;

    case EXAMSG_RETRANSMIT:
      if (network_status() == 0)
	{
	  ExamsgRetransmit *retr = (ExamsgRetransmit *)&ev;
	  exalog_debug("will retransmit messages: %d", retr->count);

	  retransmit(retr->count);
	  reset_ping(pi);
	}
      break;

    case EXAMSG_FENCE:
      {
	const examsgd_fencing_req_t *req =
	  &((examsgd_fencing_req_msg_t *)&ev)->request;

	network_handle_fence_request(req);
      }
      break;

    case EXAMSG_NEW_COMER:
      reset_ping(pi);
      break;

    default:
      {
       exa_nodeset_t dest_nodes;

       exa_nodeset_single(&dest_nodes, mid.netid.node);
       examsgAckReply(local_mh, &ev, EINVAL, mid.id, &dest_nodes);
       exalog_error("got unknown message (type %d)", ev.any.type);
       EXA_ASSERT(false);
      }
      break;
    }

  return 1;
}

/**
 * Read and process an event from the network mailbox.
 *
 * \param[in,out] pi  Ping info
 *
 * \return 1 if processed an event, 0 if not, and negative error code otherwise
 */
static int
net_mbox_event(ping_info_t *pi)
{
  ExamsgMID mid;
  static char msg[sizeof(ExamsgNetRqst) + sizeof(Examsg)];
  int s;

  if (network_status() != 0)
    return 0;

  s = examsgMboxRecv(EXAMSG_NETMBOX_ID, &mid, sizeof(mid),
	             msg, sizeof(msg));
  if (s == 0) /* Nothing to read */
      return 0;

  if (s <= sizeof(ExamsgNetRqst))
    {
      exalog_error("received %d bytes, error %s", s, exa_error_msg(s));
      return -EINVAL;
    }

  s = network_send(&mid, (ExamsgNetRqst *)msg);
  if (s < 0)
    {
      if (network_manageable(s))
	{
	  remember_unsent(&mid, msg, sizeof(msg));
	  return s;
	}

      exalog_error("net mailbox event: error %s", exa_error_msg(s));
      return -EIO;
    }

  reset_ping(pi);

  return 1;
}

/**
 * Thread processing local events (coming from localhost).
 *
 * This routine may set the network status to down as a side-effect
 * of calling network_send().
 *
 * \param dummy  Unused
 *
 * \return NULL
 */
static void *
local_events_routine(void *dummy)
{
  ping_info_t pi;

  exalog_trace("local events routine started");

  reset_ping(&pi);

  while (!quit)
    {
      int status;
      int err;

      /* Must be interruptible because signal TERM triggers quit condition */
      err = examsgWaitInterruptible(local_mh, &pi.delay);
      if (err == -EINTR)
	continue;

      if (err == -ETIME)
	{
	  status = network_status();
	  if (status == -ENETDOWN)
	    {
	      reset_ping(&pi);
	      continue;
	    }

	  if (unsent.pending)
	    {
	      if (send_unsent() && status != 0)
		network_set_status(0);
	    }
	  else
	    {
	      send_ping();
	      adjust_ping(&pi);
	    }

	  continue;
	}

      /* Local mailbox messages are more important than network messages
       * because they may ask for resend, so they are processed first.
       *
       * Process as many local messages as possible before considering
       * network messages so as to avoid sending a new network message
       * when there still are important messages.
       */
      while (local_mbox_event(&pi) == 1)
	;

      status = network_status();
      if (status == -ENETDOWN)
	continue;

      if (unsent.pending)
	{
	  if (send_unsent() && status != 0)
	    network_set_status(0);
	}
      else
	net_mbox_event(&pi);
    }

  return NULL;
}

/**
 * A message couldn't be delivered to a mailbox because it was full.
 * The message is thrown away.
 *
 * \param[in] mbox_id  Mailbox id
 * \param[in] mid      Mid of message received
 * \param[in] msg      Undelivered message
 */
static void
mailbox_full(int mbox_id, const ExamsgMID *mid, const Examsg *msg)
{
  exalog_debug("mailbox %d full, ignoring message from '%s' of type '%s'",
	       mbox_id, mid->host, examsgTypeName(msg->any.type));

  /* Decrement the counter unless the message is special (special
   * messages have no sequence number).  Decrementing is necessary
   * because the counter has been incremented in checkseq (at that
   * step, we couldn't know the mailbox would be full). */
  if (msg->any.type != EXAMSG_SUP_PING)
      network_ignore_message(mid->netid.node);
}

/**
 * Thread processing network events (coming from other nodes).
 *
 * This routine may set the network status to down as a side-effect
 * of calling network_recv(), and sets said status to up when the
 * network comes back.
 *
 * \param[in] dummy  Unused
 *
 * \return NULL
 */
static void
net_events_routine(void *dummy)
{
  int dest_mbox;
  ExamsgMID mid;
  size_t size;
  char *msg;
  int s;

  exalog_as(EXAMSG_CMSGD_ID);
  exalog_trace("network events routine started");

  while (!quit)
    {
      int status = network_status();
      bool retry;

      if (status == -ENETDOWN)
	{
	  network_waitup();
	  network_set_status(0);
	}

      do
	{
	  s = network_recv(net_mh, &mid, &msg, &size, &dest_mbox);
	  retry = (s < 0 && network_manageable(s) && s != -ENETDOWN);
	  if (retry)
	      os_sleep(1);
	}
      while (retry);

      /* Succeeded, the network status is ok */
      if (s > 0 && status != 0)
	network_set_status(0);

      if (s == 0 || s == -ENETDOWN)
	continue;

      EXA_ASSERT(s > 0);

      /* Ping from another node for keepalive */
      if (((ExamsgAny *)msg)->type == EXAMSG_PING)
	{
	  EXA_ASSERT(dest_mbox == EXAMSG_CMSGD_ID);
	  exalog_trace("received an EXAMSG_PING from %u:%s",
		       mid.netid.node, mid.host);
	  continue;
	}

      exalog_trace("delivering %" PRIzu " bytes to %d",
		   size, dest_mbox);

      s = examsgMboxSend(&mid, examsgOwner(net_mh), dest_mbox, msg, size);
      switch (s)
	{
	case -ENXIO:
         /* The mailbox does not exist (yet). This is not an error: csupd may
          * not be started yet and we receive an examsg for it.
          * XXX Doesn't sound too good to me, and we should at least check that
          * the destination is indeed csupd */
	  break;

	case -ENOSPC:
	  mailbox_full(dest_mbox, &mid, (Examsg *)msg);
	  break;

	default:
	  EXA_ASSERT_VERBOSE(s == size + sizeof(mid),
		             "Error %d delivering message to %d",
			     s, dest_mbox);
	  break;
	}
    }
}

/** \brief SIGTERM et al. handler
 */
static void
sig_term(int dummy)
{
  quit = true;
}

/** \brief Print daemon usage summary.
 *
 * \param[in] pname  Program name
 */
static void
usage(const char *pname)
{
  fprintf(stderr,
	  "Usage: %s [options] -c <cluster id> -i <node id> -n <node name>\n"
	  "                    -N <hostname> -I <inca>\n"
	  "Mandatory parameters:\n"
	  "  -c, --cluster-id <cid>        Cluster ID\n"
	  "  -i, --node-id <nid>           Node ID\n"
	  "  -I, --incarnation <inca>      Incarnation number\n"
	  "  -n, --node-name <name>        Node name\n"
	  "  -N, --hostname <host>         Hostname\n"
	  "Options:\n"
	  "  -m, --mcast-addr <multicast>  Multicast address\n"
	  "  -p, --mcast-port <port>       Multicast port\n",
	  pname);
}

/** \brief Initialization of examsgd daemon.
 *
 * Command line must contain the node name and the interface name to use
 * for control messages.
 *
 * Accepts hidden option -d (debugging mode).
 *
 * \param[in] argc	Argument count.
 * \param[in] argv	Array of argument values.
 *
 * \return exit code.
 */
int daemon_init(int argc, char *argv[])
{
  int s;

  /* getopt variables */
  static struct option long_opts[] =
    {
      { "cluster-id",  required_argument, NULL, 'c' },
      { "help",        no_argument,       NULL, 'h' },
      { "hostname",    required_argument, NULL, 'N' },
      { "incarnation", required_argument, NULL, 'I' },
      { "mcast-addr",  required_argument, NULL, 'm' },
      { "mcast-port",  required_argument, NULL, 'p' },
      { "node-id",     required_argument, NULL, 'i' },
      { "node-name",   required_argument, NULL, 'n' },
      { "stats",       no_argument,       NULL, 's' },
      { NULL,          0,                 NULL, 0   }
    };
  int long_idx, c;
  char *e;
  extern char *optarg;
  extern int optind;

  /* configurable options and default values */
  const char *node_name = NULL;
  const char *hostname = NULL;
  const char *mgroup = EXAMSG_MCASTIP;
  unsigned short mport = EXAMSG_PORT;
  unsigned short inca = 0;
  exa_uuid_t cluster_uuid;
  exa_nodeid_t nodeid;

  bool err = false;

  uuid_zero(&cluster_uuid);
  nodeid = EXA_NODEID_NONE;

  /* options parsing */
  while ((c = os_getopt_long(argc, argv, "c:dhi:I:m:n:N:p:s", long_opts, &long_idx))
	 != -1)
    switch (c)
      {
      case 'c':
	if (uuid_scan(optarg, &cluster_uuid) < 0)
	  {
	    fprintf(stderr, "invalid cluster id: '%s'\n", optarg);
	    return -EINVAL;
	  }
	break;

      case 'i':
	nodeid = (exa_nodeid_t)strtol(optarg, &e, 10);
	if (*e || !EXA_NODEID_VALID(nodeid))
	  {
	    fprintf(stderr, "invalid node id: '%s'\n", optarg);
	    return -EINVAL;
	  }
	break;

      case 'I':
	inca = (unsigned short)strtol(optarg, &e, 10);
	if (*e || inca == 0)
	  {
	    fprintf(stderr, "invalid incarnation: '%s'\n", optarg);
	    return -EINVAL;
	  }
	break;

	/* multicast group */
      case 'm':
	mgroup = optarg;
	break;

      case 'n':
	node_name = optarg;
	break;

	/* hostname */
      case 'N':
	hostname = optarg;
	break;

	/* communication port */
      case 'p':
	mport = strtol(optarg, &e, 0);
	if (*e != '\0')
	  {
	    fprintf(stderr, "invalid port number '%s'\n", optarg);
	    return -EINVAL;
	  }
	break;

      case 's':
	examsg_show_stats();
	return 0;
	break;

	/* usage */
      case 'h':
      case '?':
      default:
	usage(argv[0]);
	return -EINVAL;
      }

  if (uuid_is_zero(&cluster_uuid))
    {
      fprintf(stderr, "missing cluster id\n");
      err = true;
    }

  if (nodeid == EXA_NODEID_NONE)
    {
      fprintf(stderr, "missing node id\n");
      err = true;
    }

  if (node_name == NULL)
    {
      fprintf(stderr, "missing node name\n");
      err = true;
    }

  if (hostname == NULL)
    {
      fprintf(stderr, "missing hostname\n");
      err = true;
    }

  if (inca == 0)
    {
      fprintf(stderr, "missing incarnation\n");
      err = true;
    }

  if (err)
    return -EINVAL;

  /* Get cluster id, number of nodes, node id, node name
     and interface parameters */
  if (argc - optind != 0)
    {
      fprintf(stderr, "stray parameters\n");
      usage(argv[0]);
      return -EINVAL;
    }

  signal(SIGTERM, sig_term);
  signal(SIGINT, sig_term);

  s = examsg_static_init(EXAMSG_STATIC_GET);
  if (s)
  {
      fprintf(stderr, "Can't initialize messaging layer.");
      return s;
  }

  exalog_static_init();

  /* Log as exa_msgd by default */
  exalog_as(EXAMSG_CMSGD_ID);

  /* set up network communication */
  return startup(&cluster_uuid, node_name, hostname, nodeid, mgroup, mport, inca);
}


int daemon_main(void)
{
    os_thread_t net_events_thread;

    if (exathread_create(&net_events_thread,
			 EXAMSGD_STACK_SIZE + MIN_THREAD_STACK_SIZE,
			 net_events_routine, NULL))
    {
	/* Init was ok, and thread is running => start the real work */
	local_events_routine(NULL);

	os_thread_join(net_events_thread);
    }

    cleanup();
    exalog_static_clean();
    examsg_static_clean(EXAMSG_STATIC_RELEASE);

    os_meminfo("Examsgd", OS_MEMINFO_DETAILED);

    return 0;
}
