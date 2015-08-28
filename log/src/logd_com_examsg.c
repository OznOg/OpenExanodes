/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_assert.h"
#include "examsg/include/examsg.h"
#include "examsg/src/mailbox.h"
#include "logd_com.h"
#include <errno.h>

/** Maximum number of log messages in the mailbox */
#define EXALOG_MAX_MESSAGES  128

static ExamsgHandle mh;  /**< Examsg handle */

int logd_com_init(void)
{
  int s;

  /* Initialize examsg framework */
  mh = examsgInit(EXAMSG_LOGD_ID);
  if (!mh)
    {
      os_syslog(OS_SYSLOG_ERROR, "cannot initialize examsg");
      return -errno;
    }

  /* Create local mailbox */
  s = examsgAddMbox(mh, EXAMSG_LOGD_ID, EXALOG_MAX_MESSAGES, sizeof(exalog_msg_t));
  if (s)
    {
      os_syslog(OS_SYSLOG_ERROR, "Cannot log mailbox mailbox, error = %d", s);
      return s;
    }

  return 0;
}

void logd_com_exit(void)
{
    examsgDelMbox(mh, EXAMSG_LOGD_ID);
    examsgExit(mh);
}

int logd_com_recv(exalog_data_t *out)
{
  ExamsgMID mid;
  int s;

  do {
      s = examsgWait(mh);
      EXA_ASSERT(s == 0);
  } while ((s = examsgMboxRecv(EXAMSG_LOGD_ID, &mid, sizeof(mid),
		               out, sizeof(*out))) == 0);

  return s > 0 ? 0 : s;
}

int logd_com_send(const void *buf, size_t size)
{
  ExamsgMID mid;
  int s = examsgMboxSend(&mid, EXAMSG_LOGD_ID, EXAMSG_LOGD_ID, (const char *)buf, size);
  return s > 0 ? 0 : s;
}
