/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "sup_debug.h"
#include "sup_ping.h"

#include "csupd/include/exa_csupd.h"

#include "examsg/include/examsg.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h" /* for exa_error_msg */

#include "os/include/os_error.h"

/** Maximum number of ping messages in the mailbox. Each node sends one
    ping message per ping period, so we should receive only as many pings
    as there are nodes, but we need some slack since Csupd may not be able
    to read its messages if the node is frozen, hence the factor two */
#define SUP_MAX_PING_MSG  (EXA_MAX_NODES_NUMBER * 2)

/** Ping message */
EXAMSG_DCLMSG(sup_ping_msg_t, sup_ping_t ping);

/** Examsg mailbox handle */
static ExamsgHandle sup_mh;

/**
 * Deliver to Evmgr the membership calculated by Csupd.
 *
 * \param[in] gen    Generation number
 * \param[in] mship  Membership to deliver
 *
 * \return 0 if successfull, negative error code otherwise
 */
int
sup_deliver(sup_gen_t gen, const exa_nodeset_t *mship)
{
  int ret;
  SupEventMshipChange msg;

  msg.any.type = EXAMSG_SUP_MSHIP_CHANGE;
  msg.gen = gen;
  exa_nodeset_copy(&msg.mship, mship);

  ret = examsgSendNoBlock(sup_mh, EXAMSG_ADMIND_EVMGR_ID, EXAMSG_LOCALHOST, &msg,
	                  sizeof(SupEventMshipChange));
  EXA_ASSERT_VERBOSE(ret == sizeof(SupEventMshipChange),
                     "Unable to deliver membership to the evmgr (%d)", ret);

  return 0;
}

/**
 * Send a ping to all instances of Csupd using examsg.
 *
 * \param[in] cluster  Cluster
 * \param[in] view     View to send
 */
void
sup_send_ping(const sup_cluster_t *cluster, const sup_view_t *view)
{
  sup_ping_msg_t ping_msg;
  int err;

  ping_msg.any.type = EXAMSG_SUP_PING;

  ping_msg.ping.sender = cluster->self->id;
  ping_msg.ping.incarnation = cluster->self->incarnation;

  sup_view_copy(&ping_msg.ping.view, view);

  EXA_ASSERT(sup_check_ping(&ping_msg.ping, 'S'));

  err = examsgSendNoBlock(sup_mh, EXAMSG_CMSGD_ID, EXAMSG_LOCALHOST,
                          &ping_msg, sizeof(ping_msg));
  if (err != sizeof(ping_msg))
    __debug("cannot send ping : %s (%d)", exa_error_msg(err), err);
}

/**
 * Receive incoming pings through Examsg.
 *
 * \param[out] ping  Received ping
 *
 * \return true if a ping was received, false otherwise
 */
bool
sup_recv_ping(sup_ping_t *ping)
{
  /* initialized to 0s, this will trigger a ping at the very beginning */
  static struct timeval timeout = { 0, 0 };
  ExamsgMID mid;
  sup_ping_msg_t msg;
  int err;

  err = examsgWaitTimeout(sup_mh, &timeout);
  if (err < 0 && err != -ETIME)
    {
      __error("encountered an error while waiting for messages : %s (%d)",
	      exa_error_msg(err), err);
      return false;
    }

  if (err == -ETIME)
    {
      do_ping = true;
      timeout.tv_sec = ping_period;
      timeout.tv_usec = 0;
      return false;
    }

  err = examsgRecv(sup_mh, &mid, &msg, sizeof(msg));
  /* XXX shouldn't happen (?) because of examsgWaitTimeout() above,
   * but it *does* happen at clstop */
  if (err == 0)
    return false;

  if (err < 0)
    {
      __error("encountered an error while retrieving message : %s (%d)",
	      exa_error_msg(err), err);
      return false;
    }

  EXA_ASSERT(err == sizeof(msg));
  EXA_ASSERT(msg.any.type == EXAMSG_SUP_PING);

  *ping = msg.ping;

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
  int err;

  sup_mh = examsgInit(EXAMSG_CSUPD_ID);
  if (!sup_mh)
    return false;

  /* Create local mailbox, buffer at most SUP_MAX_PING_MSG ping messages */
  err = examsgAddMbox(sup_mh, EXAMSG_CSUPD_ID, SUP_MAX_PING_MSG,
	              sizeof(sup_ping_msg_t));
  if (err)
    {
      __error("cannot create mailbox : %s (%d)", exa_error_msg(err), err);
      if (sup_mh)
        examsgExit(sup_mh);
      return false;
    }

  return true;
}

/**
 * Free examsg mailbox
 */
void
sup_cleanup_messaging(void)
{
  examsgDelMbox(sup_mh, EXAMSG_CSUPD_ID);
  examsgExit(sup_mh);
  sup_mh = NULL;
}
