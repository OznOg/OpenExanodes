/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "common/include/daemon_request_queue.h"

#include <string.h>
#include "os/include/strlcpy.h"
#include <errno.h>

#include "os/include/os_mem.h"
#include "common/include/daemon_api_server.h"
#include "common/include/exa_assert.h"
#include "os/include/os_semaphore.h"

#ifdef __KERNEL__
#error "This cannot be used in kernel code"
#endif

/* --- daemon_request_queue ------------------------------------------ */
/**
 * Private structure for handling the queue of requests
 */
struct daemon_request_queue {
  enum {
           REQ_STATE_NONE,                        /**< no request */
           REQ_STATE_PENDING,                     /**< one request pending */
           REQ_STATE_INTERRUPTED,                 /**< one request interrupted */
           REQ_STATE_INTERRUPTED_AND_PENDING,     /**< one request interrupted and one request pending */
           REQ_STATE_BROKEN /**< all daemon_queue_queue_get_request() on this queue will end with 1 */
#define DAEMON_REQUEST_STATE_IS_VALID(s) ((s) >= REQ_STATE_NONE \
	                                  && (s) <= REQ_STATE_BROKEN)
       } state;

  char name[EXA_MAXSIZE_REQUEST_QUEUE_NAME + 1];

  /* current request */
  char current_msg[DAEMON_REQUEST_MSG_MAXSIZE];
  size_t current_len;
  ExamsgID current_from;

  /* pending request */
  char pending_msg[DAEMON_REQUEST_MSG_MAXSIZE];
  size_t pending_len;
  ExamsgID pending_from;

  /* Wait up working thread when a request is incoming */
  os_sem_t sem_events_to_handle;

  /* Mutex for requests */
  os_sem_t lock;
};

/* --- daemon_request_queue_new -------------------------------------- */
/**
 * Initialize a queue of requests
 *
 *  \param[in] name Name of the request queue to create
 *
 * \return struct daemon_request_queue *
 */
struct daemon_request_queue *
daemon_request_queue_new(const char *name)
{
  struct daemon_request_queue * q;

  q = os_malloc(sizeof(struct daemon_request_queue));
  if (!q)
    return NULL;
  memset(q, 0, sizeof(struct daemon_request_queue));

  q->state = REQ_STATE_NONE;
  strlcpy(q->name, name, sizeof(q->name));
  os_sem_init(&q->sem_events_to_handle, 0);
  os_sem_init(&q->lock, 1);

  return q;
}

/* --- daemon_request_queue_delete ----------------------------------- */
/**
 * Free a queue of requests
 *
 *  \param[in] queue    The request queue to delete
 *
 * \return void
 */
void daemon_request_queue_delete(struct daemon_request_queue *queue)
{
  EXA_ASSERT(queue);
  os_free(queue);
}

/* --- daemon_queue_add_request -------------------------------------- */
/**
 * Add a request to the queue
 *
 * \param[in] queue     Request queue handler.
 * \param[in] msg       Examsg of the request.
 * \param[in] len       Size of the buffer msg.
 * \param[in] from      Sender of the request.
 *
 * \return void
 */
void daemon_request_queue_add_request(struct daemon_request_queue *queue,
	                              const void *msg, size_t len,
				      ExamsgID from)
{
  EXA_ASSERT(len <= DAEMON_REQUEST_MSG_MAXSIZE);

  os_sem_wait(&queue->lock);

  exalog_debug("Queue `%s': Add a request", queue->name);

  EXA_ASSERT_VERBOSE(DAEMON_REQUEST_STATE_IS_VALID(queue->state),
	             "Invalid state %d for queue '%s'",
		     queue->state, queue->name);

  switch(queue->state)
    {
      case REQ_STATE_NONE:
        memset(&queue->current_msg, 0, sizeof(queue->current_msg));
        memcpy(&queue->current_msg, msg, len);
        queue->current_len = len;
        queue->current_from = from;
        queue->state = REQ_STATE_PENDING;
        break;

      case REQ_STATE_INTERRUPTED:
        memset(&queue->pending_msg, 0, sizeof(queue->pending_msg));
        memcpy(&queue->pending_msg, msg, len);
        queue->pending_len = len;
        queue->pending_from = from;
        queue->state = REQ_STATE_INTERRUPTED_AND_PENDING;
        break;

      case REQ_STATE_PENDING:
      case REQ_STATE_INTERRUPTED_AND_PENDING:
	/* cannot handle several requests at the same time: there is one
	 * request pending and it has not been interrupted. */
      case REQ_STATE_BROKEN:
	EXA_ASSERT_VERBOSE(false, "Inconsistent state %d for queue '%s'",
		           queue->state, queue->name);
        break;
    }

  /* wake up worker thread */
  os_sem_post(&queue->sem_events_to_handle);

  os_sem_post(&queue->lock);
}

/* --- daemon_queue_add_interrupt ------------------------------------ */
/**
 * Add an interrupt to the queue
 *
 * \param[in] queue     Request queue handler.
 * \param[in] mh        Examsg Handler.
 * \param[in] from	Sender of the request.
 *
 * \return void
 */
void
daemon_request_queue_add_interrupt(struct daemon_request_queue *queue,
	                           ExamsgHandle mh, ExamsgID from)
{
  int ret;

  os_sem_wait(&queue->lock);

  exalog_debug("Queue `%s': Add interrupt", queue->name);

  EXA_ASSERT_VERBOSE(DAEMON_REQUEST_STATE_IS_VALID(queue->state),
	             "Invalid state %d for queue '%s'",
		     queue->state, queue->name);

  switch(queue->state)
    {
      case REQ_STATE_NONE:
        /* Throw away the interrupt because we are not currently handling a
         * request. This may seem odd, but it is possible and normal to
         * receive an interrupt when we are not currently handling a request.
         * If the daemon send the reply at the same time admind send the int
         * */
        break;
      case REQ_STATE_PENDING:
      case REQ_STATE_INTERRUPTED_AND_PENDING:
        /* ack the interrupt and set the request to state interrupted */
        ret = admwrk_daemon_ackinterrupt(mh, from);
        EXA_ASSERT(ret == 0); /* cannot continue if examsg does not work */

        /* New state of the queue */
        queue->state = REQ_STATE_INTERRUPTED;
        break;

      case REQ_STATE_INTERRUPTED:
	/* We should not receive 2 interrupts for the same request */
	EXA_ASSERT_VERBOSE(false, "We should not receive 2 interrupts"
		                  " for the same request");
	break;

      case REQ_STATE_BROKEN:
	EXA_ASSERT(false); /* FIXME why is this actually impossible ? */
    }

    os_sem_post(&queue->lock);
}

/* --- daemon_queue_break_get_request -------------------------------------- */
/**
 * Break all daemon_request_queue_get_request(),
 *
 * \param[in] queue     Request queue handler.
 */
void
daemon_request_queue_break_get_request(struct daemon_request_queue * queue)
 {
   int inter;
   do
     {
       os_sem_wait(&queue->lock);

       EXA_ASSERT(queue->state != REQ_STATE_BROKEN);
       if (queue->state == REQ_STATE_NONE)
         {
           queue->state = REQ_STATE_BROKEN;
           os_sem_post(&queue->sem_events_to_handle);
           inter = 0;
         }
       else
         inter = 1;
       os_sem_post(&queue->lock);
     } while (inter);

 }

/* --- daemon_queue_get_request -------------------------------------- */
/**
 * Get a request to the queue
 *
 * \param[in] queue     Request queue handler.
 * \param[out] msg      Examsg of the request.
 * \param[in] len       Size of the buffer msg.
 * \param[out] from     Sender of the request.
 *
 * \return 0 if ok.
 */
int
daemon_request_queue_get_request(struct daemon_request_queue * queue,
	                         void *msg, size_t len, ExamsgID *from)
{
  EXA_ASSERT(msg);
  EXA_ASSERT(from);
  EXA_ASSERT(len <= DAEMON_REQUEST_MSG_MAXSIZE);

  while (1) /* read requests until we have a request to handle */
    {
      /* wait the pusher thread wake up me */
      os_sem_wait(&queue->sem_events_to_handle);

      /* take the lock */
      os_sem_wait(&queue->lock);

      EXA_ASSERT_VERBOSE(DAEMON_REQUEST_STATE_IS_VALID(queue->state),
	      "Invalid state %d for queue '%s'",
	      queue->state, queue->name);

      switch (queue->state)
        {
          case REQ_STATE_BROKEN:
              os_sem_post(&queue->sem_events_to_handle);
              os_sem_post(&queue->lock);
              return 1;
          case REQ_STATE_NONE:
            /* the pusher thread wake up me for nothing. Return to bed. */
            os_sem_post(&queue->lock);
            continue;
            break;
          case REQ_STATE_PENDING:
            /* We have just a request to handle. Easy case. */

            /* copy the request to the user's buffer */
            EXA_ASSERT(len>=queue->current_len);
            memcpy(msg, &queue->current_msg, len);
            *from = queue->current_from;

            break;
          case REQ_STATE_INTERRUPTED:
            /* The pusher thread give a request but cancel it immediately.
             * Delete the interrupted request. */
            exalog_debug("Queue `%s': Delete request (REQ_STATE_INTERRUPTED)",
                         queue->name);
            queue->state = REQ_STATE_NONE;

            /* Return to bed. */
            os_sem_post(&queue->lock);
            continue;
            break;
          case REQ_STATE_INTERRUPTED_AND_PENDING:
            exalog_warning("Queue `%s': Delete request. "
                           "Take pending request (REQ_STATE_INTERRUPTED_AND_PENDING)",
                           queue->name);

            /* move the request, overwriting the old interrupted request */
            memcpy(&queue->current_msg, &queue->pending_msg, sizeof(queue->current_msg));
            queue->current_from = queue->pending_from;
            queue->current_len = queue->pending_len;

            /* delete the old cell */
            memset(&queue->pending_msg, 0, sizeof(queue->pending_msg));
            memset(&queue->pending_from, 0, sizeof(queue->pending_from));
            queue->pending_len = 0;

            /* Set new state */
            queue->state = REQ_STATE_PENDING;

            /* we are now in the same case as REQ_STATE_PENDING but we must
             * not make another loop in "while". */

            /* copy the request to the user's buffer */
            EXA_ASSERT(len>=queue->current_len);
            memcpy(msg, &queue->current_msg, len);
            *from = queue->current_from;

            break;
        }

      /* release the lock */
      os_sem_post(&queue->lock);

      break;
    }

  exalog_debug("Queue `%s': Got request", queue->name);
  return 0;
}

/* --- daemon_queue_need_interrupt ----------------------------------- */
/**
 * Check the request is not in the state interrupted. The worker
 * thread must not reply if the request has been set to interrupted
 *
 * WARNING: call daemon_queue_end_request() after this function !!!
 *
 * \param[in] queue     Request queue handler.
 *
 * \return 0 if the request is ok, !=0 if the request is interrupted
 */
static int
daemon_request_queue_need_interrupt(struct daemon_request_queue * queue)
{
  int ret = 0;

  /* take the lock */
  os_sem_wait(&queue->lock);

  EXA_ASSERT_VERBOSE(DAEMON_REQUEST_STATE_IS_VALID(queue->state),
                     "Invalid state %d for queue '%s'",
	             queue->state, queue->name);

  switch (queue->state)
    {
      case REQ_STATE_PENDING:
        /* No interruption. Easy case. */
        ret = 0;
        break;

      case REQ_STATE_INTERRUPTED:
      case REQ_STATE_INTERRUPTED_AND_PENDING:
        /* interruption! */
        ret = 1;
        break;

      case REQ_STATE_NONE:
	/* we are currently handling a request. The request cannot disappear! */
      case REQ_STATE_BROKEN:
        EXA_ASSERT_VERBOSE(false, "State '%s' is inconsistent",
		           queue->state == REQ_STATE_NONE ? "none" : "broken");
        break;
    }

  exalog_debug("Queue `%s': Check interruptions: interrupted=%d", queue->name, ret);

  return ret;
}

/* --- daemon_queue_end_request -------------------------------------- */
/**
 * Free the request. Must be call after the daemon reply to the
 * request.
 *
 * WARNING: Must be call even if the request has been interrupted.
 *
 * \param[in] queue     Request queue handler.
 *
 * \return void
 */
static void
daemon_request_queue_end_request(struct daemon_request_queue * queue)
{
  EXA_ASSERT_VERBOSE(DAEMON_REQUEST_STATE_IS_VALID(queue->state),
                     "Invalid state %d for queue '%s'",
	             queue->state, queue->name);

  switch (queue->state)
    {
      case REQ_STATE_PENDING:
      case REQ_STATE_INTERRUPTED:
        /* there is no more request */
        /* Set new state */
        queue->state = REQ_STATE_NONE;
        break;

      case REQ_STATE_INTERRUPTED_AND_PENDING:
        /* move the request, overwriting the old interrupted request */
        memcpy(&queue->current_msg, &queue->pending_msg, sizeof(queue->current_msg));
        queue->current_from = queue->pending_from;
        queue->current_len = queue->pending_len;

        /* delete the old cell */
        memset(&queue->pending_msg, 0, sizeof(queue->pending_msg));
        memset(&queue->pending_from, 0, sizeof(queue->pending_from));
        queue->pending_len = 0;

        /* Set new state */
        queue->state = REQ_STATE_PENDING;
        break;

      case REQ_STATE_NONE:
	/* we are currently handling a request. The request cannot disappear! */
      case REQ_STATE_BROKEN:
        EXA_ASSERT_VERBOSE(false, "State '%s' is inconsistent",
		           queue->state == REQ_STATE_NONE ? "none" : "broken");
    }

  exalog_debug("Queue `%s': Request terminated", queue->name);

  /* release the lock */
  os_sem_post(&queue->lock);
}

int daemon_request_queue_reply(ExamsgHandle mh, ExamsgID from,
			       struct daemon_request_queue *queue,
			       const void *answer,  size_t answer_size)
{
  int rv = 0;

  if (!daemon_request_queue_need_interrupt(queue))
    rv = admwrk_daemon_reply(mh, from, answer, answer_size);

  daemon_request_queue_end_request(queue);

  return rv;
}


