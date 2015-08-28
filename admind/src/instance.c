/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/instance.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_command.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr_mship.h"
#include "admind/src/rpc.h"
#include "admind/src/rpc_command.h"
#include "common/include/exa_error.h"
#include "common/include/exa_nodeset.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"

#ifdef USE_YAOURT
#include <yaourt/yaourt.h>
#endif

typedef struct {
  adm_service_id_t service_id;

  inst_op_t        op;
  exa_nodeset_t    involved_in_op;

  /** committed_up is updated only on node up. Other nodes already have
   * the committed_up */
  exa_nodeset_t committed_up;
} inst_sync_before_recovery_t;

typedef struct {
  exa_nodeset_t committed_up;
  bool resources_changed_up;
  bool resources_changed_down;
  adm_service_id_t service_id;
  uint32_t nb_check_handled;
} inst_sync_after_recovery_t;

/**
 * Describes the current state of a service.
 * This structure stores the current state and the needed operations that
 * are concerning a service for the recovery mechanism. */
typedef struct adm_service_state
{
  /*********** STATE STUFF ****************/

  /** Set of instances that were _completely_ recovered up at least once
   * since the last time they went down. NOTE: an instance that did not
   * finished its recovery up is considered as down and thus is _NOT_ in this
   * set. */
  exa_nodeset_t committed_up;

  /** Number of pending check down events */
  uint32_t check_down_needed;

  /** Number of pending check up events */
  uint32_t check_up_needed;

  /** A down event occurs since the last recovery of this service */
  exa_nodeset_t down_needed;

  /** A up event occured */
  exa_nodeset_t up_needed;

  /* The status of one or more of the resources used by this service has
   * changed.
   * FIXME This is badly named or misplaced: this means that an OTHER service
   * changed, so this is not really part of the CURRENT service state...
   * The problem here is ownership: who is responsible for this field ? */
  bool resources_changed_down;
  bool resources_changed_up;

  /******** RECOVERY STUFF **********/
  /** Set of instances that triggered the current op_in_progress */
  exa_nodeset_t involved_in_op;
  inst_op_t op_in_progress; /**< type of the current recovery */

} adm_service_state_t;

/** Lock.
 *
 * Fields csupd_up, check_needed, down_needed are protected by this
 * lock from concurrency between the event manager thread and the recovery
 * working thread. Other fields are not protected because they are used only
 * by the recovery working thread.
 * FIXME this mutex does not know what it is supposed to protect and do not
 * protect it completely anyway... There would probably need a lock each
 * service own data....
 */
static os_thread_mutex_t mutex;
#define LOCK() os_thread_mutex_lock(&mutex)
#define UNLOCK() os_thread_mutex_unlock(&mutex)

void inst_static_init(void)
{
  os_thread_mutex_init(&mutex);
}

/** Accessor on the states array, one should ALWAYS use it to retrieve data
 * from the service states table.
 * FIXME there would probably need a per field accessor, but this can be done
 * only when locking is correctly done.. so for now, access is centralized here
 */
static inline adm_service_state_t *state_of(adm_service_id_t id)
{
    /** Array that stores each service state. This can be considered as private
     * data to this file; Array is 'supposed' to be portected by the lock
     * declared here before. FIXME use the lock... /!\ bug #2986 */
    static adm_service_state_t states[ADM_SERVICE_LAST + 1];

    EXA_ASSERT(ADM_SERVICE_ID_IS_VALID(id));
    return &states[id];
}

/* --- inst_op2str ------------------------------------------------ */
/**
 * Convert inst_op_t to string
 *
 * \param[in] op          Type of operation (up, down, check)
 *
 * \return char*
 */
static char *
inst_op2str(inst_op_t op)
{
  switch(op)
    {
      case INST_OP_NOTHING:
        return "NOTHING";
      case INST_OP_CHECK_DOWN:
        return "CHECK_DOWN";
      case INST_OP_CHECK_UP:
        return "CHECK_UP";
      case INST_OP_DOWN:
        return "DOWN";
      case INST_OP_UP:
        return "UP";
    }
  EXA_ASSERT(false);
  return NULL;
}

/* --- inst_dump -------------------------------------------------- */
/**
 * Dump instance hashtable to exalog, using logitem
 *
 * \return void
 */
static void
inst_dump(void)
{
  const struct adm_service *service;
#define _dump_set(set_name) \
      do {\
	  char buffer[EXA_MAX_NODES_NUMBER + 1];\
	  exa_nodeset_to_bin(&state_of(service->id)->set_name, buffer);\
	  exalog_debug("%s: %s=%s", adm_service_name(service->id),\
                                    #set_name, buffer);\
      } while (0);

  adm_service_for_each(service)
  {
    if (state_of(service->id)->op_in_progress != INST_OP_NOTHING)
      exalog_debug("%s: op_in_progress=%s", adm_service_name(service->id),
		   inst_op2str(state_of(service->id)->op_in_progress));

    _dump_set(involved_in_op);
    _dump_set(committed_up);
    _dump_set(up_needed);
    _dump_set(down_needed);
    exalog_debug("Check down=%"PRIu32" up=%"PRIu32,
	    state_of(service->id)->check_down_needed,
	    state_of(service->id)->check_up_needed);
    if (state_of(service->id)->resources_changed_up)
      exalog_debug("%s: resources_changed_up=true", adm_service_name(service->id));
    if (state_of(service->id)->resources_changed_down)
      exalog_debug("%s: resources_changed_down=true", adm_service_name(service->id));
  }
}


/*************************************************************************
 * Accessed ONLY FROM EVMGR
 */

/* --- inst_evt_up ------------------------------------------------ */
/**
 * Register the event "node up" from csupd in the hashtable and wake
 * up the recovery working thread to run the recovery.
 *
 * \param[in] node_name   The node that become UP
 *
 * \return void
 */
void inst_evt_up(const struct adm_node *node)
{
  const struct adm_service *service;

#ifdef USE_YAOURT
  yaourt_event_wait(EXAMSG_ADMIND_EVMGR_ID, "inst_evt_up %s", node->name);
#endif

  LOCK();

  adm_service_for_each(service)
  {
      adm_service_state_t *state = state_of(service->id);
      exa_nodeset_add(&state->up_needed, node->id);

      /* If the node was not up nor going up, no need to keep track
       * of the down operation. The node went down, the recovery was done
       * once, then it came back, but nothing was done, then down, but
       * nothing was needed to be done, then up, and we need to recover it */
      if (!exa_nodeset_contains(&state->committed_up, node->id)
	  && !(exa_nodeset_contains(&state->involved_in_op, node->id)
	       && state->op_in_progress == INST_OP_UP))
          exa_nodeset_del(&state->down_needed, node->id);
  }

  UNLOCK();
}

/* --- inst_evt_down ---------------------------------------------- */
/**
 * Register the event "node down" from csupd in the hashtable and wake
 * up the recovery working thread to run the recovery.
 *
 * \param[in] node_name   The node that become DOWN
 *
 * \return void
 */
void inst_evt_down(const struct adm_node *node)
{
  const struct adm_service *service;

#ifdef USE_YAOURT
  yaourt_event_wait(EXAMSG_ADMIND_EVMGR_ID, "inst_evt_down %s", node->name);
#endif

  LOCK();

  adm_service_for_each(service)
  {
      adm_service_state_t *state = state_of(service->id);

      /* The instance is going down before we had time to recover it...
       * anyway, forget about it */
      exa_nodeset_del(&state->up_needed, node->id);

      /* Filter if there is no need to do a recovery.
       * Thus nothing to do when the instance was not up nor going up.
       * This means that an instance that is currently beeing recovered up
       * will be recovered down if it crashes. */
      if (!exa_nodeset_contains(&state->committed_up, node->id)
	  && !(exa_nodeset_contains(&state->involved_in_op, node->id)
	       && state->op_in_progress == INST_OP_UP))
	  continue;

      /* If the service is not up locally, there is no down operation
       * to perform: When the local node performs the UP, it will get the
       * list of nodes that are committed UP which will not contain the node
       * that has just failed. */
      if (!adm_nodeset_contains_me(&state->committed_up)
          && !(adm_nodeset_contains_me(&state->involved_in_op)
               && state->op_in_progress == INST_OP_UP
               && evmgr_has_quorum()))
	  continue;

      exa_nodeset_add(&state_of(service->id)->down_needed, node->id);
  }

  UNLOCK();

  exalog_debug("=== After node down for %s ===", node->name);
  inst_dump();
}

/* --- inst_evt_check_down ---------------------------------------- */
/**
 * Register the event "node check" from csupd in the hashtable and
 * wake up the recovery working thread to run the recovery.
 *
 * \param[in] node_name   The node that need to be checked
 *
 * \return void
 */
void inst_evt_check_down(const struct adm_node *node,
	                 const struct adm_service *service)
{
  LOCK();

  state_of(service->id)->check_down_needed++;

  UNLOCK();
}

/* --- inst_evt_check_up ------------------------------------------ */
/**
 * Register the event "node check" from csupd in the hashtable and
 * wake up the recovery working thread to run the recovery.
 *
 * \param[in] node_name   The node that need to be checked
 *
 * \return void
 */
void inst_evt_check_up(const struct adm_node *node,
	               const struct adm_service *service)
{
  LOCK();

  state_of(service->id)->check_up_needed++;

  UNLOCK();
}

/* --- inst_set_all_instances_down -------------------------------- */
/**
 * Set all instances status to down.
 *
 * Used by clstop.
 *
 * \return void
 */
void inst_set_all_instances_down(void)
{
    const struct adm_service *service;

    adm_service_for_each(service)
    {
	adm_service_state_t *state = state_of(service->id);
	exa_nodeset_reset(&state->up_needed);
	state->check_down_needed = 0;
	state->check_up_needed = 0;
	exa_nodeset_reset(&state->down_needed);
	exa_nodeset_reset(&state->committed_up);
    }
}

/* --- inst_op_wanted_by_service --------------------------------- */
/**
 * Does this service want this operation ?
 *
 * The lock must be taken by the caller.
 */
static int
inst_op_wanted_by_service(const adm_service_state_t *state, inst_op_t op)
{
  exa_nodeset_t up_needed;

  /* If there is no node committed up, nothing to do: this, as a matter of
   * fact, prevents a node going up to perform a node down (or any other
   * operation). This is needed by the current framework, but really depends
   * on the design of the recovery. I keep this behaviour like this until
   * the behaviour is really settled in design. */
  if (op != INST_OP_UP && exa_nodeset_is_empty(&state->committed_up))
      return false;

  switch (op)
  {
    case INST_OP_CHECK_DOWN:
      return state->check_down_needed > 0;

    case INST_OP_DOWN:
      return !exa_nodeset_is_empty(&state->down_needed)
	     || state->resources_changed_down;

    case INST_OP_UP:
      exa_nodeset_copy(&up_needed, &state->up_needed);
      /* FIXME how actually an instance can be up_needed and committed up ?
       * The only situation seems to be when a node/instance is going down,
       * but as the DOWN is prioritary, the current operation should not be
       * INST_OP_UP */
      exa_nodeset_substract(&up_needed, &state->committed_up);
      return !exa_nodeset_is_empty(&up_needed) || state->resources_changed_up;

    case INST_OP_CHECK_UP:
      return state->check_up_needed > 0;

    case INST_OP_NOTHING:
      return false;

    default:
      EXA_ASSERT_VERBOSE(false, "Unknown operation %d", op);
      return false;
  }
}

/* --- inst_op_wanted -------------------------------------------- */
/**
 * Does one of the services want this operation ?
 *
 * The lock must be taken by the caller.
 */
static int
inst_op_wanted(inst_op_t op)
{
  const struct adm_service *service;

  adm_service_for_each(service)
  {
    if (inst_op_wanted_by_service(state_of(service->id), op))
      return true;
  }

  return false;
}

/* --- inst_which_recovery ---------------------------------------- */
/**
 * Which recovery (down, up, check) we want the hierarchy ?
 *
 * The lock must be taken by the caller.
 *
 * \return inst_op_t: the recovery needed for the hierarchy
 */
static inst_op_t
inst_which_recovery(void)
{
  /* 1st priority: check down */
  if (inst_op_wanted(INST_OP_CHECK_DOWN))
    return INST_OP_CHECK_DOWN;

  /* 2nd priority: down */
  if (inst_op_wanted(INST_OP_DOWN))
    return INST_OP_DOWN;

  /* 3th priority: up */
  if (inst_op_wanted(INST_OP_UP))
    return INST_OP_UP;

  /* 4th priority: check up */
  if (inst_op_wanted(INST_OP_CHECK_UP))
    return INST_OP_CHECK_UP;

  /* 5th priority: nothing to do */
  return INST_OP_NOTHING;
}

/**
 * Establish the nodeset op_in_progress for a given service and for a given
 * operation.
 * Caller MUST hold the LOCK and MUST be evmgr
 * \param[in] state      a service state
 * \param[in] op         the recovery operation we are about to perform
 */
static void
inst_compute_involved_in_op(adm_service_state_t *state,
			    inst_op_t op)
{
  if (!inst_op_wanted_by_service(state, op))
  {
    state->op_in_progress = INST_OP_NOTHING;
    exa_nodeset_reset(&state->involved_in_op);
    return;
  }

  switch (op)
  {
    case INST_OP_CHECK_DOWN:
    case INST_OP_CHECK_UP:
      exa_nodeset_reset(&state->involved_in_op);
      break;

    case INST_OP_DOWN:
      exa_nodeset_copy(&state->involved_in_op, &state->down_needed);
      break;

    case INST_OP_UP:
      exa_nodeset_copy(&state->involved_in_op, &state->up_needed);
      exa_nodeset_substract(&state->involved_in_op, &state->committed_up);
      break;

    case INST_OP_NOTHING:
      /* This is not supposed to happen as the above inst_op_wanted_by_service
       * returns false for INST_OP_NOTHING */
    default:
      EXA_ASSERT_VERBOSE(false, "Something went wrong");
      break;
  }
  state->op_in_progress = op;
}


/* --- inst_compute_recovery --------------------------------------- */
/**
 * Compute the recovery that is needed
 *
 * \return the recovery to perform
 */
inst_op_t inst_compute_recovery(void)
{
  inst_op_t op;
  const struct adm_service *service;
#ifdef USE_YAOURT
  bool interrupted = false;
#endif

  LOCK();

  /* Get the kind of recovery that is required */
  op = inst_which_recovery();

  /* dump debug */
  exalog_debug("=== Before recovery %s (begin) ===", inst_op2str(op));
  inst_dump();

  adm_service_for_each(service)
  {
    adm_service_state_t *s_state = state_of(service->id);

#ifdef USE_YAOURT
    if (s_state->op_in_progress != INST_OP_NOTHING)
      interrupted = true;
#endif

    /* Include the instance in the recovery */
    inst_compute_involved_in_op(s_state, op);
  }

  /* dump debug */
  exalog_debug("=== Before recovery (middle) ===");
  inst_dump();

  UNLOCK();

#ifdef USE_YAOURT
  yaourt_event_wait(EXAMSG_ADMIND_EVMGR_ID,
                    "recovery_interrupted %d", interrupted);
#endif

  return op;
}

/*
 * End ONLY FROM EVMGR
 *****************************************************************************/



/***************************************************************************
 * ONLY FROM RPC.c
 */

/* --- inst_get_current_membership -------------------------------- */
/**
 * Get the current membership for local commands (rpc.c)
 *
 * \param[in] service     Service
 * \param[out] membership The membership to set
 *
 * \return void
 */
void inst_get_current_membership(int thr_nb, const struct adm_service *service,
                                 exa_nodeset_t *membership)
{
    /* Special case for local recovery down when there is no quorum.  We handle
     * an instance down for ourself so we have to take part in the local commands
     * and barriers. For now, this is needed only for nodestop command. */
    if (!evmgr_has_quorum())
    {
	exa_nodeset_reset(membership);
	exa_nodeset_add(membership, adm_my_id);
	return;
    }

    inst_get_nodes_up(service, membership);

    /* FIXME for sure, what is coming next is a really ugly hack....
     * This function is supposed to return the mship on which a command must be
     * done, but as it does not really know what is going on (because this is
     * called inside rpc.c) it tries to guess the result by looking at the
     * thread id.... */
    if (thr_nb == RECOVERY_THR_ID)
    {
	/* As the rpc is supposed to be done only on node that are making
	 * part of the recovery process, it is needed to add the instances
	 * that are not marked "up" but that are involved. */
	if (state_of(service->id)->op_in_progress == INST_OP_UP)
	    exa_nodeset_sum(membership, &state_of(service->id)->involved_in_op);

	/* On another hand, instances that are going down are removed from
	 * the participants. */
	if (state_of(service->id)->op_in_progress == INST_OP_DOWN)
	    exa_nodeset_substract(membership, &state_of(service->id)->involved_in_op);

        /* We remove the nodes for which a check down is needed. This is a fix
         * for the bug #4589 which happened when a check down was required and
         * a node down was detected before the recovery for the check down had
         * begun. */
	if (state_of(service->id)->op_in_progress == INST_OP_CHECK_DOWN)
            exa_nodeset_substract(membership, &state_of(service->id)->down_needed);
    }
}

/* --- inst_is_node_down ------------------------------------------ */
/**
 * Check if a service marked an instance of the node 'node' as down_needed
 * This is used only by the recovery thread and should remain so.
 *
 * Used only by rpc.c AND MUST BE USED ONLY THERE !
 *
 * \param[out] node       The node name to check
 *
 * \return true if the node became down, false if it is still up
 */
bool inst_is_node_down_rec(exa_nodeid_t nid)
{
    const struct adm_service *service;

    adm_service_for_each(service)
	if (exa_nodeset_contains(&state_of(service->id)->down_needed, nid))
	    return true;

  return false;
}

/*
 * Tells if any instance on the node 'node' is not committed up
 * This is needed for info and command threads to know if they should wait
 * an answer from a node.
 */
bool inst_is_node_down_cmd(exa_nodeid_t nid)
{
    const struct adm_service *service;

    /* FIXME this function should take the service to check as param as
     * checking any service causes races. (maybe one service is not
     * committed up but another is... */
    adm_service_for_each(service)
	if (!exa_nodeset_contains(&state_of(service->id)->committed_up, nid))
	    return true;

    /* FIXME there is a race here: if a node went down completely and was seen
     * up and recovered completely, this function will never return that the
     * node failed when actually it did. */
    /* FIXME: rpc are used outside of the recovery, node  marked down (ie the
     * revovery was completely done) while a command was running are not taken
     * into account in local command, nor in barrier.
     * This case can only happen in the command thread, because it is a race
     * between the recovery thread and the command thread (and obviously, the
     * recovery thread cannot have a race with itself...).
     * Actually, this race can happen because the failure detection is based
     * on polling of states and not the real reception of events down; thus
     * if a node passed down and returns up during a command the event down
     * happened, but if the local command did not check it a the rigth moment,
     * it just miss it, and then the local command may wait for an answer from
     * a node that will never answer... (because it forgot everything about it
     * when restarting) */

  return false;
}

/*
 * END ONLY FROM RPC.C
 **************************************************************************************/



/***********************************************************************
 * CALLED ONLY FROM clnodeadd
 */
int inst_node_add(struct adm_node *node)
{
  const struct adm_service *service;

  adm_service_for_each(service)
  {
    EXA_ASSERT(!exa_nodeset_contains(&state_of(service->id)->up_needed, node->id));
    EXA_ASSERT(state_of(service->id)->check_down_needed == 0);
    EXA_ASSERT(state_of(service->id)->check_up_needed == 0);
    EXA_ASSERT(!exa_nodeset_contains(&state_of(service->id)->down_needed, node->id));
    EXA_ASSERT(!exa_nodeset_contains(&state_of(service->id)->involved_in_op, node->id));
    EXA_ASSERT(!exa_nodeset_contains(&state_of(service->id)->committed_up, node->id));
  }

  return EXA_SUCCESS;
}

/* END CALLED ONLY FROM clnodeadd
 ***********************************************************************/

/***********************************************************************
 * CALLED ONLY FROM clnodedel
 */
void inst_node_del(struct adm_node *node)
{
  const struct adm_service *service;

  adm_service_for_each(service)
  {
    EXA_ASSERT(!exa_nodeset_contains(&state_of(service->id)->up_needed, node->id));
    EXA_ASSERT(state_of(service->id)->check_down_needed == 0);
    EXA_ASSERT(state_of(service->id)->check_up_needed == 0);
    EXA_ASSERT(!exa_nodeset_contains(&state_of(service->id)->down_needed, node->id));
    EXA_ASSERT(!exa_nodeset_contains(&state_of(service->id)->involved_in_op, node->id));
    EXA_ASSERT(!exa_nodeset_contains(&state_of(service->id)->committed_up, node->id));
  }
}

/* END CALLED ONLY FROM clnodedel
 ***********************************************************************/

/**
 * Tells  that a resource USED by the given service went down
 * \param[in] service     Service
 */
void inst_set_resources_changed_down(const struct adm_service *service)
{
    /* Any call to recovey handles changes up/down on resources, thus
     * as this service is not up, the resource change will be handled during
     * its future recovery up */
    if (adm_nodeset_contains_me(&state_of(service->id)->committed_up))
	state_of(service->id)->resources_changed_down = true;
}

/**
 * Tells that a resource USED by the given service went up
 * \param[in] service     Service
 */
void inst_set_resources_changed_up(const struct adm_service *service)
{
    /* Any call to recovey handles changes up/down on resources, thus
     * as this service is not up, the resource change will be handled during
     * its future recovery up */
    if (adm_nodeset_contains_me(&state_of(service->id)->committed_up))
	state_of(service->id)->resources_changed_up = true;
}

/* --- inst_get_nodes_going_up ------------------------------ */
/**
 * Get the list of node up in progress
 *
 * Warning: the code should work for the recovery working thread but
 *          is unspecified for other working threads.
 *
 * \param[in] service     Service
 * \param[out] nodes      The list of nodes to build
 *
 * \return void
 */
void inst_get_nodes_going_up(const struct adm_service *service,
				   exa_nodeset_t* nodes)
{
  if (state_of(service->id)->op_in_progress == INST_OP_UP)
    exa_nodeset_copy(nodes, &state_of(service->id)->involved_in_op);
  else
    exa_nodeset_reset(nodes);
}

/* --- inst_get_nodes_going_down ---------------------------- */
/**
 * Get the list of node down in progress
 *
 * Warning: the code should work for the recovery working thread but
 *          is unspecified for other working threads.
 *
 * \param[in] service     Service
 * \param[out] nodes      The list of nodes to build
 *
 * \return void
 */
void inst_get_nodes_going_down(const struct adm_service *service,
			       exa_nodeset_t* nodes)
{
  if (state_of(service->id)->op_in_progress == INST_OP_DOWN)
    exa_nodeset_copy(nodes, &state_of(service->id)->involved_in_op);
  else
    exa_nodeset_reset(nodes);
}

/* --- inst_get_nodes_up ------------------------------------------ */
/**
 * Get the list of instances currently up ie that are completely recovered
 * (correspond to committed up)
 *
 * Warning: the code should work for the recovery working thread but
 *          is unspecified for other working threads.
 *
 * \param[in] service     Service
 * \param[out] nodes      the nodeset of instances that are committed up
 *
 * \return void
 */
void inst_get_nodes_up(const struct adm_service *service,
		       exa_nodeset_t* nodes_up)
{
  exa_nodeset_copy(nodes_up, &state_of(service->id)->committed_up);
}

/* --- inst_get_nodes_down ---------------------------------------- */
/**
 * Get the list of node currently down in the current local command
 *
 * Usefull for services.
 *
 * Warning: the code should work for the recovery working thread but
 *          is unspecified for other working threads.
 *
 * \param[in] service     Service
 * \param[out] nodes      The list of nodes to build
 *
 * \return void
 */
void inst_get_nodes_down(const struct adm_service *service,
			 exa_nodeset_t* nodes_down)
{
  exa_nodeset_t nodes_up;

  adm_nodeset_set_all(nodes_down);
  inst_get_nodes_up(service, &nodes_up);
  exa_nodeset_substract(nodes_down, &nodes_up);
}

/* --- inst_is_node_stopped --------------------------------------- */
/**
 * Check if a node has all services csupd & committed down.
 *
 * \param[out] node       The node name to check
 *
 * \return true if the node is down, false if one service is still up
 */
int inst_is_node_stopped(const struct adm_node *node)
{
  const struct adm_service *service;

  exalog_debug("=== inst_is_node_stopped ===");
  inst_dump();

  adm_service_for_each(service)
  {
    /* The node is waiting for a recovery up which is actually not the same
     * thing than being stopped */
    if (exa_nodeset_contains(&state_of(service->id)->up_needed, node->id))
      return false;
    if (exa_nodeset_contains(&state_of(service->id)->committed_up, node->id))
      return false;
  }

  return true;
}

/* --- inst_check_blockable_event --------------------------------- */
/**
 * Check if an event might block daemon_query.
 *
 * The event might be either an instance down (a node is down for
 * example) or an instance check (a disk is down for example).
 *
 * No need to look in the resource hashtable because such events must
 * be found by looking in the check_needed flag.
 *
 * \return true if an event might block daemon_query, false otherwise
 */
int inst_check_blockable_event(void)
{
  char nodes_str[EXA_MAXSIZE_HOSTSLIST + 1];
  const struct adm_service *service;

  /* Locking is useless here. There is no consistency issue. Moreover futexes
     are known to block under memory pressure and we are not allowed to block
     here (see bug #2571). */

  adm_service_for_each(service)
  {
    exa_nodeset_t committed_up, down_needed;

    /* Don't interrupt a recovery DOWN of a node by itself. Otherwise the
       recovery could loop indefinitely. */
    exa_nodeset_copy(&down_needed, &state_of(service->id)->down_needed);
    if (state_of(service->id)->op_in_progress == INST_OP_DOWN)
      exa_nodeset_substract(&down_needed, &state_of(service->id)->involved_in_op);

    /* Get the instances that are actually completely up as it is considered
     * that an instance that is not completely up is down; this
     * relies on the fact that a service N cannot be commited up on a node
     * if the instance of service N-1 is not commited up on the node.
     * This makes the deadlock impossible because a service N can be blocked
     * only if the services N-X that are below in the service stack are
     * waiting for a recovery down, but if the N recovery is being performed,
     * the N-X services are necessarily commited up.
     * FIXME check this very seriously... */
    inst_get_nodes_up(service, &committed_up);
    if (!exa_nodeset_disjoint(&down_needed, &committed_up))
    {
      adm_nodeset_to_names(&state_of(service->id)->down_needed, nodes_str, sizeof(nodes_str));
      exalog_debug("Query interrupted by a node down of %s", nodes_str);
      return true;
    }

    /* FIXME The next test makes impossible to use admwrk_daemon_query in
     * check_down callbacks. Hopefully, there are not used in there for now.
     * FIXME It would really be a good thing for rpc admwrk_query to be
     * notified of down event and not to look in internal recovery
     * structures... */
    if (state_of(service->id)->check_down_needed > 0)
    {
      exalog_debug("Query interrupted by a resource down in service '%s'",
	           adm_service_name(service->id));
      return true;
    }
  }

  return false;
}

static void
inst_sync_before_recovery(int thr_nb, cl_error_desc_t *err_desc)
{
  int ret = EXA_SUCCESS;
  const struct adm_service *service;

  /* sync check_needed, down_needed and op_in_progress on slave nodes */
  adm_service_for_each(service)
  {
    inst_sync_before_recovery_t msg;
    adm_service_state_t *state = state_of(service->id);

    /* If service does not want a recovery, do not try to sync data.
     * This is made necessary because inst_sync_before_recovery perform a sync
     * of all services at the time when a service need a recovery, even if all
     * others are not started at all (committed up empty). Thus this prevent
     * trying to perform a sync operation with actually nobody on these very
     * services. */
    if (state->op_in_progress == INST_OP_NOTHING)
	continue;

    /* Build the message */
    memset(&msg, 0, sizeof(msg));

    msg.service_id = service->id;
    msg.op = state->op_in_progress;
    exa_nodeset_copy(&msg.involved_in_op, &state->involved_in_op);
    exa_nodeset_copy(&msg.committed_up, &state->committed_up);

    /* Send the message */
    ret = admwrk_exec_command(thr_nb, service, RPC_INST_SYNC_BEFORE_RECOVERY, &msg, sizeof(msg));
    EXA_ASSERT(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NODE_DOWN);

    /* Synchronization was interrupted by a node down, thus a new comutation
     * is needed, so we abort here, anyway the next recovery will end the work */
    if (ret == -ADMIND_ERR_NODE_DOWN)
	break;
  }

  /* dump debug */
  exalog_debug("=== Before recovery (end) ===");
  inst_dump();

  set_error(err_desc, ret, NULL);
}

/* --- inst_local_sync_before_recovery ---------------------------- */
/**
 * Local function called with EXAMSG_INST_SYNC_BEFORE_RECOVERY
 *
 * \param[in] thr_nb      Thread number
 * \param[in] data        data received
 *
 * \return void
 */
static void
inst_local_sync_before_recovery(int thr_nb, void *data)
{
  inst_sync_before_recovery_t *msg = data;
  adm_service_state_t *state;

  LOCK();

  /* Find the struct instance to update */
  state = state_of(msg->service_id);

  /* Update the hashtable */
  state->op_in_progress = msg->op;
  exa_nodeset_copy(&state->involved_in_op, &msg->involved_in_op);

  /* Nodes that are comming back do not know that a cluster was already
   * existing (some nodes are already up and completely functional), thus the
   * leader sends the current commited up nodes to inform new comers of the
   * current state of the service. But actually, from the new comer point of
   * view, those nodes are not up (totally handled locally) but going up.
   * Thus they are added to the list of nodes we are recovering up locally. */
  if (msg->op == INST_OP_UP && adm_nodeset_contains_me(&msg->involved_in_op))
      exa_nodeset_sum(&state->involved_in_op, &msg->committed_up);
  else
      /* If a node was already present in the cluster (ie, it is not in the
       * node_going_up list) it MUST have the set commited up status (otherwise
       * some unexpected behaviour happened) */
      EXA_ASSERT(exa_nodeset_equals(&state->committed_up, &msg->committed_up));

  UNLOCK();

  admwrk_ack(thr_nb, EXA_SUCCESS);
}

static void
__inst_sync_after_recovery(int thr_nb, const struct adm_service *service,
                           const exa_nodeset_t *committed_up,
                           uint32_t nb_check_handled)
{
  inst_sync_after_recovery_t msg;
  int ret;

  /* Build the message: copy raw states */
  msg.service_id = service->id;
  msg.nb_check_handled = nb_check_handled;

  exa_nodeset_copy(&msg.committed_up, committed_up);

  /* Send the message */
  ret = admwrk_exec_command(thr_nb, service, RPC_INST_SYNC_AFTER_RECOVERY, &msg, sizeof(msg));
  /* ignore -ADMIND_ERR_NODE_DOWN errors because the commit operation worked on
   * all nodes that are *alive*; those which dies are not important here */
  EXA_ASSERT(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NODE_DOWN);
}


/* --- inst_sync_after_recovery ---------------------------------------- */
/**
 * Commit instance status in the hashtable. This function must be
 * called only if the recovery was not interrupted.
 *
 * No need to take the lock because we don't use csupd_up or
 * *_needed flags.
 *
 * \return void
 */
static void
inst_sync_after_recovery(int thr_nb, const struct adm_service *service,
	                 uint32_t nb_check_handled)
{
  const adm_service_state_t *state = state_of(service->id);
  exa_nodeset_t committed_up;

  exa_nodeset_copy(&committed_up, &state->committed_up);

  /* When the recovery of the service is finished, all nodes inside the
   * involved_in_op list are successfully integrated (because the recovery
   * never fails) thus they can be added to the commited up list (the list of
   * the fully recovered nodes) */
  if (state->op_in_progress == INST_OP_UP)
    exa_nodeset_sum(&committed_up, &state->involved_in_op);

  /* Same thing than above, but as the current operation was removing a node,
   * when it finishes, the node can be safely removed from the nodes up */
  if (state->op_in_progress == INST_OP_DOWN)
    exa_nodeset_substract(&committed_up, &state->involved_in_op);

  /* No need to lock because we don't touch csupd_up and *_check */
  __inst_sync_after_recovery(thr_nb, service, &committed_up, nb_check_handled);
}


/* --- inst_local_sync_after_recovery ----------------------------- */
/**
 * Local function called with EXAMSG_INST_SYNC_AFTER_RECOVERY
 *
 * \param[in] thr_nb      Thread number
 * \param[in] data        data received
 *
 * \return void
 */
static void
inst_local_sync_after_recovery(int thr_nb, void *data)
{
  inst_sync_after_recovery_t *msg = data;
  adm_service_state_t *state;

  LOCK();

  /* Find the struct instance to update */
  state = state_of(msg->service_id);

  /* Commit instance locally. */
  /* FIXME here some kind of transactional behaviour would really be fine:
   * local nodes could acknowledge IIF they agree on the new commited up */
  exa_nodeset_copy(&state->committed_up, &msg->committed_up);

  switch (state->op_in_progress)
  {
      case INST_OP_UP:
	  state->resources_changed_up = false;
	  break;

      case INST_OP_DOWN:
	  exa_nodeset_substract(&state->down_needed, &state->involved_in_op);
	  state->resources_changed_down = false;
	  break;

      case INST_OP_CHECK_DOWN:
	  EXA_ASSERT_VERBOSE(state->check_down_needed >= msg->nb_check_handled,
		  "Inconsistent values for check down '%"PRIu32"' '%"PRIu32"'",
		  state->check_down_needed, msg->nb_check_handled);
	  state->check_down_needed -= msg->nb_check_handled;
	  break;

      case INST_OP_CHECK_UP:
          if (state->check_up_needed < msg->nb_check_handled)
          {
              /* See bug #4601 */
	      exalog_warning(
                  "Leader asked to reset more check up (%"PRIu32") than I saw (%"PRIu32")",
                  msg->nb_check_handled, state->check_up_needed);
              state->check_up_needed = 0;
          }
          else
              state->check_up_needed -= msg->nb_check_handled;
	  break;

      case INST_OP_NOTHING:
	  break;
  }

  /* The recovery of this service is finished; fields about recovery operation
   * are now useless (because the work was done) */
  state->op_in_progress = INST_OP_NOTHING;
  state->involved_in_op = EXA_NODESET_EMPTY;

  UNLOCK();

  exalog_debug("=== After recovery %s (end) ===", adm_service_name(msg->service_id));
  inst_dump();

  admwrk_ack(thr_nb, EXA_SUCCESS);
}

static void
inst_set_leaderable(int thr_nb, adm_service_id_t id)
{
  admwrk_exec_command(thr_nb, adm_services[id], RPC_INST_SET_LEADERABLE, NULL, 0);
}

static void
inst_local_set_leaderable(int thr_nb, void *dummy)
{
  exalog_debug("I'm now leaderable");

  /* Allow this node to become leader. */
  evmgr_mship_set_local_started(true);

  admwrk_ack(thr_nb, EXA_SUCCESS);
}

/* --- adm_hierarchy_run_stop ---------------------------------------- */
/** \brief Stop the hierarchy: take all services from the specified
 * hierarchy and put them in the state "stopped".
 *
 * \param[in] thr_nb      Thread number
 *
 * \return 0 in case of success
 */
void adm_hierarchy_run_stop(int thr_nb, const stop_data_t *stop_data, cl_error_desc_t *err_desc)
{
  const struct adm_service *s;
  int error_val = EXA_SUCCESS;

  exalog_info("run stop");

  adm_service_for_each_reverse(s)
  {
    exa_nodeset_t committed_up;
    exalog_info("stop service %s", adm_service_name(s->id));
    error_val = s->suspend ? s->suspend(thr_nb) : EXA_SUCCESS;
    if (error_val != EXA_SUCCESS)
      break;
    error_val = s->stop ? s->stop(thr_nb, stop_data) : EXA_SUCCESS;
    if (s->resume)
      s->resume(thr_nb);
    if (error_val)
      break;

    /* FIXME the following is a hugly hack to permit the services to commit
     * their new membership. The problem is that we would need to have a
     * structure which would store new states, and pass it to after recovery
     * in order to get it synchronized on all ondes and committed.
     * Until we have a correct framework to do that, this stuff does the trick
     * even if it is really hugly */
    exa_nodeset_copy(&committed_up, &state_of(s->id)->committed_up);
    exa_nodeset_substract(&committed_up, &stop_data->nodes_to_stop);
    __inst_sync_after_recovery(thr_nb, s, &committed_up, 0);
  }

#ifdef USE_YAOURT
  yaourt_event_wait(EXAMSG_ADMIND_RECOVERY_ID, "adm_hierarchy_run_stop end");
#endif

  set_error(err_desc, error_val, NULL);
}

/* --- adm_hierarchy_run_shutdown ------------------------------------ */

/** \brief Shutdown the hierarchy: take all services from the specified
 * hierarchy and put them from the state "stopped" to the state "down".
 *
 * \param[in] thr_nb      Thread number
 *
 * \return 0 in case of success
 */
static void adm_hierarchy_run_shutdown(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct adm_service *s;
  int error_val = EXA_SUCCESS;

  exalog_info("run shutdown");

  /* Find the last service in the hierarchy */
  adm_service_for_each_reverse(s)
  {
    exalog_info("shutdown service %s", adm_service_name(s->id));
    error_val = s->shutdown ? s->shutdown(thr_nb) : EXA_SUCCESS;

    /* Continue shutdown anyway that's the best we can possibly do */
    if (error_val != EXA_SUCCESS)
      exalog_error("shutdown failed: %s", exa_error_msg(error_val));
  }

#ifdef USE_YAOURT
  yaourt_event_wait(EXAMSG_ADMIND_RECOVERY_ID, "adm_hierarchy_run_shutdown end");
#endif

  set_error(err_desc, error_val, NULL);
}

/* --- adm_hierarchy_run_init ---------------------------------------- */
/** \brief Init the hierarchy: take all services from the specified
 * hierarchy and put them from the state down to the state "stopped".
 *
 * \param[in] thr_nb      Thread number
 *
 * \return 0 in case of success
 */
static void adm_hierarchy_run_init(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct adm_service *s;
  int error_val = EXA_SUCCESS;

  exalog_info("run init");

  cmd_check_license_status();

  adm_service_for_each(s)
  {
    exalog_info("init service %s", adm_service_name(s->id));
    error_val = s->init ? s->init(thr_nb) : EXA_SUCCESS;
    if (error_val != EXA_SUCCESS)
    {
      exalog_error("init failed: %s", exa_error_msg(error_val));
      adm_hierarchy_run_shutdown(thr_nb, data, err_desc);
      set_error(err_desc, error_val, NULL);
      return;
    }
  }

#ifdef USE_YAOURT
  yaourt_event_wait(EXAMSG_ADMIND_RECOVERY_ID, "adm_hierarchy_run_init end");
#endif

  set_success(err_desc);
}

static void log_recovery(adm_service_id_t sid)
{
    if (state_of(sid)->op_in_progress == INST_OP_NOTHING)
        return;

    if (exa_nodeset_is_empty(&state_of(sid)->involved_in_op))
        exalog_info("run recovery %s (resources) for service %s",
                    inst_op2str(state_of(sid)->op_in_progress),
                    adm_service_name(sid));
    else
    {
        char nodes_str[EXA_MAXSIZE_HOSTSLIST + 1];

        adm_nodeset_to_names(&state_of(sid)->involved_in_op, nodes_str, sizeof(nodes_str));
        exalog_info("run recovery %s (instances and resources) for service %s (nodes: %s)",
                    inst_op2str(state_of(sid)->op_in_progress),
                    adm_service_name(sid), nodes_str);
    }
}

static void __recovery_up_down_resource(int thr_nb,
                                        cl_error_desc_t *err_desc)
{
    const struct adm_service *service, *prev_service;
#define do_step_or_return(s, step, err_desc) \
    do { \
	/* FIXME here we abort the recovery if the state of resources changed
	 * since we started the recovery. In fact this higthlight the fact that
	 * the recovery loop is a top level operation and should not be done
	 * here: after each step, it is needed to tell what is the prioritary
	 * recovery operation to perform. Sometime it is a up, sometime a down
	 * depending on how the situation changes. As all data needed to compute
	 * recovery are owned by evmgr, recomputing the recovery here on fly is
	 * opened to races and breaks ownership, that's why we return. */ \
	if (state_of((s)->id)->op_in_progress == INST_OP_NOTHING \
            && state_of((s)->id)->resources_changed_down) \
	{ \
	    /* FIXME The error set is -ADMIND_ERR_NODE_DOWN because the recovery
	     * must return success of nodedown.... */ \
	    set_error(err_desc, -ADMIND_ERR_NODE_DOWN, \
		    "Resources of service '%s' changed state, restarting recovery.", \
		    adm_service_name((s)->id)); \
	    return; \
	} \
        \
        if (state_of((s)->id)->op_in_progress != INST_OP_NOTHING) \
        { \
	  exalog_debug(#step " service %s", adm_service_name((s)->id)); \
	  (err_desc)->code = (s)->step ? (s)->step(thr_nb) : EXA_SUCCESS; \
	  if ((err_desc)->code != EXA_SUCCESS) \
	  { \
	      set_error((err_desc), (err_desc)->code, \
		        "The " #step " of service %s failed: %s", \
		        adm_service_name((s)->id), \
		        exa_error_msg((err_desc)->code)); \
	      return; \
	  } \
        } \
    } while (0);

    /* Suspend and recover of first service is done outside of the loop because
     * there is no previous service to resume */
    do_step_or_return(adm_services[ADM_SERVICE_FIRST], suspend, err_desc);

    log_recovery(ADM_SERVICE_FIRST);

    do_step_or_return(adm_services[ADM_SERVICE_FIRST], recover, err_desc);

    for (prev_service = adm_services[ADM_SERVICE_FIRST],
	      service = adm_services[ADM_SERVICE_FIRST + 1];
	 service;
	 service = (service->id < ADM_SERVICE_LAST) ?
	           adm_services[service->id + 1] : NULL)
    {
        /* suspend service N */
        do_step_or_return(service, suspend, err_desc);

	/* Careful:
	 * It is mandatory for the after recovery to be done AFTER resume.
	 * inst_sync_after_recovery mark the recovery as completely finished
	 * thus doing it before resume could lead to a deadlock if the resume
	 * fails:
	 * - instances are maked recovered,
	 * - the suspend of next services fails; the loop returns
	 * - a new recovery begins, but do not make a recovery of the service
	 *   which lack a resume => deadlock */
        /* Resume service N - 1 */
        do_step_or_return(prev_service, resume, err_desc);

	if (state_of((prev_service)->id)->op_in_progress != INST_OP_NOTHING)
	    /* sync membership change for service N - 1 */
	    inst_sync_after_recovery(thr_nb, prev_service, 0);

	log_recovery(service->id);

        /* Recover service N */
        do_step_or_return(service, recover, err_desc);

        prev_service = service;
    }

    /* Resume last service */
    do_step_or_return(prev_service, resume, err_desc);

    if (state_of((prev_service)->id)->op_in_progress != INST_OP_NOTHING)
	/* sync membership change for last service */
	inst_sync_after_recovery(thr_nb, prev_service, 0);

    set_success(err_desc);
}

/* --- adm_hierarchy_run_recovery ------------------------------------ */
/** \brief Do a recovery of the hierarchy: take all services from the
 * specified hierarchy and do a "suspend, recovery and resume".
 *
 * \param[in]     thr_nb      Thread number
 * \param[in]     data        *(inst_op_t *)data is the recovery to perform
 * \param[in:out] err_desc    Error descriptor
 */

static void
adm_hierarchy_run_recovery(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct adm_service *s;
  inst_op_t op = *(inst_op_t *)data;

  exalog_debug("hierarchy_run_recovery %s: start", inst_op2str(op));
#ifdef USE_YAOURT
  yaourt_event_wait(EXAMSG_ADMIND_RECOVERY_ID,
		    "adm_hierarchy_run_recovery begin %s", inst_op2str(op));
#endif

  /* Synchronize instance states on nodes */
  inst_sync_before_recovery(thr_nb, err_desc);
  if (err_desc->code)
      return;

  switch (op)
  {
    case INST_OP_CHECK_DOWN:
      exalog_info("run recovery %s", inst_op2str(op));
      adm_service_for_each(s)
      {
	uint32_t nb_check_handled = state_of(s->id)->check_down_needed;

	if (s->check_down && nb_check_handled > 0)
	{
	  exalog_info("check service %s", adm_service_name(s->id));
	  set_error(err_desc, s->check_down(thr_nb), NULL);
	}

	/* commit membership change for this instances */
	inst_sync_after_recovery(thr_nb, s, nb_check_handled);
      }
      break;

    case INST_OP_CHECK_UP:
      adm_service_for_each(s)
      {
	uint32_t nb_check_handled = state_of(s->id)->check_up_needed;

	if (s->check_up && nb_check_handled > 0)
	{
	  set_error(err_desc, s->check_up(thr_nb), NULL);
	  if (err_desc->code != EXA_SUCCESS)
	    break; /* careful adm_service_for_each is a loop, break here
		    * just terminate the loop
		    * FIXME Why is there no inst_sync_after_recovery then ? */
	}

	/* commit membership change for this instances */
	inst_sync_after_recovery(thr_nb, s, nb_check_handled);
      }
      break;

    case INST_OP_UP:
    case INST_OP_DOWN:
      /* If this is the first recovery up (start), alls nodes have the same state.
	 We have to set them leaderable to distinguish them from a node that could
	 arrive later during the start. This operation can be done on any node
	 until the first service is fully recovered up on at least one node. In
	 this case, any new comer will not be in the same state than the nodes that
	 already finished their recovery of some service.  */
      if (op == INST_OP_UP && exa_nodeset_is_empty(&state_of(ADM_SERVICE_FIRST)->committed_up))
	inst_set_leaderable(thr_nb, ADM_SERVICE_FIRST);

      __recovery_up_down_resource(thr_nb, err_desc);

      /* Allow completely recovered up nodes to become leader. */
      if (op == INST_OP_UP && err_desc->code == EXA_SUCCESS)
	inst_set_leaderable(thr_nb, ADM_SERVICE_LAST);

      break;

    case INST_OP_NOTHING:
    default:
      EXA_ASSERT_VERBOSE(false, "Unknown or illegal operation %s (%d)",
	                 inst_op2str(op), op);
      break;
  }

#ifdef USE_YAOURT
  yaourt_event_wait(EXAMSG_ADMIND_RECOVERY_ID,
		    "adm_hierarchy_run_recovery end %s", inst_op2str(op));
#endif

  EXA_ASSERT_VERBOSE(err_desc->code == EXA_SUCCESS
                     || err_desc->code == -ADMIND_ERR_NODE_DOWN,
		     "Recovery failed: %s (%d)", err_desc->msg, err_desc->code);

  exalog_debug("hierarchy_run_recovery %s: end", inst_op2str(op));

  if (op != INST_OP_CHECK_UP)
    exalog_info("recovery %s %s", inst_op2str(op),
	        err_desc->code ? "interrupted" : "successful");
}



__export(EXA_ADM_CLINIT) __no_param;

const AdmCommand exa_clinit = {
  .code            = EXA_ADM_CLINIT,
  .msg             = "clinit",
  .accepted_status = ADMIND_STOPPED,
  .match_cl_uuid   = true,
  .cluster_command = adm_hierarchy_run_init,
  .allowed_in_recovery = true,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};

const AdmCommand run_shutdown = {
  .code            = EXA_ADM_RUN_SHUTDOWN,
  .msg             = "shutdown",
  .cluster_command = adm_hierarchy_run_shutdown,
  .allowed_in_recovery = true,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};

const AdmCommand run_recovery = {
  .code            = EXA_ADM_RUN_RECOVERY,
  .msg             = "recovery",
  .cluster_command = adm_hierarchy_run_recovery,
  .allowed_in_recovery = true,
  .local_commands  = {
    { RPC_INST_SYNC_BEFORE_RECOVERY, inst_local_sync_before_recovery },
    { RPC_INST_SYNC_AFTER_RECOVERY,  inst_local_sync_after_recovery  },
    { RPC_INST_SET_LEADERABLE,       inst_local_set_leaderable       },
    { RPC_COMMAND_NULL, NULL }
  }
};

