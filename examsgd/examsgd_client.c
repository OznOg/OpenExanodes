/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "examsgd_client.h"
#include "examsgd.h"

#include "common/include/exa_constants.h"
#include "os/include/strlcpy.h"

#include "log/include/log.h"


/* --- examsgFencingOperation --------------------------------------- */

/** \brief Asks examsgd to fence or unfence a list of node
 *
 * \param[in] mh	 Examsg handle used for communication.
 * \param[in] node_set	 list of node we want to fence/unfence.
 * \param[in] order	 tyep of order we perform : FENCE or UNFENCE.
 * \param[in] evloop	 Event loop to call while waiting for operation.
 * \param[in] evdata	 private data for \a evloop.
 *
 * \return 0 on success.
 */
static int
examsgFencingOperation(ExamsgHandle mh, const exa_nodeset_t *node_set,
                       enum fencing_order order)
{
  examsgd_fencing_req_msg_t msg;
  int s;

  EXA_ASSERT(node_set);

  msg.any.type = EXAMSG_FENCE;
  exa_nodeset_copy(&msg.request.node_set, node_set);
  msg.request.order = order;

  s = examsgSendNoBlock(mh, EXAMSG_CMSGD_ID, EXAMSG_LOCALHOST, &msg,
                        sizeof(msg));
  if (s != sizeof(msg))
    return s;

  return 0;
}


/** \brief Asks examsgd to fence a list of node
 *
 * \param[in] mh         Examsg handle used for communication.
 * \param[in] node_set   list of node we want to fence.
 * \param[in] evloop     Event loop to call while waiting for operation.
 * \param[in] evdata     private data for \a evloop.
 *
 * \return 0 on success.
 */
int
examsgFence(ExamsgHandle mh, const exa_nodeset_t *node_set)
{
  return examsgFencingOperation(mh, node_set, FENCE);
}

/** \brief Asks examsgd to unfence a list of node
 *
 * \param[in] mh         Examsg handle used for communication.
 * \param[in] node_set   list of node we want to unfence.
 * \param[in] evloop     Event loop to call while waiting for operation.
 * \param[in] evdata     private data for \a evloop.
 *
 * \return 0 on success.
 */
int
examsgUnfence(ExamsgHandle mh, const exa_nodeset_t *node_set)
{
  return examsgFencingOperation(mh, node_set, UNFENCE);
}

/* --- examsgAddNode ------------------------------------------------- */

/** \brief Add a node to exa_msg so that exa_msgd can receive msg from
 * this node
 *
 * \param[in] mh	Examsg handle used for communication.
 * \param[in] node_id   Node id
 * \param[in] host	Host name.
 * \param[in] evloop	Event loop to call while waiting for operation.
 * \param[in] evdata	private data for \a evloop.
 *
 * \return 0 on success.
 */

int
examsgAddNode(ExamsgHandle mh, exa_nodeid_t node_id, const char *host)
{
  examsg_node_info_msg_t msg;
  int s, ackerr;

  EXA_ASSERT(EXA_NODEID_VALID(node_id) && host);

  msg.any.type = EXAMSG_ADDNODE;
  msg.node_id = node_id;
  strlcpy(msg.node_name, host, sizeof(msg.node_name));

  s = examsgSendWithAck(mh, EXAMSG_CMSGD_ID, EXAMSG_LOCALHOST,
			(Examsg *)&msg, sizeof(msg), &ackerr);
  if (s != sizeof(msg))
    return s;

  exalog_debug("added node %u:'%s' to exa_msgd", node_id, host);

  return ackerr;
}


/**
 * Remove a node from exa_msgd.
 *
 * \param[in] mh       Examsg handle
 * \param[in] node_id  Id of node to remove
 * \param[in] evloop   Event loop
 * \param     evdata   private data for #evloop
 *
 * \return 0 on success, negative error code otherwise
 */
int
examsgDelNode(ExamsgHandle mh, exa_nodeid_t node_id)
{
  examsg_node_info_msg_t msg;
  int s, ackerr;

  EXA_ASSERT(EXA_NODEID_VALID(node_id));

  msg.any.type = EXAMSG_DELNODE;
  msg.node_id = node_id;

  s = examsgSendWithAck(mh, EXAMSG_CMSGD_ID, EXAMSG_LOCALHOST,
			(Examsg *)&msg, sizeof(msg), &ackerr);
  if (s != sizeof(msg))
    return s;

  exalog_debug("deleted node %u from exa_msgd", node_id);

  return ackerr;
}

int examsgdExit(ExamsgHandle mh)
{
    ExamsgAny msg;
    int s, ackerr;

    msg.type = EXAMSG_EXIT;

    s = examsgSendWithAck(mh, EXAMSG_CMSGD_ID, EXAMSG_LOCALHOST,
	                  (Examsg *)&msg, sizeof(msg), &ackerr);

    return s != sizeof(msg) ? s : ackerr;
}

