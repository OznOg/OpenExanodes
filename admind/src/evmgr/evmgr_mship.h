/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EVMGR_MSHIP_H__
#define __EVMGR_MSHIP_H__

#include "csupd/include/exa_csupd.h"
#include "token_manager/tm_client/include/tm_client.h"

/** Membership info from a node */
typedef struct evmgr_mship_info
  {
    exa_nodeset_t mship;        /**< Membership */
    sup_gen_t gen;              /**< Membership generation */
    bool already_started; /**< Node is started ? */
    bool is_leader;       /**< Node is current leader ? */
    int pad;
  } evmgr_mship_info_t;

/** Message READY sent by leader wannabe to other nodes */
EXAMSG_DCLMSG(evmgr_ready_msg_t, struct {
  evmgr_mship_info_t info;
});

/** Message YES sent to leader wannabe in reply to READY */
EXAMSG_DCLMSG(evmgr_yes_msg_t, struct {
  sup_gen_t gen;  /**< Membership generation */
});

/** Membership commit phase */
typedef enum evmgr_mship_phase
  {
    EVMGR_MSHIP_PHASE_NONE,
    EVMGR_MSHIP_PHASE_PREPARE,
    EVMGR_MSHIP_PHASE_COMMIT
  } evmgr_mship_phase_t;

/** Message PREPARE sent by actual leader to other nodes */
EXAMSG_DCLMSG(evmgr_prepare_msg_t, struct {
  evmgr_mship_info_t info;  /**< Membership & leader info */
  exa_nodeid_t leader_id;   /**< Id of leader */
});

/** Message COMMIT sent by leader to other nodes */
EXAMSG_DCLMSG(evmgr_commit_msg_t, struct {
  sup_gen_t gen;  /**< Generation of membership to commit */
});

/** Message ABORT sent to abort current commit */
EXAMSG_DCLMSG(evmgr_abort_msg_t, struct {
  sup_gen_t gen;  /**< Membership generation */
});

/** Message ACK sent to leader in reply to PREPARE/COMMIT */
EXAMSG_DCLMSG(evmgr_ack_msg_t, struct {
  sup_gen_t gen;              /**< Membership generation acked */
  evmgr_mship_phase_t phase;  /**< Phase acked */
});

void need_recovery_set(bool b);

bool need_recovery_get(void);

int evmgr_mship_init(void);
void evmgr_mship_shutdown(void);

const exa_nodeset_t *evmgr_mship(void);

bool evmgr_mship_may_have_quorum(const exa_nodeset_t *mship);
bool evmgr_has_quorum(void);

void evmgr_mship_set_local_started(bool is_started);

void evmgr_mship_received_local(ExamsgHandle mh, const SupEventMshipChange *mc);

void evmgr_mship_received_ready(ExamsgHandle mh, const evmgr_ready_msg_t *ready,
				exa_nodeid_t sender_id);

void evmgr_mship_received_yes(ExamsgHandle mh, const evmgr_yes_msg_t *yes,
			      exa_nodeid_t sender_id);

void evmgr_mship_received_prepare(ExamsgHandle mh,
				  const evmgr_prepare_msg_t *prep,
				  exa_nodeid_t sender_id);

void evmgr_mship_received_ack(ExamsgHandle mh, const evmgr_ack_msg_t *ack,
			      exa_nodeid_t sender_id);

bool evmgr_mship_received_commit(ExamsgHandle mh,
				       const evmgr_commit_msg_t *commit,
				       exa_nodeid_t sender_id);

void evmgr_mship_received_abort(ExamsgHandle mh,
				const evmgr_abort_msg_t *abort_msg,
				exa_nodeid_t sender_id);

/**
 * Tells if a valid token manager is set.
 *
 * return true a valid token manager is set, false else.
 */
bool evmgr_mship_token_manager_is_set(void);

/**
 * Tells if node is connected to token manager.
 * Obviously, this function is meaningless when no token manager is set.
 *
 * return true if TM is connected, false otherwise
 */
bool evmgr_mship_token_manager_is_connected(void);

/**
 * Try to connect to the token manager.
 * This may perform a network connection thus may allocate memory in case the
 * TM is set.
 *
 *  @return 0 if successful, a negative error code otherwise
 */
int evmgr_mship_tm_connect(void);

/**
 * Tells if the node is trying to acquire the token.
 *
 * "Trying to acquire the token" means that the clique the node is in doesn't
 * have a quorum and the node doesn't currently hold the token, and thus the
 * node is in a situation where it is supposed to try to acquire the token.
 *
 * @return true if trying, false otherwise
 */
bool evmgr_mship_trying_to_acquire_token(void);

#endif /* __EVMGR_MSHIP_H__ */
