/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_REQUEST_QUEUE
#define H_REQUEST_QUEUE

#include "examsg/include/examsg.h"
#include "log/include/log.h"

/* === Internal API ================================================== */

#define EXA_MAXSIZE_REQUEST_QUEUE_NAME 15

#define DAEMON_REQUEST_MSG_MAXSIZE (8 * 1024)

/* struct daemon_request_queue (administration request queue) is a private
 * structure */
struct daemon_request_queue;

/* Create a new queue */
struct daemon_request_queue * daemon_request_queue_new(const char *name);

/* Delete a queue */
void daemon_request_queue_delete(struct daemon_request_queue *q);

/* Get the name of a queue, defined at its creation */
const char* daemon_request_queue_get_name(struct daemon_request_queue *q);

/* add a request to a queue */
void daemon_request_queue_add_request(struct daemon_request_queue *q,
	                              const void *msg, size_t len,
				      ExamsgID from);

/* add an interrupt to a queue */
void daemon_request_queue_add_interrupt(struct daemon_request_queue *q,
	                                ExamsgHandle mh, ExamsgID from);

/* break all daemon_request_queue_get_request() on this queue */
void daemon_request_queue_break_get_request(struct daemon_request_queue *queue);

/* get a request from a queue */
int daemon_request_queue_get_request(struct daemon_request_queue *q,
	                             void *msg, size_t len,
				     ExamsgID *from);

/*
 * Helper function that calls admwrk_daemon_reply after checking that
 * the queued request hasn't been interrupted and then frees the
 * request from the queue.
 */
int daemon_request_queue_reply(ExamsgHandle mh, ExamsgID from,
			       struct daemon_request_queue *queue,
			       const void *answer,  size_t answer_size);


#endif /* H_REQUEST_QUEUE */
