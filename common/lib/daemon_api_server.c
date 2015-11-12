/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */



#include <string.h>
#include <errno.h>

#include "log/include/log.h"
#include "examsg/include/examsg.h"

#include "common/include/exa_assert.h"

/* --- admwrk_daemon_reply ------------------------------------------- */

/** \brief Send acknowledgement message.
 *
 * \param[in] mh                    Examsg handle, created with examsgInit().
 * \param[in] id                    Original sender
 * \param[in] answer, answer_size   Buffer to send to the caller component
 *
 * \return 0 on success or a negative error code.
 */
int admwrk_daemon_reply(ExamsgHandle mh, ExamsgID id,
                        const void *answer, size_t answer_size)
{
  int ret;
  ExamsgAny header;

  header.type = EXAMSG_DAEMON_REPLY;

  ret = examsgSendWithHeader(mh, id, EXAMSG_LOCALHOST, &header,
			     answer, answer_size);
  EXA_ASSERT_VERBOSE(ret != -ENXIO, "Failed to reply to exa_admind. Maybe it crashed.");
  if (ret == answer_size)
    return 0;
  return ret;
}

/* --- admwrk_daemon_ackinterrupt ------------------------------------ */

/** \brief Send ack to the interrupt
 *
 * \param[in] mh                    Examsg handle, created with examsgInit().
 * \param[in] id                  Original sender of the interrupt
 *
 * \return 0 on success or a negative error code.
 */
int admwrk_daemon_ackinterrupt(ExamsgHandle mh, ExamsgID id)
{
  int ret;
  ExamsgAny msg_answer;

  msg_answer.type = EXAMSG_DAEMON_INTERRUPT_ACK;

  ret = examsgSend(mh, id, EXAMSG_LOCALHOST, &msg_answer,
		   sizeof(msg_answer));
  EXA_ASSERT_VERBOSE(ret != -ENXIO, "Failed to reply to exa_admind. Maybe it crashed.");
  if (ret == sizeof(msg_answer))
    return 0;
  return ret;
}

