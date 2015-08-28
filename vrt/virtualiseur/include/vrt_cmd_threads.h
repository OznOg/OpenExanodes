/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __VRT_CMD_THREADS_H__
#define __VRT_CMD_THREADS_H__

#include "common/include/daemon_request_queue.h"

int vrt_cmd_threads_init(void);
void vrt_cmd_threads_cleanup(void);
struct daemon_request_queue *vrt_cmd_thread_queue_get(ExamsgID id);

#endif /* __VRT_CMD_THREADS_H__ */
