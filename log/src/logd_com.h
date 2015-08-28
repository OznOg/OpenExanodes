/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __LOGD_COM_H
#define __LOGD_COM_H

#include <os/include/os_inttypes.h>
#include <os/include/os_time.h>

#include "log/include/log.h"

/** Change the loglevel of a component */
typedef struct exalog_config {
  ExamsgID component;	 /**< component id */
  exalog_level_t level;  /**< level of logs */
} exalog_config_t;

/** Log message */
typedef struct exalog_msg {
  char file[EXALOG_NAME_MAX];		/**< file name */
  char func[EXALOG_NAME_MAX];		/**< function name */
  uint32_t line;			/**< line number */

  struct timeval rclock;		/**< real "physical" clock */

  exalog_level_t level;			/**< log level */
  ExamsgID cid;
  unsigned lost;

  char msg[EXALOG_MSG_MAX];		/**< text */
} exalog_msg_t;

typedef struct exalog_data {
    enum { LOG_CONFIG, LOG_HOST, LOG_MSG, LOG_QUIT } type;
    union {
	exalog_config_t log_config;
	exalog_msg_t    log_msg;
	char hostname[EXA_MAXSIZE_HOSTNAME + 1];
    } d;
} exalog_data_t;

int logd_com_init(void);
void logd_com_exit(void);
int logd_com_recv(exalog_data_t *out);
int logd_com_send(const void *buf, size_t size);

#endif
