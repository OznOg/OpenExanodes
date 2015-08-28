/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_DAEMON_FAKES_H__
#define __UT_VRT_DAEMON_FAKES_H__

#include "common/include/daemon_request_queue.h"

int daemon_request_queue_get_request(struct daemon_request_queue *q,
	                             void *msg, size_t len,
				     ExamsgID *from)
{
    return 0;
}

int daemon_request_queue_reply(ExamsgHandle mh, ExamsgID from,
			       struct daemon_request_queue *queue,
			       const void *answer,  size_t answer_size)
{
    return 0;
}

struct daemon_request_queue * daemon_request_queue_new(const char *name)
{
    return NULL;
}

void daemon_request_queue_delete(struct daemon_request_queue *q)
{
}

void daemon_request_queue_break_get_request(struct daemon_request_queue *queue)
{
}

#endif
