/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_time.h"
#include "os/include/os_assert.h"

#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

int os_sleep(uint32_t seconds)
{
    if (seconds > 3600U)
        return -EINVAL;

    return os_microsleep(seconds * 1000 * 1000);
}

int os_millisleep(uint32_t mseconds)
{
    if (mseconds > 3600000U)
        return -EINVAL;

    return os_microsleep(mseconds * 1000);
}

int os_microsleep(uint32_t useconds)
{
    struct timespec req;
    struct timespec rem;
    int ret;

    if (useconds > 3600000000U)
        return -EINVAL;

    /* The nanosleep() system call schedules the process even though the timeout
     * is zero. We don't want that. Especially we don't want to schedule the
     * examsgd process each time it sends a message send when the backoff is
     * set to zero */
    if (useconds == 0)
        return 0;

    req.tv_sec  = useconds / 1000000;
    req.tv_nsec = (useconds % 1000000) * 1000;

    OS_ASSERT(TIMESPEC_IS_VALID(&req));

    /* Restart the sleep with the remaining time in case of interruption. */
    do
    {
        ret = nanosleep(&req, &rem);
        if (ret != 0 && errno == EINTR)
            req = rem;
    }
    while(ret != 0 && errno == EINTR);

    if (ret != 0)
        return -errno;

    return 0;
}

int os_get_monotonic_time(struct timespec *tp)
{
    int retval;

    retval = clock_gettime(CLOCK_MONOTONIC, tp);
    if (retval == -1)
	retval = -errno;
    else
        OS_ASSERT(TIMESPEC_IS_VALID(tp));

    return retval;
}


int os_gettimeofday(struct timeval *tv)
{
    int retval;

    retval = gettimeofday(tv, NULL);
    if (retval == -1)
	retval = -errno;

    return retval;
}


struct timeval os_timeval_diff(const struct timeval *lhs,
                               const struct timeval *rhs)
{
    struct timeval diff;

    diff.tv_sec = lhs->tv_sec - rhs->tv_sec;
    diff.tv_usec = lhs->tv_usec;

    while (rhs->tv_usec > diff.tv_usec)
    {
        --diff.tv_sec;
        diff.tv_usec += 1000000;
    }
    diff.tv_usec -= rhs->tv_usec;

    return diff;
}


uint64_t os_gettimeofday_msec(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * uint64_t_C(1000) + now.tv_usec / uint64_t_C(1000);
}

bool os_localtime(const time_t *timep, struct tm *result)
{
  return localtime_r(timep, result) != NULL ? true : false;
}


bool os_ctime(const time_t *timep, char *buf)
{
    return ctime_r(timep, buf) != NULL ? true : false;
}
