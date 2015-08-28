/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __COMPLETION_H__
#define __COMPLETION_H__


#include "os/include/os_semaphore.h"


/**
 * Completion variable. A completion variable is a way to synchronize
 * two threads when one thread needs to signal to the other thread
 * that a given event has occurred.
 * The first thread initializes the completion variable and waits on
 * it while the second thread performs some work. When done, the
 * second thread uses the completion variable to wake up the first
 * one.
 */
typedef struct completion
{
    os_sem_t sem;
    int err;
} completion_t;


static inline void init_completion(completion_t *completion)
{
    os_sem_init(&completion->sem, 0);
    completion->err = 0;
}


static inline int wait_for_completion(completion_t *completion)
{
    os_sem_wait(&completion->sem);

    os_sem_destroy(&completion->sem);
    return completion->err;
}


static inline void complete(completion_t *completion, int err)
{
    completion->err = err;
    os_sem_post(&completion->sem);
}

#endif
