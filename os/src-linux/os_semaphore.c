/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_assert.h"
#include "os/include/os_error.h"
#include "os/include/os_semaphore.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_time.h"

#include <sys/time.h>
#include <stdlib.h>

int os_sem_init(os_sem_t * sem, int value)
{
    return sem_init(sem, 0, value);
}

int os_sem_wait_intr(os_sem_t *sem)
{
    if (sem_wait(sem) == 0)
        return 0;

    /* XXX Nowhere in Exanodes is the return code checked, so we just assert */
    OS_ASSERT_VERBOSE(errno == EINTR, "sem_wait() failed: error 0x%02x", errno);

    return -EINTR;
}

int os_sem_wait(os_sem_t * sem)
{
    while (os_sem_wait_intr(sem) == -EINTR)
        ;
    return 0;
}

void os_sem_post(os_sem_t * sem)
{
    sem_post(sem);
}

int os_sem_waittimeout(os_sem_t * sem, int timeout_ms)
{
    struct timeval tv;
    struct timespec time;

    gettimeofday(&tv,NULL);

    tv.tv_usec += (timeout_ms % 1000) * 1000;
    tv.tv_sec += timeout_ms / 1000;
    if (tv.tv_usec >= 1000000)
    {
        tv.tv_usec -= 1000000;
        tv.tv_sec++;
    }

    OS_ASSERT(TIMEVAL_IS_VALID(&tv));

    time.tv_nsec = tv.tv_usec * 1000;
    time.tv_sec = tv.tv_sec;

    OS_ASSERT(TIMESPEC_IS_VALID(&time));

    return sem_timedwait(sem, &time) == 0 ? 0 : -errno;
}

void os_sem_destroy(os_sem_t * sem)
{
    sem_destroy(sem);
}
