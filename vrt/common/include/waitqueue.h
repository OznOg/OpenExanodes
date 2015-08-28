/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __WAITQUEUE_H__
#define __WAITQUEUE_H__


#include "os/include/os_semaphore.h"
#include "os/include/os_thread.h"


struct wait_queue;

typedef struct
{
    os_thread_mutex_t mutex;
    os_sem_t sem;
    int waiter;
} wait_queue_head_t;


static inline void init_waitqueue_head(wait_queue_head_t *wq)
{
    os_thread_mutex_init(&wq->mutex);
    os_sem_init(&wq->sem, 0);
    wq->waiter = 0;
}

static inline void clean_waitqueue_head(wait_queue_head_t *wq)
{
    os_thread_mutex_destroy(&wq->mutex);
    os_sem_destroy(&wq->sem);
}


#define wait_event(wq, EVENT)                   \
    do                                          \
    {                                           \
        int test_event;                         \
        do                                      \
        {                                       \
            os_thread_mutex_lock(&(wq).mutex);    \
            test_event = EVENT ;                \
            if (!test_event)                    \
                (wq).waiter++;                  \
            os_thread_mutex_unlock(&(wq).mutex);  \
            if (!test_event)                    \
                os_sem_wait(&(wq).sem);         \
        } while (!test_event);                  \
    } while (0)


/*
 * wait_event_or_timeout is not really correct, because if we
 * are waked up but the event is false, we reset the wait timeout
 * and so we can wait more than timeout
 */
#define wait_event_or_timeout(wq, EVENT, ms_timeout)            \
    do                                                          \
    {                                                           \
        int test_event;                                         \
        int wait_result;                                        \
        do                                                      \
	{                                                       \
            os_thread_mutex_lock(&(wq).mutex);                    \
            test_event = EVENT ;                                \
            if (!test_event)                                    \
                (wq).waiter++;                                  \
            os_thread_mutex_unlock(&(wq).mutex);                  \
            if (!test_event)                                    \
                wait_result = os_sem_waittimeout(&(wq).sem , ms_timeout);  \
	    else                                                \
                break;                                          \
            if (wait_result == 0)                               \
                continue;                                       \
            os_thread_mutex_lock(&(wq).mutex);                    \
            if ((wq).waiter>0)                                  \
	    {                                                   \
                (wq).waiter--;                                  \
                os_thread_mutex_unlock(&(wq).mutex);              \
                break;                                          \
	    }                                                   \
            os_thread_mutex_unlock(&(wq).mutex);                  \
            os_sem_wait(&(wq).sem);                             \
        } while (1);                                            \
    } while (0)


#define wake_up_all(wq) wake_up(wq)


static inline void wake_up(wait_queue_head_t *wq_head)
{
    os_thread_mutex_lock(&wq_head->mutex);
    while (wq_head->waiter > 0)
    {
        os_sem_post(&wq_head->sem);
        wq_head->waiter--;
    }
    os_thread_mutex_unlock(&wq_head->mutex);
}


#endif
