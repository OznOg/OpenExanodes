/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <string.h>

#include "common/include/exa_error.h"
#include "log/include/log.h"
#include "common/include/daemon_api_client.h"
#include "common/include/exa_assert.h"


/* --- admwrk_daemon_query ---------------------------------------- */

/**
 * Send a message to a daemon and wait the answer
 *
 * @param[in]  mh                     Examsg handle
 * @param[in]  to                     ID of the daemon
 * @param[in]  request_type           Type of the request (ExamsgType)
 * @param[in]  request, request_size  Buffer to send to the daemon
 * @param[out] answer, answer_size    Buffer for the answer
 *
 * @return     0 or negative error code
 */
int  admwrk_daemon_query(ExamsgHandle mh, ExamsgID to, ExamsgType request_type,
                         const void *request, size_t request_size,
                         void *answer,  size_t answer_size)
{
  int ret;
  Examsg msg;
  Examsg msg_answer;
  ExamsgMID mid;
  size_t examsg_request_size = sizeof(ExamsgAny) + request_size;
  size_t examsg_answer_size  = sizeof(ExamsgAny) + answer_size;

  /* Build the message to send */
  msg.any.type = request_type;
  EXA_ASSERT(sizeof(Examsg) >= examsg_request_size);
  memcpy(msg.payload, request, request_size);

  /* Send the message */
  ret = examsgSend(mh, to, EXAMSG_LOCALHOST, &msg, examsg_request_size);
  if (ret != examsg_request_size)
      return ret;

  /* Receive a message */
  do {
      ret = examsgWait(mh);
      EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "examsgWait() returned %d", ret);

      ret = examsgRecv(mh, &mid, &msg_answer, examsg_answer_size);
  } while (ret == 0);

  /* if the daemon ack our request, returns */
  EXA_ASSERT_VERBOSE(ret == sizeof(ExamsgAny) + answer_size,
      "the received message has not the right size (%d != %" PRIzu "+%" PRIzu ")",
      ret, sizeof(ExamsgAny), answer_size);
  EXA_ASSERT(mid.netid.node == EXA_NODEID_LOCALHOST);
  EXA_ASSERT(msg_answer.any.type == EXAMSG_DAEMON_REPLY);
  memcpy(answer, msg_answer.payload, answer_size);
  return 0;

}


/* --- admwrk_daemon_query_nointr -------------------------------- */

/**
 * Send a message to a daemon and wait the answer
 *
 * @param[in]  mh                     Examsg handle
 * @param[in]  to                     ID of the daemon
 * @param[in]  request_type           Type of the request (ExamsgType)
 * @param[in]  request, request_size  Buffer to send to the daemon
 * @param[out] answer, answer_size    Buffer for the answer
 *
 * @return     0 or negative error code
 */
int admwrk_daemon_query_nointr(ExamsgHandle mh, ExamsgID to,
			       ExamsgType request_type,
			       const void *request, size_t request_size,
			       void *answer,  size_t answer_size)
{
  return admwrk_daemon_query(mh, to, request_type,
                             request, request_size,
                             answer, answer_size);
}

