/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_WORK_THREAD
#define H_WORK_THREAD

#include "common/include/daemon_request_queue.h"

typedef enum
{
  LUM_REQUEST_QUEUE_INFO,
  LUM_REQUEST_QUEUE_COMMAND,
} lum_request_queue_t;

#define LUM_REQUEST_QUEUE_LAST LUM_REQUEST_QUEUE_COMMAND

extern struct daemon_request_queue *lum_request_info_queue;
extern struct daemon_request_queue *lum_request_command_queue;

void lum_workthreads_start(void);
void lum_workthreads_stop(void);

#endif /* H_WORK_THREAD */
