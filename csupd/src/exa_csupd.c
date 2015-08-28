/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**\file
 * \brief Cluster supervision daemon.
 */

#include "sup_cluster.h"
#include "sup_clique.h"
#include "sup_ping.h"
#include "sup_debug.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"
#include "common/include/exa_names.h"
#include "csupd/include/exa_csupd.h"
#include "log/include/log.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_getopt.h"
#include "os/include/os_mem.h"
#include "os/include/os_syslog.h"
#include "os/include/os_time.h"
#include "os/include/os_daemon_child.h"
#include "os/include/os_daemon_parent.h"
#include "os/include/os_stdio.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <signal.h>

/* For temporary logging functions */
#include <stdarg.h>

#ifdef USE_YAOURT
#include <yaourt/yaourt.h>
#endif

static const char *program;    /**< Daemon name */

int ping_period = 1;           /**< Ping period, in seconds */
bool do_ping;            /**< Whether a ping must be sent */

static int ping_timeout = 5;   /**< Ping timeout, in seconds */
static sup_cluster_t cluster;  /**< Cluster this daemon is part of */
static sup_node_t *self;       /**< Shortcut to local node */

static bool self_view_changed;   /**< Self view has changed ? */
static bool other_view_changed;  /**< View of another node changed ? */

static exa_nodeset_t accepted_clique;  /**< Last accepted clique */

static uint32_t admind_pid = 0;                       /**< PID of Admind */
static os_daemon_t admind_daemon = OS_DAEMON_INVALID; /**< Admind handle */

static bool quit = false; /**< is it time to quit ? */

/** Whether self is coordinator */
#define self_is_coord()  (self->view.coord == self->id)

/** Whether self sees a given node */
#define self_sees(node) \
  (exa_nodeset_contains(&self->view.nodes_seen, (node)->id))

/** Highest generation accepted or committed by self */
#define self_highest_gen()  (MAX(self->view.accepted, self->view.committed))

/**
 * Print info about a node.
 *
 * \param[in] node  Node to print info about
 */
static void
dump_node(const sup_node_t *node)
{
  char seen_str[EXA_MAX_NODES_NUMBER + 1];
  char clique_str[EXA_MAX_NODES_NUMBER + 1];

  if (node == NULL)
    return;

  exa_nodeset_to_bin(&node->view.nodes_seen, seen_str);
  exa_nodeset_to_bin(&node->view.clique, clique_str);

  __debug("%c%c%u: (state=%s seen=%s clique=%s coord=%u accepted=%u committed=%u)",
	  self_sees(node) ? ' ' : '-', node->id == self->id ? '*' : ' ',
	  node->id, sup_state_name(node->view.state),
	  seen_str, clique_str,
	  node->view.coord,
	  node->view.accepted, node->view.committed);
}

/**
 * Set the current state.
 *
 * \param[in] new_state  Supervisor's new state
 */
static inline void
set_state(sup_state_t new_state)
{
#ifdef USE_YAOURT
  yaourt_event_wait(EXAMSG_CSUPD_ID, "sup_set_state %s",
		    sup_state_name(new_state));
#endif
  self->view.state = new_state;
  __trace("state is now %s", sup_state_name(new_state));
}

/**
 * Handle exit properly
 */
static void
exit_handler(int sig)
{
  quit = true;
}

/**
 * Handle a segmentation fault
 */
static void segfault_handler(int sig)
{
  abort();
}

/**
 * Dump our view of the cluster in the logs.
 */
static void
dump_view(int sig)
{
  exa_nodeid_t node_id;
  char known_str[EXA_MAX_NODES_NUMBER + 1];

  if (sig)
    __debug("got signal %d, dumping view", sig);

  __debug("incarnation: %hu", self->incarnation);

  exa_nodeset_to_bin(&cluster.known_nodes, known_str);
  __debug("num_nodes: %u known_nodes: %s", cluster.num_nodes, known_str);

  for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
    if (exa_nodeset_contains(&cluster.known_nodes, node_id))
	dump_node(sup_cluster_node(&cluster, node_id));
}

/**
 * Mark a node as being alive
 *
 * \param[in] node  Node to mark
 */
static void
mark_alive(sup_node_t *node)
{
  node->last_seen = 0;

  if (!self_sees(node))
    {
      __trace("node %u alive", node->id);
      sup_view_add_node(&self->view, node->id);
      self_view_changed = true;
    }
}

/**
 * Mark a node as being dead
 *
 * \param[in] node  Node to mark
 */
static void
mark_dead(sup_node_t *node)
{
  /* Can't ever mark self as dead */
  EXA_ASSERT(node->id != self->id);

  node->last_seen = ping_timeout;
  node->view.state = SUP_STATE_UNKNOWN;

  if (self_sees(node))
    {
      __trace("node %u died", node->id);
      sup_view_del_node(&self->view, node->id);
      self_view_changed = true;
    }
}

/**
 * Update the time the nodes were last seen (ie, how long ago a ping
 * was last received from them).
 *
 * Must be called every #ping_period seconds.
 */
static void
update_last_seen(void)
{
  exa_nodeid_t node_id;

  for (node_id = 0; node_id < self->view.num_seen; node_id++)
    {
      sup_node_t *node = sup_cluster_node(&cluster, node_id);

      if (node && node != self && self_sees(node))
	{
	  node->last_seen += ping_period;
	  if (node->last_seen >= ping_timeout)
	    mark_dead(node);
	}
    }

  EXA_ASSERT(self->last_seen == 0);
}

/**
 * Check whether we're seen as coordinator by all the nodes in our clique.
 * NOTE: This function assumes that we see ourself as coord.
 *
 * \return true if the whole clique agrees that we are coord, false otherwise
 */
static bool
clique_sees_self_as_coord(void)
{
  exa_nodeid_t node_id;

  for (node_id = 0; node_id < self->view.num_seen; node_id++)
    if (exa_nodeset_contains(&self->view.clique, node_id))
      {
	sup_node_t *node = sup_cluster_node(&cluster, node_id);

	EXA_ASSERT(node);

	if (!self_sees(node))
	  return false;

	if (!exa_nodeset_equals(&node->view.clique, &self->view.clique))
	  return false;

	if (node->view.coord != self->view.coord)
	  return false;
      }

  __trace("clique_sees_self_as_coord -> YES");

  return true;
}

/**
 * Check whether all nodes in our clique have accepted the clique
 * (aka membership) whose generation is given.
 *
 * \param[in] gen  Membership generation
 *
 * \return true if all have accepted, false otherwise
 */
static bool
clique_has_accepted(sup_gen_t gen)
{
  exa_nodeid_t node_id;

  for (node_id = 0; node_id < self->view.num_seen; node_id++)
    if (exa_nodeset_contains(&self->view.clique, node_id))
      {
	sup_node_t *node = sup_cluster_node(&cluster, node_id);

	EXA_ASSERT(node);

	if (!self_sees(node))
	  return false;

	if (!exa_nodeset_equals(&node->view.clique, &self->view.clique))
	  return false;

	if (node->view.accepted != gen)
	  return false;
      }

  return true;
}

/**
 * Get the membership generation number to use in the next agreement.
 * Takes into account *all* nodes seen, not just the nodes in the clique.
 *
 * \return Generation number
 */
static sup_gen_t
next_gen(void)
{
  exa_nodeid_t node_id;
  sup_gen_t max_gen = 0;

  for (node_id = 0; node_id < self->view.num_seen; node_id++)
    {
      sup_node_t *node = sup_cluster_node(&cluster, node_id);

      if (node && self_sees(node))
	max_gen = MAX(max_gen, MAX(node->view.accepted, node->view.committed));
    }

  return max_gen + 1;
}

/**
 * Accept the current clique.
 *
 * \param[in] gen  Generation of accepted clique
 */
static void
accept_clique(sup_gen_t gen)
{
  set_state(SUP_STATE_ACCEPT);
  self->view.accepted = gen;

  exa_nodeset_copy(&accepted_clique, &self->view.clique);
}

/**
 * Commit the previously accepted clique.
 */
static void
commit_clique(void)
{
#ifdef DEBUG
  char clique_str[EXA_MAX_NODES_NUMBER + 1];

  exa_nodeset_to_bin(&accepted_clique, clique_str);
  __debug("@@@ delivering membership %u:%s to Evmgr", self->view.accepted,
	  clique_str);
#endif

  EXA_ASSERT(self_sees(self));
  sup_deliver(self->view.accepted, &accepted_clique);

  set_state(SUP_STATE_COMMIT);
  self->view.committed = self->view.accepted;
}

/**
 * Called in state CHANGE to process a ping.
 *
 * \param[in] ping  Ping to process
 */
static void
state_change_process_ping(const sup_ping_t *ping)
{
  switch (ping->view.state)
    {
    case SUP_STATE_UNKNOWN:
      /* Not possible */
      break;

    case SUP_STATE_CHANGE:
      if (self_is_coord())
	{
	  __trace("i'm coord");

	  /* Go to state ACCEPT if all the nodes in our clique see us as
	   * their coordinator. This will initiate the agreement phase.
	   */
	  if (clique_sees_self_as_coord())
	    {
	      __trace("+++ EVERYONE IN MY CLIQUE SEES ME AS COORD");

	      accept_clique(next_gen());
	      __trace("\t=> new state=%s, accepted=%u",
		     sup_state_name(self->view.state),
		     self->view.accepted);
	    }
	}
      break;

    case SUP_STATE_ACCEPT:
      if (!self_is_coord())
	{
	  /* Go to state ACCEPT if the sender is our coordinator, views the
	   * same clique as us and its accepted clique generation is higher
	   * than ours.
	   */
	  if (ping->sender == self->view.coord
	      && exa_nodeset_equals(&ping->view.clique, &self->view.clique)
	      && ping->view.accepted > self_highest_gen())
	    {
	      __trace("coord ok, view ok, accepted ok => i accept");
	      accept_clique(ping->view.accepted);
	    }
	}
      break;

    case SUP_STATE_COMMIT:
      if (!self_is_coord())
	{
	  /* Commit the membership if we accepted it */
	  if (exa_nodeset_equals(&ping->view.clique, &accepted_clique)
	      && ping->view.committed == self->view.accepted
	      && ping->view.committed > self->view.committed)
	    {
	      __trace("saw commit %u from %u and i accepted %u => i commit %u",
		      ping->view.committed, ping->sender, self->view.accepted,
		      self->view.accepted);
	      commit_clique();
	    }
	}
      break;
    }
}

/**
 * Called in state ACCEPT to process a ping.
 *
 * \param[in] ping  Ping to process
 */
static void
state_accept_process_ping(const sup_ping_t *ping)
{
  switch (ping->view.state)
    {
    case SUP_STATE_UNKNOWN:
      /* Not possible */
      break;

    case SUP_STATE_CHANGE:
      /* Ignore: the event that triggered the sender's state change will
       * eventually be seen by us, and our view will change too
       */
      break;

    case SUP_STATE_ACCEPT:
      if (self_is_coord())
	{
	  /* If the sender sees ourself as its coordinator and has accepted
	   * our proposed clique, check if we have reached the point where all
	   * nodes have accepted.
	   *
	   * If so, commit the clique. This will eventually trigger the commit
	   * on all other nodes.
	   */
	  if (ping->view.coord == self->id
	      && ping->view.accepted == self->view.accepted)
	    {
	      if (clique_has_accepted(self->view.accepted))
		{
		  __trace(":):) ALL IN CLIQUE HAVE ACCEPTED VIEW WITH GEN %u",
			 self->view.accepted);
		  commit_clique();
		}
	    }
	}
      break;

    case SUP_STATE_COMMIT:
      if (!self_is_coord())
	{
	  /* If the sender views the same clique as us and its committed
	   * generation is equal to our accepted generation, commit.
	   */
	  if (exa_nodeset_equals(&ping->view.clique, &accepted_clique)
	      && ping->view.committed == self->view.accepted
	      && ping->view.committed > self->view.committed)
	    {
	      __trace("saw commit %u from %u and i accepted %u => i commit %u",
		      ping->view.committed, ping->sender, self->view.accepted,
		      self->view.accepted);
	      commit_clique();
	    }
	}
      break;
    }
}

/**
 * Called in state COMMIT to process a ping.
 *
 * \param[in] ping  Ping to process
 */
static void
state_commit_process_ping(const sup_ping_t *ping)
{
  switch (ping->view.state)
    {
    case SUP_STATE_UNKNOWN:
      /* Not possible */
      break;

    case SUP_STATE_CHANGE:
      /* If we get there, it means the sender is already known (seen by self)
       * but the sender's view had changed.
       *
       *   - If self eventually sees the change too, it will go to state CHANGE
       *     (which should be the "normal" case)
       *
       *   - TODO: If self never sees the change, the sender should be deemed
       *     dead at some point, which will make self go to state CHANGE.
       */
      break;

    case SUP_STATE_ACCEPT:
    case SUP_STATE_COMMIT:
      /* Ignore: we're already in commit state, so we can't accept another
       * membership (unless we've seen the change ourself, which means we
       * would be in state CHANGE) and we can't commit another membership
       */
      break;
    }
}

/**
 * Update info on a node.
 *
 * This may have the side-effect of changing our state to CHANGED.
 *
 * \param     node  Node to update the info on
 * \param[in] ping  Ping received from #node
 */
static void
update_node_info(sup_node_t *node, const sup_ping_t *ping)
{
  bool inca_changed, view_changed;
  bool rebooted_too_fast;

  inca_changed = (ping->incarnation != node->incarnation);
  view_changed = !sup_view_equals(&ping->view, &node->view);

  /* If the node's incarnation has changed but the node was not seen down
   * in-between, it means the node has rebooted faster than ping_timeout
   */
  rebooted_too_fast = self_sees(node) && inca_changed;

  node->incarnation = ping->incarnation;
  sup_view_copy(&node->view, &ping->view);

  if (rebooted_too_fast)
    {
      __trace("node %u rebooted too fast, marking it dead", node->id);
      mark_dead(node);
    }
  else
    mark_alive(node);

  if (inca_changed || view_changed)
    {
      __trace("node %u's view changed", node->id);
      other_view_changed = true;
    }
}

/**
 * Check ping validity.
 *
 * \param[in] ping  Ping to check
 * \param[in] op    Operation checking for: 'S' (sending) or 'R' (receiving)
 *
 * \return true if valid, false otherwise
 */
bool
sup_check_ping(const sup_ping_t *ping, char op)
{
  bool ok = true;
  char suffix[32];

  EXA_ASSERT(op == 'S' || op == 'R');

  if (op == 'S')
    os_snprintf(suffix, sizeof(suffix), "sending ping");
  else
    os_snprintf(suffix, sizeof(suffix), "ping from node %u", ping->sender);

  if (ping->incarnation == 0)
    {
      __error("%s: invalid incarnation", suffix);
      ok = false;
    }

  if (!SUP_STATE_CHECK(ping->view.state) || ping->view.state == SUP_STATE_UNKNOWN)
    {
      __error("%s: invalid state %d", suffix, ping->view.state);
      ok = false;
    }

  if (ping->view.num_seen == 0)
    {
      __error("%s: empty view", suffix);
      ok = false;
    }

  if (!exa_nodeset_contains(&ping->view.nodes_seen, ping->sender))
    {
      __error("%s: view does not contain %u", suffix, ping->sender);
      ok = false;
    }

  return ok;
}

/**
 * Process a ping message
 *
 * \param[in] ping  Ping to process
 */
static void
sup_process_ping(const sup_ping_t *ping)
{
  sup_node_t *node;

#ifdef WITH_TRACE
  __trace("<-- from node %u: incarnation=%hu", ping->sender, ping->incarnation);
  sup_view_debug(&ping->view);
#endif

  EXA_ASSERT(sup_check_ping(ping, 'R'));

  node = sup_cluster_node(&cluster, ping->sender);
  if (node == NULL)
    {
      /* First time the node is seen, let's add it to the cluster */
      sup_cluster_add_node(&cluster, ping->sender);
      node = sup_cluster_node(&cluster, ping->sender);
      EXA_ASSERT(node);
    }

  if (node != self)
    update_node_info(node, ping);

  switch (self->view.state)
    {
    case SUP_STATE_UNKNOWN:
      /* Can't possibly be in UNKNOWN state */
      EXA_ASSERT(false);
      break;

    case SUP_STATE_CHANGE:
      state_change_process_ping(ping);
      break;

    case SUP_STATE_ACCEPT:
      state_accept_process_ping(ping);
      break;

    case SUP_STATE_COMMIT:
      state_commit_process_ping(ping);
      break;
    }
}

/**
 * Before sending a ping.
 */
static void
sup_pre_ping(void)
{
  update_last_seen();

#ifdef USE_YAOURT
  if (yaourt_event_wait(EXAMSG_CSUPD_ID, "sup_pre_ping") != 0)
    self_view_changed = true;
#endif

  if (self_view_changed || other_view_changed)
    {
      exa_nodeset_t new_clique;
      bool clique_changed;
      exa_nodeid_t coord;

      __trace("RECALCULATING CLIQUE");

      sup_clique_compute(&cluster, &new_clique);
      clique_changed = !exa_nodeset_equals(&new_clique, &self->view.clique);
      if (clique_changed)
	{
	  exa_nodeset_copy(&self->view.clique, &new_clique);
	  coord = exa_nodeset_first(&self->view.clique);
	  if (coord != self->view.coord)
	    {
	      __trace("new coord: %u", coord);
	      self->view.coord = coord;
	    }
	}

#ifdef DEBUG
      {
	char clique_str[EXA_MAX_NODES_NUMBER + 1];
	exa_nodeset_to_bin(&self->view.clique, clique_str);
	__trace("Clique: %s %s", clique_str, clique_changed ? "CHANGED" : "");
      }
#endif
    }

  if (self_view_changed || other_view_changed)
    {
      __trace("*** SUP_MSHIP_CHANGE ***");
      set_state(SUP_STATE_CHANGE);
    }

#ifdef WITH_TRACE
  dump_view(0);
#endif
}

/**
 * After sending a ping.
 */
static void
sup_post_ping(void)
{
  self_view_changed = false;
  other_view_changed = false;
}

/**
 * Check whether Admind is running.
 */
static void check_admind(void)
{
#ifndef WITH_SUPSIM
    /* We can't use os_daemon_status() here because it is implemented with
     * waitpid(): a process P can use waitpid() on a process P' *iff* P' is
     * a child of P, and Admind is not a child of Csupd!
     *
     * We use os_daemon_exists() which may return a false positive if the
     * specified daemon is zombie, but Admind can't be zombie since it is
     * reparented to init and init will reap it.
     */
    if (!os_daemon_exists(admind_daemon))
    {
        const char *name = exa_daemon_name(EXA_DAEMON_ADMIND);
        os_syslog(OS_SYSLOG_ERROR, "daemon %s (pid %"PRIu32") is dead => EXITING",
                  name, admind_pid);
        exit(1);
    }
#endif
}

static bool csupd_quit(void)
{
#ifdef WITH_SUPSIM
  return quit;
#else
  return quit || daemon_must_quit();
#endif
}

/**
 * Main loop.
 */
static void
loop(void)
{
  static struct timespec last_time;

  __trace("marking self as alive");

  os_get_monotonic_time(&last_time);

  /* We *always* see ourself */
  mark_alive(self);
  do_ping = true;

  while (!csupd_quit())
    {
      struct timespec now;
      sup_ping_t ping;

      os_get_monotonic_time(&now);

      /* if the node was detected as frozen for more than half a ping_timeout
       * we abort because this behaviour is not acceptable (byzantine) */
      EXA_ASSERT_VERBOSE(
	     difftime(now.tv_sec, last_time.tv_sec) <= (ping_timeout + 1) / 2,
	     "Node frozen during '%lu' seconds. Aborting",
	     (unsigned long)difftime(now.tv_sec, last_time.tv_sec));
      last_time = now;

      if (do_ping)
	{
	  do_ping = false;

	  check_admind();

	  sup_pre_ping();
	  sup_send_ping(&cluster, &self->view);
	  sup_post_ping();
	}

      /* wait for an event and process it */
      if (sup_recv_ping(&ping))
        sup_process_ping(&ping);
    }

  sup_view_debug(&self->view);
  __trace("I am seen down, bye bye");
}

/**
 * Daemon initialization.
 *
 * \param[in] local_id     Local node id
 * \param[in] incarnation  Current incarnation
 *
 * \return true if successfull, false otherwise
 */
static bool
init(exa_nodeid_t local_id, unsigned short incarnation)
{
#ifndef WIN32
  struct sigaction siga;
#endif

  /* initialize logging subsystem */
  init_log();

  os_openlog(exa_daemon_name(EXA_DAEMON_CSUPD));

  sup_cluster_init(&cluster);

  /* Set self */
  if (sup_cluster_add_node(&cluster, local_id) < 0)
    return false;

  self = cluster.self = sup_cluster_node(&cluster, local_id);
  EXA_ASSERT(self);
  self->incarnation = incarnation;

  __debug("instance id: %u", self->id);
  __debug("incarnation: %hu", self->incarnation);
  __debug("ping period: %d secs", ping_period);

  if (!sup_setup_messaging(local_id))
    return false;

#ifdef USE_YAOURT
  if (!yaourt_init())
    {
      __debug("Yaourt: Csupd init FAILED (%s)", yaourt_error);
      return false;
    }
#endif

#ifdef WIN32
  signal(SIGSEGV, segfault_handler);
  signal(SIGABRT, segfault_handler);
#else
  /* block all signals inside handler */
  sigfillset(&siga.sa_mask);
  siga.sa_flags = 0;
  siga.sa_handler = dump_view;
  sigaction(SIGUSR1,  &siga, NULL);

  sigfillset(&siga.sa_mask);
  /* The segfault handler needs be called only once in order to signal admind
   * we are dying. The next signal (raised in the handler) will be handled by
   * the default handler */
  siga.sa_flags = SA_RESETHAND;
  siga.sa_handler = segfault_handler;
  sigaction(SIGSEGV, &siga, NULL);
  sigaction(SIGABRT, &siga, NULL);

  siga.sa_flags = 0;
  siga.sa_handler = SIG_IGN;
  sigaction(SIGUSR2, &siga, NULL);

  siga.sa_handler = exit_handler;
  sigaction(SIGTERM, &siga, NULL);
#endif

  return true;
}

/**
 * Display usage help and exit.
 *
 * \param[in] status  Exit status
 */
static void
usage(int status)
{
  fprintf(stderr, "Exanodes Cluster Supervision Daemon\n");
#ifdef WITH_SUPSIM
  fprintf(stderr, "(simulation mode)\n");
#endif

  fprintf(stderr, "Usage: %s [options]\n", program);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -a, --admind <p>       Set admind PID to <p>\n");
  fprintf(stderr, "  -h, --help             Display this usage help\n");
  fprintf(stderr, "  -i, --node-id <i>      Set node id to <i>\n");
  fprintf(stderr, "  -I, --incarnation <i>  Set incarnation to <i>\n");
  fprintf(stderr, "  -p, --ping <p>         Set ping period to <p> seconds\n");
  fprintf(stderr, "  -t, --timeout <t>      Set ping timeout to <t> seconds\n");

  exit(status);
}

#ifdef WIN32
#define isatty _isatty
#define fileno _fileno
#endif

/* Print an error message to stderr or exanodes.log (depending on
 * whether the daemon has been launched from a tty) and exit */
#define err_return(...)                                 \
  do {							\
    if (isatty(fileno(stderr)))				\
      {                                                 \
	fprintf(stderr, __VA_ARGS__);              	\
	fprintf(stderr, "\n"); 		             	\
      }							\
    else						\
      {							\
	init_log();					\
	__error(__VA_ARGS__);				\
      }							\
    return -EINVAL;                                     \
  } while (0)

int daemon_init(int argc, char *argv[])
{
  static struct option long_opts[] =
    {
      { "admind",      required_argument, NULL, 'a' },
      { "help",        no_argument,       NULL, 'h' },
      { "incarnation", required_argument, NULL, 'I' },
      { "node-id",     required_argument, NULL, 'i' },
      { "ping",        required_argument, NULL, 'p' },
      { "timeout",     required_argument, NULL, 't' },
      { NULL,          0                , NULL, 0   }
    };
  int long_idx;
  exa_nodeid_t local_id = EXA_NODEID_NONE;
  unsigned short incarnation;

  program = argv[0];

  while (true)
    {
      int c = os_getopt_long(argc, argv, "a:hi:I:p:t:", long_opts, &long_idx);
      if (c == -1)
	break;

      switch (c)
	{
	case 'a':
	  if (sscanf(optarg, "%"PRIu32, &admind_pid) != 1 || admind_pid <= 1)
	    err_return("invalid admind pid: '%s'", optarg);
	  break;

	case 'h':
	  usage(0);
	  break;

	case 'i':
	  if (sscanf(optarg, "%u", &local_id) != 1
	      || local_id >= EXA_MAX_NODES_NUMBER)
	    err_return("invalid node id: '%s'", optarg);
	  break;

	case 'I':
	  if (sscanf(optarg, "%hu", &incarnation) != 1 || incarnation == 0)
	    err_return("invalid incarnation: '%s'", optarg);
	  break;

	case 'p':
	  if (sscanf(optarg, "%d", &ping_period) != 1 || ping_period <= 0)
	    err_return("invalid ping period: '%s'", optarg);
	  break;

	case 't':
	  if (sscanf(optarg, "%d", &ping_timeout) != 1 || ping_timeout <= 0)
	    err_return("invalid ping timeout: '%s'", optarg);
	  break;

	default:
	  usage(1);
	}
    }

  if (optind < argc)
    err_return("%s: too many arguments\nType %s --help for usage help",
	      program, program);

  if (local_id == EXA_NODEID_NONE)
    err_return("missing required node id");

  if (admind_pid == 0)
    err_return("missing required Admind pid");

  if (os_daemon_from_pid(&admind_daemon, admind_pid) != 0)
    err_return("failed getting handle on admind (pid %"PRIu32")", admind_pid);

  if (ping_timeout <= ping_period)
    err_return("ping timeout must be greater than ping period");

  if (incarnation == 0)
    err_return("missing incarnation");

  if (!init(local_id, incarnation))
    err_return("Cannot initialize.");

  return 0;
}

int daemon_main(void)
{
    loop();

    quit = false;

    os_daemon_free(admind_daemon);

    sup_cleanup_messaging();

#ifndef WITH_SUPSIM
    exalog_static_clean();
    examsg_static_clean(EXAMSG_STATIC_RELEASE);
#endif

    os_meminfo("Csupd", OS_MEMINFO_DETAILED);

    return 0;
}

#ifdef WITH_SUPSIM

int main(int argc, char **argv)
{
    int err = daemon_init(argc, argv);

    return err == 0 ? daemon_main() : 1;
}

#endif
