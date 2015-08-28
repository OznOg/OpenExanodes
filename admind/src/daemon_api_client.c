/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "os/include/os_error.h"
#include <string.h>

#include "admind/src/instance.h"
#include "common/include/exa_error.h"
#include "examsg/include/examsg.h"
#include "log/include/log.h"
#include "common/include/daemon_api_client.h"
#include "common/include/daemon_request_queue.h"

#ifdef USE_YAOURT
#include <yaourt/yaourt.h>
#endif

/* --- _there_is_node_down ------------------------------------------- */
/**
 * Check if there is a node down, thus if we should interrupt
 *
 * @return true or false
 */
static bool
_there_is_node_down(void)
{
    return inst_check_blockable_event();
}

/* --- receive_reply_timeout --------------------------------------- */
/**
 * Receive reply from the daemon
 *
 * @param[in]  mh            Examsg handle
 * @param[out] answer        Buffer for the answer
 * @param[in]  answer_size   output buffer answer size
 * @param[in]  timeout       timeout for receiving
 *
 * @return     0 if success, -ADMIND_ERR_NODE_DOWN on interrupt
 *             or -ETIME on timeout.
 */
static int
receive_reply_timeout(ExamsgHandle mh, void *answer, size_t answer_size,
	             struct timeval *timeout)
{
  ExamsgMID mid;
  Examsg msg_answer;
  int ret;

  do {
      ret = examsgWaitTimeout(mh, timeout);
      if (ret)
	  return ret;
  }  while ((ret = examsgRecv(mh, &mid, &msg_answer, sizeof(Examsg))) == 0);

  EXA_ASSERT(mid.netid.node == EXA_NODEID_LOCALHOST);
  EXA_ASSERT_VERBOSE(ret > 0, "Received failed with error %d", ret);

  /* Must receive either request reply or ack of the interrupt */
  switch (msg_answer.any.type)
  {
      case EXAMSG_DAEMON_INTERRUPT_ACK:
#ifdef USE_YAOURT
	  yaourt_event_wait(examsgOwner(mh), "admwrk_daemon_query receive ack interrupt");
#endif
	  return -ADMIND_ERR_NODE_DOWN;
	  break;

      case EXAMSG_DAEMON_REPLY:
	  EXA_ASSERT_VERBOSE(ret == sizeof(ExamsgAny) + answer_size,
		  "the received message has not the right size (%d != %" PRIzu "+%" PRIzu ")",
		  ret, sizeof(ExamsgAny), answer_size);

#ifdef USE_YAOURT
	  yaourt_event_wait(examsgOwner(mh), "admwrk_daemon_query receive reply");
#endif
	  memcpy(answer, msg_answer.payload, answer_size);
	  break;

      default:
	  EXA_ASSERT_VERBOSE(false, "Bad message type: %d", msg_answer.any.type);
  }

  return 0;
}

/* --- _admwrk_daemon_query --------------------------------------- */

/**
 * Send a message to a daemon and wait for the answer
 *
 * @param[in]  mh                     Examsg handle
 * @param[in]  to                     ID of the daemon
 * @param[in]  request_type           Type of the request (ExamsgType)
 * @param[in]  request, request_size  Buffer to send to the daemon
 * @param[out] answer, answer_size    Buffer for the answer
 * @param[in]  might_block            tells if the function can interrupt itself
 *                                    on node failures
 *
 * @return     0 or negative error code
 */
static int
_admwrk_daemon_query(ExamsgHandle mh, ExamsgID to, ExamsgType request_type,
                     const void *request, size_t request_size,
                     void *answer,  size_t answer_size, bool might_block)
{
  ExamsgAny header;
  int ret;

#ifdef USE_YAOURT
  yaourt_event_wait(examsgOwner(mh), "admwrk_daemon_query begin");
#endif

  /* Send the message */
  header.type = request_type;
  ret = examsgSendWithHeader(mh, to, EXAMSG_LOCALHOST,
		             &header, request, request_size);

  /* If the daemon is not there anymore (ret == -ENXIO) the instance can be considered
   * as down thus we can return -ADMIND_ERR_NODE_DOWN because this is a kind of failure
   * detection, after all. */
  /* FIXME this lib would REALLY deserve to have its own return code INTERRUPTED that
   * would not mix up with standard nodedown detection. */
  if (ret == -ENXIO)
      return -ADMIND_ERR_NODE_DOWN;

  if (ret != request_size)
      return ret;

#ifdef USE_YAOURT
  yaourt_event_wait(examsgOwner(mh), "admwrk_daemon_query after send");
#endif

  do {
      struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
      /* Receive a message */
      ret = receive_reply_timeout(mh, answer, answer_size,
	                           might_block ? &timeout : NULL);
      /* Everything was ok, this is finished */
      if (ret == 0)
	  return 0;

      EXA_ASSERT_VERBOSE(ret == -ETIME, "Received failed with error %d", ret);
  } while (!_there_is_node_down());

  EXA_ASSERT(might_block);

  /* If we are here, this means that we received an interrupt */

#ifdef USE_YAOURT
  yaourt_event_wait(examsgOwner(mh), "admwrk_daemon_query receive interrupt");
#endif

  header.type = EXAMSG_DAEMON_INTERRUPT;
  ret = examsgSendWithHeader(mh, to, EXAMSG_LOCALHOST, &header, NULL, 0);

  /* Other daemons (eg exa_fsd) could be killed before admind so their mailbox
   * do not exist anymore. In such a case we assert here on the failing node
   * rather than returning an error to the leader and make it assert too. */
  EXA_ASSERT_VERBOSE(ret != -ENXIO, "Failed to send a message to %s. Maybe it crashed.",
	  examsgIdToName(to));

  if (ret < 0)
      return ret;

  /* There must be an incoming answer, so do not use timeout */
  return receive_reply_timeout(mh, answer, answer_size, NULL);
}

/* --- admwrk_daemon_query ---------------------------------------- */

/**
 * Send a message to a daemon and wait the answer. Used for request
 * that might block.
 *
 * @param[in]  mh                     Examsg handle
 * @param[in]  to                     ID of the daemon
 * @param[in]  request_type           Type of the request (ExamsgType)
 * @param[in]  request, request_size  Buffer to send to the daemon
 * @param[out] answer, answer_size    Buffer for the answer
 *
 * @return     0 or negative error code
 */
int
admwrk_daemon_query(ExamsgHandle mh, ExamsgID to, ExamsgType request_type,
                         const void *request, size_t request_size,
                         void *answer,  size_t answer_size)
{
  return _admwrk_daemon_query(mh, to, request_type,
                              request, request_size,
                              answer, answer_size, true);
}

/* --- admwrk_daemon_query_nointr -------------------------------- */

/**
 * Send a message to a daemon and wait the answer. Used for request
 * that should not be interrupted by a node down or check
 * down. Because of that, it can only be used to execute code that
 * will not block in any case for a reason that would need a recovery
 * in order to allow the code to continue its execution. For example,
 * this function cannot be used to call code that does I/O, either on
 * distant or local devices, or memory allocations that could lead to
 * page cache flush (GFP_ATOMIC allocations are allowed, but not
 * GFP_KERNEL).
 *
 * @param[in]  mh                     Examsg handle
 * @param[in]  to                     ID of the daemon
 * @param[in]  request_type           Type of the request (ExamsgType)
 * @param[in]  request, request_size  Buffer to send to the daemon
 * @param[out] answer, answer_size    Buffer for the answer
 *
 * @return     0 or negative error code
 */
int
admwrk_daemon_query_nointr(ExamsgHandle mh, ExamsgID to,
			   ExamsgType request_type,
			   const void *request, size_t request_size,
			   void *answer,  size_t answer_size)
{
  return _admwrk_daemon_query(mh, to, request_type,
                              request, request_size,
                              answer, answer_size, false);
}

