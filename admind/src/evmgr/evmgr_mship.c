/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "evmgr_mship.h"

#include <string.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_monitor.h"
#include "admind/src/adm_nic.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/instance.h"
#include "common/include/exa_math.h"
#include "common/include/exa_names.h"
#include "examsgd/examsgd_client.h"
#include "log/include/log.h"
#include "token_manager/tm_client/include/tm_client.h"
#include "os/include/os_stdio.h"

#ifdef WITH_MONITORING
#include "monitoring/md_client/include/md_notify.h"
#endif

/** All: Best candidate for leadership */
static exa_nodeid_t best_candidate = EXA_NODEID_NONE;

/* All */
static evmgr_mship_info_t leader;  /**< Leader info */
static evmgr_mship_info_t best;    /**< Best candidate info */
static evmgr_mship_info_t local;   /**< Local info */

#define reset_info(info)  memset((info), 0, sizeof(*info))

/* Leader wannabe */
static exa_nodeset_t nodes_ready;  /**< Nodes that replied "yes" */

/** All: Work membership */
static exa_nodeset_t membership;

/** All: Current commit */
static struct
  {
    evmgr_mship_phase_t phase;     /**< Commit phase */
    exa_nodeid_t instigator;       /**< Node that initiated the commit */
    evmgr_mship_info_t info;       /**< Membership & leader info */
    exa_nodeid_t leader_id;        /**< Leader id */
    exa_nodeset_t nodes_expected;  /**< Nodes expected to reply (leader only) */
    exa_nodeset_t nodes_prepared;  /**< Nodes prepared (leader only) */
    exa_nodeset_t nodes_committed; /**< Nodes having committed (leader only) */
  } cc;

/** All: Generation of last committed membership */
static sup_gen_t last_committed_gen;

/** Leader: Generation of last finished commit */
static sup_gen_t last_finished_gen;

/** Leader wannabe: Next commit */
static struct
  {
    bool pending;       /**< Pending ? */
    evmgr_mship_info_t info;  /**< Membership & leader info */
    exa_nodeid_t leader_id;   /**< Leader id */
  } nc;

#define __debug(...)  exalog_debug(__VA_ARGS__)
#define __trace(...)  exalog_trace(__VA_ARGS__)

/** Phase names */
static const char *phase_names[] =
  {
    [EVMGR_MSHIP_PHASE_NONE] = "NONE",
    [EVMGR_MSHIP_PHASE_PREPARE] = "PREPARE",
    [EVMGR_MSHIP_PHASE_COMMIT] = "COMMIT"
  };

/* XXX Not pretty: the evmgr/evmgr_mship split should eventually
 * be reworked to avoid putting recovery-related stuff in here.
 * And need_recovery should be replaced by a direct call to the
 * recovery function */
static bool need_recovery = false;  /**< Recovery needed ? */

static token_manager_t *token_manager = NULL;
static bool have_token = false;

/**
 * Accessors on need_recovery flag
 */
void
need_recovery_set(bool b)
{
  need_recovery = b;
}

bool
need_recovery_get(void)
{
  return need_recovery;
}


/**
 * Accessor on the membership
 *
 * \return the official membership
 */
const exa_nodeset_t *
evmgr_mship(void)
{
  return &membership;
}

static int __quorum(void)
{
    return adm_cluster_nb_nodes() / 2 + 1;
}

bool evmgr_mship_may_have_quorum(const exa_nodeset_t *mship)
{
    int num_voices = exa_nodeset_count(mship);

    /* If the local node has a token manager, all other nodes in the cluster
       are supposed to have one (and the same) as well, thus the nodes in
       mship may get one more voice by acquiring the token (which is not
       guaranteed, of course). */
    if (token_manager != NULL)
        num_voices++;

    return num_voices >= __quorum();
}

/**
 * Tell if the current number of nodes up reach the quorum.
 *
 * \return true if quorate, false otherwise
 */
bool
evmgr_has_quorum(void)
{
    int num_voices = exa_nodeset_count(&membership);

    if (have_token)
        num_voices++;

    return num_voices >= __quorum();
}

/**
 * Whether there is a match between the local membership (from Csupd)
 * and the membership received from best.
 *
 * \param[in] info1  Node info
 * \param[in] info2  Node info
 *
 * \return true if memberships of #info1 and #info2 match, false otherwise
 */
static bool
match(const evmgr_mship_info_t *info1, const evmgr_mship_info_t *info2)
{
  return (info1->gen == info2->gen
	  && exa_nodeset_equals(&info1->mship, &info2->mship));
}

/**
 * Whether everyone is ready.
 *
 * For use by a leader wannabe.
 *
 * \return true if all nodes ready, false otherwise
 */
static bool
all_ready(void)
{
  bool eq = exa_nodeset_equals(&nodes_ready, &local.mship);

#ifdef WITH_TRACE
  /* Doing this in an otherwise pure function is less than ideal */
  if (eq)
    {
      char mship_str[EXA_MAX_NODES_NUMBER + 1];
      exa_nodeset_to_bin(&local.mship, mship_str);
      exalog_trace("ALL NODES READY TO WORK WITH MEMBERSHIP %s", mship_str);
    }
#endif

  return eq;
}

#ifdef DEBUG
/**
 * Debug a membership info.
 *
 * \param[in] caption  Description of membership
 * \param[in] gen      Generation number
 * \param[in] mship    Membership
 */
static void
__debug_mship_info(const char *caption, const evmgr_mship_info_t *info)
{
  char mship_str[EXA_MAX_NODES_NUMBER + 1];

  exa_nodeset_to_bin(&info->mship, mship_str);
  __debug("%s: gen=%u mship=%s already_started?=%d is_leader?=%d", caption,
	  info->gen, mship_str, info->already_started, info->is_leader);
}
#else
#define __debug_mship_info(caption, info)
#endif

#ifdef WITH_TRACE
/**
 * Debug the state of the local node.
 */
static void
__trace_state(void)
{
  char ready_str[EXA_MAX_NODES_NUMBER + 1];

  __debug_mship_info("local", &local);
  __debug_mship_info("best", &best);

  exa_nodeset_to_bin(&nodes_ready, ready_str);

  __debug("leader=%u best_candidate=%u nodes_ready=%s all_ready?=%d",
	  adm_leader_set ? adm_leader_id : EXA_NODEID_NONE,
	  best_candidate, ready_str, all_ready());
}
#else
#define __trace_state()
#endif

bool evmgr_mship_token_manager_is_set(void)
{
    return token_manager != NULL;
}

bool evmgr_mship_token_manager_is_connected(void)
{
    return tm_is_connected(token_manager);
}

bool evmgr_mship_trying_to_acquire_token(void)
{
   int num_alive;

   if (token_manager == NULL)
       return false;

   num_alive = exa_nodeset_count(&membership);
   return num_alive < __quorum() && !have_token;
}

static bool tm_address_forbidden(const char *tm_addr)
{
    struct adm_node *node;

    if (strcmp(tm_addr, "127.0.0.1") == 0)
        return true;

    if (!os_net_ip_is_valid(tm_addr))
        return true;

    adm_cluster_for_each_node(node)
    {
        if (strcmp(tm_addr, adm_nic_ip_str(node->nic)) == 0)
            return true;
    }

    return false;
}

/**
 * Initialize the membership module.
 */
int evmgr_mship_init(void)
{
  const char *tm_addr;

  reset_info(&local);
  reset_info(&best);
  reset_info(&leader);

  best_candidate = EXA_NODEID_NONE;

  exa_nodeset_reset(&nodes_ready);

  exa_nodeset_reset(&membership);

  memset(&cc, 0, sizeof(cc));
  cc.instigator = EXA_NODEID_NONE;
  cc.leader_id = EXA_NODEID_NONE;

  memset(&nc, 0, sizeof(nc));
  nc.leader_id = EXA_NODEID_NONE;

  last_committed_gen = 0;
  last_finished_gen = 0;

  have_token = false;
  token_manager = NULL;

  tm_addr = adm_cluster_get_param_text("token_manager_address");
  if (tm_addr == NULL || tm_addr[0] == '\0')
  {
      token_manager = NULL;
      return 0;
  }

  if (tm_address_forbidden(tm_addr))
  {
      exalog_warning("Token manager address '%s' not allowed."
                   " The token manager MUST NOT run on a cluster node, "
                   " and must be an IP address.",
                    tm_addr);
      return -ADMIND_ERR_INVALID_TOKEN_MANAGER;
  }

  if (adm_cluster_nb_nodes() > 2)
  {
      exalog_warning("Token manager is not allowed on clusters that "
                   "have more than two nodes.");
      return -ADMIND_ERR_TM_TOO_MANY_NODES;
  }

  tm_init(&token_manager, tm_addr, 0);

  /* FIXME Connecting to the TM here is wrong, as we're using the TM address
           from the configuration file *before* it has been synchronized: it
           is possibly not up to date and thus may contain a wrong TM address */
  return evmgr_mship_tm_connect();
}

int evmgr_mship_tm_connect(void)
{
  int err;

  /* Cannot connect to TM if it was not setup or it is invalid */
  if (token_manager == NULL)
      return 0;

  /* TM is already connected, nothing to do */
  if (tm_check_connection(token_manager, &adm_cluster.uuid, adm_my_id) == 0)
      return 0;

  /* Cleanup any previous existing structure here that may be pending
   * after a TM unexpected deconnection */
  tm_disconnect(token_manager);

  exalog_info("Connecting to token manager");
  err = tm_connect(token_manager);
  if (err != 0)
  {
      exalog_error("Failed connecting to token manager: %s (%d)",
              exa_error_msg(err), err);
      return -ADMIND_WARN_TOKEN_MANAGER_DISCONNECTED;
  }

  return 0;
}

void evmgr_mship_shutdown(void)
{
    if (token_manager != NULL)
    {
        exalog_info("Disconnecting from token manager");
        tm_disconnect(token_manager);
        tm_free(&token_manager);
        have_token = false;
    }
}

/**
 * Set the already_started flag to true/false.
 *
 * This is used to be able to pick as leader only nodes that are known to be
 * correctly started. In case the leader dies at the same time that some
 * nodes are seen up, we need to know the nodes that were already in the
 * cluster because only one of them can lead.
 *
 * \param[in] is_started  tells whether the node is started or not
 */
void
evmgr_mship_set_local_started(bool is_started)
{
  local.already_started = is_started;

  if (best_candidate == adm_my_id)
    best.already_started = is_started;

  if (adm_leader_set && adm_leader_id == adm_my_id)
    leader.already_started = is_started;
}

/**
 * Send READY to non-leader wannabe nodes.
 *
 * \param     mh     Examsg handle
 * \param[in] gen    Generation number
 * \param[in] mship  Membership
 */
static void
send_ready(ExamsgHandle mh, sup_gen_t gen, const exa_nodeset_t *mship)
{
  evmgr_ready_msg_t ready;
  exa_nodeset_t dest;
  int r;

  EXA_ASSERT(gen > 0 && exa_nodeset_contains(mship, adm_my_id));

  /* If we have a leader, we can send ready *iff* it is us */
  EXA_ASSERT(!adm_leader_set || adm_leader_id == adm_my_id);

  exa_nodeset_reset(&nodes_ready);

  local.gen = gen;
  exa_nodeset_copy(&local.mship, mship);

  /* On the leader wannabe, best_xxx and local_xxx are
   * initially the same */
  best = local;

  __trace_state();

  __debug_mship_info("sending READY", &local);

  /* Send the message to all nodes in the membership */
  ready.any.type = EXAMSG_EVMGR_MSHIP_READY;
  ready.info = local;

  exa_nodeset_copy(&dest, mship);
  r = examsgSend(mh, EXAMSG_ADMIND_EVMGR_ID, &dest, &ready, sizeof(ready));

  EXA_ASSERT(r == sizeof(ready));
}

/**
 * Send YES to the best candidate.
 *
 * \param     mh   Examsg handle
 * \param[in] gen  Generation number
 * \param[in] to   Destination node
 */
static void
send_yes(ExamsgHandle mh, sup_gen_t gen, exa_nodeid_t to)
{
  evmgr_yes_msg_t yes;
  exa_nodeset_t dest;
  int r;

  EXA_ASSERT(gen > 0);

  __debug("sending YES to best %u: gen=%u", best_candidate, gen);

  yes.any.type = EXAMSG_EVMGR_MSHIP_YES;
  yes.gen = gen;

  exa_nodeset_single(&dest, to);
  r = examsgSend(mh, EXAMSG_ADMIND_EVMGR_ID, &dest, &yes, sizeof(yes));

  EXA_ASSERT(r == sizeof(yes));
}

/**
 * Pick the best of two nodes, based on 3 criteria:
 *     (1) the node id
 *     (2) whether a node is already started
 * and (3) whether a node is leader.
 *
 * \param[in] node1  Node
 * \param[in] info1  Info on #node1
 * \param[in] node2  Node
 * \param[in] info2  Info on #node2
 *
 * The table below gives the node picked given the states of nodes n1 and n2.
 *
 * (L,A) = node state, where L = "is leader" and A = "already started".
 * min = min(n1,n2)
 * min/X = "should" be nX, but replaced with min(n1,n2) because otherwise
 *         a node seeing itself alone for some time might become a (non-
 *         started) leader and would then never agree to relinquish the
 *         leadership in favor of a better node, id-wise.
 * !!! = assert
 *
 * n1
 * v  n2>|| (0,0) | (0,1) | (1,1) | (1,0)
 * =======================================
 * (0,0) ||  min  |  n2   |   n2  | min/2
 * ------++-------+-------+-------+------
 * (0,1) ||  n1   |  min  |   n2  |  n1
 * ------++-------+-------+-------+------
 * (1,1) ||  n1   |  n1   |  !!!  |  n1
 * ------++-------+-------+-------+------
 * (1,0) || min/1 |  n2   |   n2  |  min
 *
 * \return Best of #node1 and #node2 (may be EXA_NODEID_NONE)
 */
static exa_nodeid_t
pick_best(exa_nodeid_t node1, const evmgr_mship_info_t *info1,
	  exa_nodeid_t node2, const evmgr_mship_info_t *info2)
{
#define L(info) ((info)->is_leader)
#define A(info) ((info)->already_started)

  if (node1 == node2)
    return node1;

  if (node1 == EXA_NODEID_NONE)
    return node2;

  if (node2 == EXA_NODEID_NONE)
    return node1;

  if (A(info1) && L(info1) && A(info2) && L(info2))
    EXA_ASSERT_VERBOSE(false,
		       "both node %u and node %u are leaders *and* started",
		       node1, node2);

  if (A(info1) == A(info2) && L(info1) == L(info2))
    return MIN(node1, node2);

  if (A(info1) && !A(info2))
    return node1;

  /* (min/X) Condition to *not* allow the change of leader even when
   * both nodes aren't started:
   *   if (L(info1) && !A(info1) && !L(info2) && !A(info2))
   *     return node1;
   * Replaced by condition below:
   */
  if (!A(info1) && !A(info2))
    return MIN(node1, node2);

  if (L(info1) && A(info1) && !L(info2) && A(info2))
    return node1;

  if (A(info2) && !A(info1))
    return node2;

  /* Commented out as this is part of (min/X):
   *   if (L(info2) && !A(info2) && !L(info1) && !A(info1))
   *     return node2;
   */

  if (L(info2) && A(info2) && !L(info1) && A(info1))
    return node2;

  return EXA_NODEID_NONE;

#undef A
#undef L
}

/**
 * Trigger a state change event for each node in a set of nodes.
 *
 * \param[in] node_set Nodeset
 * \param[in] state    'U' (up) or 'D' (down)
 */
static void
set_nodes_state(ExamsgHandle mh, const exa_nodeset_t *node_set, char state)
{
  exa_nodeid_t node_id;

  exa_nodeset_foreach(node_set, node_id)
    {
      const char *node_name = adm_nodeid_to_name(node_id);

      switch (state)
	{
	case 'U':
	  exalog_info("Node UP: %s", node_name);

#ifdef WITH_MONITORING
	  /* send a trap to monitoring daemon */
	  md_client_notify_node_up(mh, node_id, adm_nodeid_to_name(node_id));
#endif

	  inst_evt_up(adm_cluster_get_node_by_id(node_id));
	  break;

	case 'D':
	  exalog_info("Node DOWN: %s", node_name);

#ifdef WITH_MONITORING
	  /* send a trap to monitoring daemon */
	  md_client_notify_node_down(mh, node_id, adm_nodeid_to_name(node_id));
#endif

	  inst_evt_down(adm_cluster_get_node_by_id(node_id));
	  break;
	}
    }
}

static void handle_token(const exa_nodeset_t *mship)
{
    int quorum = __quorum();
    int num_alive = exa_nodeset_count(&membership);
    int err;

    if (token_manager == NULL)
        return;

    if (num_alive >= quorum && have_token)
    {
        err = tm_release_token(token_manager, &adm_cluster.uuid, adm_my_id);
        have_token = (err != 0 && err != -ENOENT); /* FIXME proper error code */

        if (!have_token)
            exalog_info("Released the token");
        else
            exalog_warning("Failed releasing the token: %s (%d)",
                           exa_error_msg(err), err);
    }
    else if (num_alive < quorum && !have_token)
    {
        /* XXX Could avoid requesting the token if there is no hope to
           have a quorum even with the token (ie only request it
           if num_alive + 1 >= quorum). */
        err = tm_request_token(token_manager, &adm_cluster.uuid, adm_my_id);
        have_token = (err == 0);

        if (have_token)
            exalog_info("Acquired the token");
        else
            exalog_info("Was denied the token");
    }
}

/**
 * Set the work membership to the specified new membership.
 *
 * \param[in] mh               Examsg handle
 * \param[in] new_csupd_mship  New membership
 */
static void
set_work_mship(ExamsgHandle mh, const exa_nodeset_t *new_csupd_mship)
{
  exa_nodeset_t changed, new_mship, known_nodes;
  int err;
#ifdef DEBUG
  char bin[EXA_MAX_NODES_NUMBER + 1];
  char hex[EXA_NODESET_HEX_SIZE + 1];
#endif

  /* get the nodeset of all known nodes in the cluster. This allows not to take
   * into account a node that would go up or down which we actually do not know
   */
  adm_nodeset_set_all(&known_nodes);

  exa_nodeset_copy(&new_mship, new_csupd_mship);
  exa_nodeset_intersect(&new_mship, &known_nodes);

#ifdef DEBUG
  exa_nodeset_to_bin(&new_mship, bin);
  exa_nodeset_to_hex(&new_mship, hex);

  exalog_debug("######## new work membership: %s = %s", bin, hex);
#endif

  /*
   * Nodes up
   */
  exa_nodeset_copy(&changed, &new_mship);
  exa_nodeset_substract(&changed, &membership);

  /* FIXME what are we supposed to do if fencing fails ? */
  err = examsgUnfence(mh, &changed);
  EXA_ASSERT_VERBOSE(!err, "Unfencing failed with error %d", err);

  set_nodes_state(mh, &changed, 'U');

  /*
   * Nodes down
   */
  exa_nodeset_copy(&changed, &membership);
  exa_nodeset_substract(&changed, &new_mship);

  err = examsgFence(mh, &changed);
  EXA_ASSERT_VERBOSE(!err, "Fencing failed with error %d", err);

  set_nodes_state(mh, &changed, 'D');

  /*
   * Set work membership
   */
  exa_nodeset_copy(&membership, &new_mship);
  handle_token(&membership);
}

/**
 * Set the leader
 * This function does not have any quorum consideration. This allow to have a
 * leader when the quorum is lost (and thus allow to perform fallback
 * operations between several 'lost' nodes')
 *
 * \param[in] new_leader  Id of new leader
 * \param[in] info        New leader's info
 */
static void
set_leader(exa_nodeid_t new_leader, const evmgr_mship_info_t *info)
{
  const struct adm_node *leader_node;

  leader_node = adm_cluster_get_node_by_id(new_leader);
  exalog_info("The leader is now %u:%s", leader_node->id, leader_node->name);

  adm_leader_id = new_leader;
  adm_leader_set = true;

  leader = *info;
  leader.is_leader = true;

  local.is_leader = (new_leader == adm_my_id);
  best.is_leader = (new_leader == best_candidate);
}

/**
 * Send PREPARE for the given info and leader id.
 *
 * \param     mh         Examsg handle
 * \param[in] info       Info preparing for
 * \param[in] leader_id  Id of new leader
 * \param[in] dest       Destination nodes
 */
static void
send_prepare(ExamsgHandle mh, const evmgr_mship_info_t *info,
	     exa_nodeid_t leader_id, const exa_nodeset_t *dest)
{
  evmgr_prepare_msg_t prep;
  int r;

#ifdef DEBUG
  {
    char mship_str[EXA_MAX_NODES_NUMBER + 1];
    exa_nodeset_to_bin(&info->mship, mship_str);
    __debug("sending PREPARE: gen=%u mship=%s leader_id=%u", info->gen,
	    mship_str, leader_id);
  }
#endif

  /* Send the message to all nodes in the membership */
  prep.any.type = EXAMSG_EVMGR_MSHIP_PREPARE;
  prep.info = *info;
  prep.leader_id = leader_id;

  r = examsgSend(mh, EXAMSG_ADMIND_EVMGR_ID, dest, &prep, sizeof(prep));

  EXA_ASSERT(r == sizeof(prep));
}

/**
 * Send a COMMIT message.
 *
 * \param     mh    Examsg handle
 * \param[in] gen   Membership generation
 * \param[in] dest  Destination nodes
 */
static void
send_commit(ExamsgHandle mh, sup_gen_t gen, const exa_nodeset_t *dest)
{
  evmgr_commit_msg_t commit_msg;
  int r;

  EXA_ASSERT(gen > 0);

#ifdef DEBUG
  {
    char dest_str[EXA_MAX_NODES_NUMBER + 1];
    exa_nodeset_to_bin(dest, dest_str);
    __debug("sending COMMIT: gen=%u to %s", gen, dest_str);
  }
#endif

  commit_msg.any.type = EXAMSG_EVMGR_MSHIP_COMMIT;
  commit_msg.gen = gen;

  r = examsgSend(mh, EXAMSG_ADMIND_EVMGR_ID, dest, &commit_msg,
		 sizeof(commit_msg));

  EXA_ASSERT(r == sizeof(commit_msg));
}

/* FIXME: abort is currently unused. */
#if __ABORT_ENABLED
/**
 * Send ABORT message for the given generation to the specified nodes.
 *
 * \param     mh    Examsg handle
 * \param[in] gen   Membership generation
 * \param[in] dest  Destination nodes
 */
static void
send_abort(ExamsgHandle mh, sup_gen_t gen, const exa_nodeset_t *dest)
{
  evmgr_abort_msg_t abort_msg;
  int r;

  EXA_ASSERT(gen > 0);

  /* If we have a leader, we can send abort *iff* it is us */
  EXA_ASSERT(!adm_leader_set || adm_leader_id == adm_my_id);

  __debug("sending ABORT");

  /* Send the message to all nodes in the membership */
  abort_msg.any.type = EXAMSG_EVMGR_MSHIP_PREPARE;
  abort_msg.gen = gen;

  r = examsgSend(mh, EXAMSG_ADMIND_EVMGR_ID, dest, &abort_msg, sizeof(abort_msg));

  EXA_ASSERT(r == sizeof(abort_msg));
}
#endif	/* __ABORT_ENABLED */

/**
 * Send an ACK for a given commit phase and membership generation
 * to the specified node.
 *
 * \param     mh     Examsg handle
 * \param[in] phase  Commit phase
 * \param[in] gen    Membership generation
 * \param[in] to     Destination node
 */
static void
send_ack(ExamsgHandle mh, evmgr_mship_phase_t phase, sup_gen_t gen,
	 exa_nodeid_t to)
{
  evmgr_ack_msg_t ack;
  exa_nodeset_t dest;
  int r;

  EXA_ASSERT(gen > 0);

  __debug("sending ACK to %u: phase=%s gen=%u", to, phase_names[phase], gen);

  ack.any.type = EXAMSG_EVMGR_MSHIP_ACK;
  ack.gen = gen;
  ack.phase = phase;

  exa_nodeset_single(&dest, to);
  r = examsgSend(mh, EXAMSG_ADMIND_EVMGR_ID, &dest, &ack, sizeof(ack));

  EXA_ASSERT(r == sizeof(ack));
}

/**
 * Build the nodeset of the nodes expected to reply during the commit
 * procedure of the specified memberhship.
 *
 * \param[in] mship  Membership to be prepared/committed
 *
 * \return Nodes expected to reply: specified membership minus dead nodes
 */
static const exa_nodeset_t *
expected_replies(const exa_nodeset_t *mship)
{
  static exa_nodeset_t expected;
  exa_nodeid_t node;

  exa_nodeset_reset(&expected);

  exa_nodeset_foreach(mship, node)
    if (exa_nodeset_contains(&local.mship, node))
      exa_nodeset_add(&expected, node);

#ifdef WITH_TRACE
  {
    char mship_str[EXA_MAX_NODES_NUMBER + 1], exp_str[EXA_MAX_NODES_NUMBER + 1];
    exa_nodeset_to_bin(mship, mship_str);
    exa_nodeset_to_bin(&expected, exp_str);
    __trace("expected replies for %s: from %s", mship_str, exp_str);
  }
#endif

  return &expected;
}

/**
 * Prepare for commit.
 *
 * \param[in] info        Membership & instigator info
 * \param[in] leader_id   Id of leader
 * \param[in] instigator  Node that sent the prepare order
 */
static void
prepare_cc(const evmgr_mship_info_t *info, exa_nodeid_t leader_id,
	   exa_nodeid_t instigator)
{
#ifdef WITH_TRACE
  {
    char mship_str[EXA_MAX_NODES_NUMBER + 1];
    exa_nodeset_to_bin(&info->mship, mship_str);
    __trace("gen=%u mship=%s leader_id=%u instigator=%u", info->gen, mship_str,
	    leader_id, instigator);
  }
#endif

  if (cc.phase == EVMGR_MSHIP_PHASE_PREPARE && info->gen == cc.info.gen)
    {
      __debug("already in PREPARE phase for gen=%u", cc.info.gen);
      return;
    }

  cc.phase = EVMGR_MSHIP_PHASE_PREPARE;
  cc.instigator = instigator;
  cc.info = *info;
  cc.leader_id = leader_id;

  exa_nodeset_copy(&cc.nodes_expected, expected_replies(&cc.info.mship));

  exa_nodeset_reset(&cc.nodes_prepared);
  exa_nodeset_reset(&cc.nodes_committed);
}

/**
 * Redo a prepare phase.
 */
static void
redo_prepare_cc(void)
{
  EXA_ASSERT(cc.phase == EVMGR_MSHIP_PHASE_PREPARE);

  exa_nodeset_copy(&cc.nodes_expected, expected_replies(&cc.info.mship));

  exa_nodeset_reset(&cc.nodes_prepared);
  exa_nodeset_reset(&cc.nodes_committed);
}

/**
 * Actually perform the commit.
 *
 * \param     mh          Examsg handle
 * \param[in] gen         Membership generation
 * \param[in] instigator  Node that sent the commit order
 */
static bool
commit_cc(ExamsgHandle mh, sup_gen_t gen, exa_nodeid_t instigator)
{
#ifdef WITH_TRACE
  if (instigator != cc.instigator)
    __trace("fyi: received instigator %d != cc.instigator %d",
	    instigator, cc.instigator);
#endif

  /* Assume the commit has already been done if the commit's generation
   * is older than the one of the last committed membership */
  if (gen <= last_committed_gen)
    {
      __debug("already committed gen=%u (last_committed_gen=%u)", gen,
	      last_committed_gen);
      return true;
    }

  EXA_ASSERT(cc.phase == EVMGR_MSHIP_PHASE_PREPARE);

  if (gen != cc.info.gen)
    {
      __debug("received gen %u != cc.info.gen %u", gen, cc.info.gen);
      return false;
    }

  __debug("committing gen=%u from instigator %u", cc.info.gen, cc.instigator);

  cc.phase = EVMGR_MSHIP_PHASE_COMMIT;
  last_committed_gen = gen;

  /* Commit the membership */
  set_work_mship(mh, &cc.info.mship);
  set_leader(cc.leader_id, &cc.info);

  return true;
}

/**
 * Redo a commit phase.
 */
static void
redo_commit_cc(void)
{
  EXA_ASSERT(cc.phase == EVMGR_MSHIP_PHASE_COMMIT);

  exa_nodeset_copy(&cc.nodes_expected, expected_replies(&cc.info.mship));
  exa_nodeset_copy(&cc.nodes_prepared, &cc.nodes_expected);
  exa_nodeset_reset(&cc.nodes_committed);
}

/**
 * Abort the current commit.
 *
 * \param[in] gen  Membership generation
 *
 * The current commit is aborted *iff* #gen matches the current commit's
 * generation.
 */
static void
abort_cc(sup_gen_t gen)
{
  EXA_ASSERT(cc.phase != EVMGR_MSHIP_PHASE_COMMIT);

  if (gen != cc.info.gen)
    return;

  cc.phase = EVMGR_MSHIP_PHASE_NONE;
  cc.instigator = EXA_NODEID_NONE;
}

/**
 * Finish the current commit.
 *
 * \param     mh   Examsg handle
 */
static void
finish_cc(ExamsgHandle mh)
{
  __trace("finishing commit: gen=%u", cc.info.gen);

  if (nc.pending)
    {
      __trace("a commit is pending (gen %u)", nc.info.gen);
      nc.pending = false;
      prepare_cc(&nc.info, nc.leader_id, adm_my_id);
      send_prepare(mh, &cc.info, cc.leader_id, &cc.nodes_expected);

      /* Don't trigger a recovery right now; a recovery will be
       * triggered by the finish of the pending recovery */
      return;
    }

  __trace("no commit pending");

  EXA_ASSERT(adm_is_leader());

  if (cc.info.gen <= last_finished_gen)
    {
      __trace("already finished commit gen=%u, ignoring finish", cc.info.gen);
      return;
    }

  last_finished_gen = cc.info.gen;

  /* Setting the need recovery flag must be understood as if it was a direct
   * function call of the recovery. This means that the need_recovery flag
   * can be set iff a recovery can be done, thus when matching the leader &
   * quorum conditions (Note: This part of code is executed only on the leader
   * so there is no extra check for leadership. */
  if (evmgr_has_quorum())
    need_recovery_set(true);
}

/**
 * Update the current commit with locally received info.
 *
 * \param     mh           Examsg handle
 * \param[in] local_mship  Local membership (from Csupd)
 */
static void
update_cc(ExamsgHandle mh, const exa_nodeset_t *local_mship)
{
  exa_nodeid_t node;

#ifdef WITH_TRACE
  {
    char mship_str[EXA_MAX_NODES_NUMBER + 1];
    exa_nodeset_to_bin(local_mship, mship_str);
    __trace("local_mship=%s", mship_str);
  }
#endif

  if (adm_my_id != cc.instigator)
    return;

  switch (cc.phase)
    {
    case EVMGR_MSHIP_PHASE_NONE:
      break;

    case EVMGR_MSHIP_PHASE_PREPARE:
      exa_nodeset_foreach(&cc.nodes_expected, node)
	if (!exa_nodeset_contains(local_mship, node))
	  {
	    __trace("node %u died, assuming prepared + committed gen=%u",
		    node, cc.info.gen);

	    exa_nodeset_add(&cc.nodes_prepared, node);
	    exa_nodeset_add(&cc.nodes_committed, node);
	    if (exa_nodeset_equals(&cc.nodes_prepared, &cc.nodes_expected))
	      {
		commit_cc(mh, cc.info.gen, cc.leader_id);
		send_commit(mh, cc.info.gen, &cc.nodes_prepared);
		break;
	      }
	  }
      break;

    case EVMGR_MSHIP_PHASE_COMMIT:
      exa_nodeset_foreach(&cc.nodes_expected, node)
	if (!exa_nodeset_contains(local_mship, node))
	  {
	    __trace("node %u died, assuming committed gen=%u", node,
		    cc.info.gen);

	    exa_nodeset_add(&cc.nodes_committed, node);
	    if (exa_nodeset_equals(&cc.nodes_committed, &cc.nodes_expected))
	      {
		finish_cc(mh);
		break;
	      }
	  }
      break;
    }
}

/**
 * Called when message CHANGE is received locally (from Csupd).
 *
 * \param     mh  Examsg handle
 * \param[in] mc  Membership change message
 */
void
evmgr_mship_received_local(ExamsgHandle mh, const SupEventMshipChange *mc)
{
  exa_nodeid_t prev_best;

  EXA_ASSERT(mh);
  EXA_ASSERT(mc && mc->gen > local.gen
	     && exa_nodeset_contains(&mc->mship, adm_my_id));

  local.gen = mc->gen;
  exa_nodeset_copy(&local.mship, &mc->mship);

  __debug_mship_info("received local", &local);

  /* Update current commit */
  update_cc(mh, &mc->mship);

  /* Best still alive ? */
  if (local.gen > best.gen && best_candidate != EXA_NODEID_NONE
      && !exa_nodeset_contains(&local.mship, best_candidate))
    {
      __trace("best %u (gen %u) dead in new mship (gen %u)",
	      best_candidate, best.gen, local.gen);

      best_candidate = EXA_NODEID_NONE;
      reset_info(&best);
    }

  /* Leader still alive ? */
  if (adm_leader_set && !exa_nodeset_contains(&local.mship, adm_leader_id))
    {
      __trace("leader %u dead in new mship (gen %u)", adm_leader_id, local.gen);

      adm_leader_id = EXA_NODEID_NONE;
      adm_leader_set = false;
      reset_info(&leader);
    }

  /* Systematically pick up the best candidate between best and local node
   * because if the local node is a better candidate, the other nodes do not
   * have any mean to know it, unless we send them a message. Thus, picking the
   * best here allow: - not to answer to the former best (and thus prevent it
   *                     to become leader)
   *                  - to send ready to other nodes (in case we are best)
   *                     and thus make them take us as leader
   */
  prev_best = best_candidate;
  best_candidate = pick_best(adm_my_id, &local, best_candidate, &best);
  if (best_candidate == adm_my_id)
      best = local;

  if (best_candidate != prev_best)
      __trace("i'm first in mship, best was %u, is now %u",
	      prev_best, best_candidate);
  else
      __trace("i'm first in mship, but best unchanged: %u",
	      best_candidate);

  if (best_candidate == EXA_NODEID_NONE)
    return;

  if (best_candidate == adm_my_id)
    {
      if (!adm_leader_set || adm_leader_id == adm_my_id)
	{
	  __trace("i'm best, sending READY");
	  send_ready(mh, local.gen, &local.mship);
	}
    }
  else if (match(&local, &best))
    {
      __trace("match");
      if (adm_leader_set)
	{
	  exa_nodeid_t b =
	    pick_best(best_candidate, &best, adm_leader_id, &leader);
	  if (b != best_candidate)
	    {
	      __trace("my leader is %u, best candidate is %u => dropped",
		      adm_leader_id, best_candidate);
	      return;
	    }
	}

      send_yes(mh, mc->gen, best_candidate);
    }
}

/**
 * Called when message READY is received.
 *
 * \param     mh         Examsg handle
 * \param[in] ready      Ready message
 * \param[in] sender_id  Sender of message
 */
void
evmgr_mship_received_ready(ExamsgHandle mh, const evmgr_ready_msg_t *ready,
			   exa_nodeid_t sender_id)
{
  const evmgr_mship_info_t *sender;
  exa_nodeid_t prev_best;

  EXA_ASSERT(mh);
  EXA_ASSERT(ready);
  EXA_ASSERT(EXA_NODEID_VALID(sender_id));

  if (sender_id == EXA_NODEID_LOCALHOST)
    sender_id = adm_my_id;

  sender = &ready->info;

  EXA_ASSERT(exa_nodeset_contains(&sender->mship, adm_my_id));

#ifdef DEBUG
  {
    char caption[64];
    os_snprintf(caption, sizeof(caption), "received READY from node %u", sender_id);
    __debug_mship_info(caption, sender);
  }
#endif

  if (sender->gen < local.gen)
    {
      __trace("received gen %u smaller than local gen %u => dropped",
	      sender->gen, local.gen);
      return;
    }

  /* Best still alive ? */
  if (sender->gen > best.gen && best_candidate != EXA_NODEID_NONE
      && !exa_nodeset_contains(&sender->mship, best_candidate))
    {
      __trace("best %u (gen %u) dead in sender's mship (gen %u)",
	      best_candidate, best.gen, sender->gen);

      best_candidate = EXA_NODEID_NONE;
      reset_info(&best);
    }

  prev_best = best_candidate;
  best_candidate = pick_best(sender_id, sender, best_candidate, &best);

  if (best_candidate == sender_id)
    best = *sender;

  if (best_candidate != prev_best)
    __trace("best was %u, is now %u", prev_best, best_candidate);
  else
    __trace("best unchanged: %u", best_candidate);

  if (best_candidate == sender_id && match(&local, &best))
    {
      __trace("match");
      __trace("sender %u is best", sender_id);

      if (adm_leader_set)
        {
	  exa_nodeid_t b = pick_best(sender_id, sender, adm_leader_id, &leader);
	  if (b != best_candidate)
	    {
	      __trace("my leader is %u, best candidate is %u => dropped",
		      adm_leader_id, b);
	      return;
	    }
	  else
	    __trace("my leader is %u, but %u has been picked over it",
		    adm_leader_id, b);
	}

      send_yes(mh, ready->info.gen, sender_id);
    }
}

/**
 * Set next (pending) commit.
 *
 * For use by leader (wannabe).
 *
 * Overwrites the pending membership, if any: nobody at that point
 * has started the commit procedure for this pending membership,
 * and thus it can be forgotten.
 *
 * \param[in] info       Membership and instigator info
 * \param[in] leader_id  Leader id
 */
static void
set_next_commit(const evmgr_mship_info_t *info, exa_nodeid_t leader_id)
{
#ifdef WITH_TRACE
  {
    char mship_str[EXA_MAX_NODES_NUMBER + 1];
    exa_nodeset_to_bin(&info->mship, mship_str);
    __trace("next commit: gen=%u mship=%s", info->gen, mship_str);
  }
#endif

  nc.pending = true;
  nc.info = *info;
  nc.leader_id = leader_id;
}

/**
 * Called when message YES is received.
 *
 * \param     mh         Examsg handle
 * \param[in] yes        Yes message
 * \param[in] sender_id  Sender of message
 */
void
evmgr_mship_received_yes(ExamsgHandle mh, const evmgr_yes_msg_t *yes,
			 exa_nodeid_t sender_id)
{
  EXA_ASSERT(mh);
  EXA_ASSERT(yes && yes->gen > 0);
  EXA_ASSERT(EXA_NODEID_VALID(sender_id));

  /* FIXME: check that local node is best */

  if (sender_id == EXA_NODEID_LOCALHOST)
    sender_id = adm_my_id;

  __debug("received YES from node %u: gen=%u match?=%d",
	  sender_id, yes->gen, yes->gen == local.gen);

  if (yes->gen != local.gen)
    return;

  exa_nodeset_add(&nodes_ready, sender_id);
  if (!all_ready())
    return;

  __trace("cc.phase = %s", phase_names[cc.phase]);

  switch (cc.phase)
    {
    case EVMGR_MSHIP_PHASE_NONE:
      /* FIXME: should abort here; prepare is very conservative */
      prepare_cc(&local, adm_my_id, adm_my_id);
      send_prepare(mh, &local, adm_my_id, &cc.nodes_expected);
      break;

    case EVMGR_MSHIP_PHASE_PREPARE:
      __trace("a PREPARE phase was in progress: gen=%u", cc.info.gen);
      set_next_commit(&local, adm_my_id);
      redo_prepare_cc();
      send_prepare(mh, &cc.info, cc.leader_id, &cc.nodes_expected);
      break;

    case EVMGR_MSHIP_PHASE_COMMIT:
      if (cc.instigator != adm_my_id && last_committed_gen > 0)
	{
	  __trace("a COMMIT phase was in progress: gen=%u", cc.info.gen);
	  set_next_commit(&local, adm_my_id);
	  redo_commit_cc();
	  send_commit(mh, cc.info.gen, &cc.nodes_prepared);
	}
      else
	{
	  prepare_cc(&local, adm_my_id, adm_my_id);
	  send_prepare(mh, &local, adm_my_id, &cc.nodes_expected);
	}
      break;
    }
}

/**
 * Called when message PREPARE is received.
 *
 * \param     mh         Examsg handle
 * \param[in] prep       Preparation message
 * \param[in] sender_id  Sender of message
 */
void
evmgr_mship_received_prepare(ExamsgHandle mh,
			     const evmgr_prepare_msg_t *prep,
			     exa_nodeid_t sender_id)
{
  EXA_ASSERT(mh);
  EXA_ASSERT(prep && prep->info.gen > 0
	     && exa_nodeset_contains(&prep->info.mship, adm_my_id));
  EXA_ASSERT(EXA_NODEID_VALID(prep->leader_id));
  EXA_ASSERT(EXA_NODEID_VALID(sender_id));

#ifdef DEBUG
  {
    char mship_str[EXA_MAX_NODES_NUMBER + 1];
    exa_nodeset_to_bin(&prep->info.mship, mship_str);
    __debug("received PREPARE from node %u: gen=%u mship=%s leader=%u",
	    sender_id, prep->info.gen, mship_str, prep->leader_id);
  }
#endif

  prepare_cc(&prep->info, prep->leader_id, sender_id);
  send_ack(mh, EVMGR_MSHIP_PHASE_PREPARE, prep->info.gen, sender_id);
}

/**
 * Called when message ACK is received.
 *
 * \param     mh         Examsg handle
 * \param[in] ack        Ack message
 * \param[in] sender_id  Sender of message
 */
void
evmgr_mship_received_ack(ExamsgHandle mh, const evmgr_ack_msg_t *ack,
			 exa_nodeid_t sender_id)
{
  EXA_ASSERT(mh);
  EXA_ASSERT(ack && ack->gen > 0);
  EXA_ASSERT(EXA_NODEID_VALID(sender_id));

  __debug("received ACK from node %u for phase=%s gen=%u", sender_id,
	  phase_names[ack->phase], ack->gen);

  if (ack->gen != cc.info.gen)
    return;

  switch (ack->phase)
    {
    case EVMGR_MSHIP_PHASE_NONE:
      EXA_ASSERT(false);
      break;

    case EVMGR_MSHIP_PHASE_PREPARE:
      exa_nodeset_add(&cc.nodes_prepared, sender_id);
      if (exa_nodeset_equals(&cc.nodes_prepared, &cc.nodes_expected))
	{
	  commit_cc(mh, cc.info.gen, cc.leader_id);
	  send_commit(mh, cc.info.gen, &cc.nodes_prepared);
	}
      break;

    case EVMGR_MSHIP_PHASE_COMMIT:
      exa_nodeset_add(&cc.nodes_committed, sender_id);
      if (exa_nodeset_equals(&cc.nodes_committed, &cc.nodes_expected))
	finish_cc(mh);
      break;
    }
}

/**
 * Called when message COMMIT is received.
 *
 * \param     mh         Examsg handle
 * \param[in] commit     Commit message
 * \param[in] sender_id  Sender of message
 *
 * \return true if commit done, false if not (ignored)
 */
bool
evmgr_mship_received_commit(ExamsgHandle mh, const evmgr_commit_msg_t *commit,
			    exa_nodeid_t sender_id)
{
  EXA_ASSERT(mh);
  EXA_ASSERT(commit && commit->gen > 0);
  EXA_ASSERT(EXA_NODEID_VALID(sender_id));

  __debug("received COMMIT from node %u: gen=%u", sender_id, commit->gen);

  if (!commit_cc(mh, commit->gen, sender_id))
    return false;

  send_ack(mh, EVMGR_MSHIP_PHASE_COMMIT, commit->gen, sender_id);

  return true;
}

/**
 * Called when message ABORT is received.
 *
 * \param     mh         Examsg handle
 * \param[in] abort_msg  Abortion message
 * \param[in] sender_id  Sender of message
 */
void
evmgr_mship_received_abort(ExamsgHandle mh, const evmgr_abort_msg_t *abort_msg,
			   exa_nodeid_t sender_id)
{
  EXA_ASSERT(mh);
  EXA_ASSERT(abort_msg);
  EXA_ASSERT(EXA_NODEID_VALID(sender_id));

  __debug("received ABORT from node %u: gen=%u", sender_id, abort_msg->gen);

  abort_cc(abort_msg->gen);
}
