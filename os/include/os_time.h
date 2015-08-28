/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_TIME_H
#define _OS_TIME_H

#include "os/include/os_inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

static inline bool TIMESPEC_IS_VALID(const struct timespec *ts)
{
    return ts != NULL && ts->tv_sec >= 0 && ts->tv_nsec >= 0
           && ts->tv_nsec < 1000000000;
}

static inline bool TIMEVAL_IS_VALID(const struct timeval *tv)
{
    return tv != NULL && tv->tv_sec >= 0 && tv->tv_usec >= 0
           && tv->tv_usec < 1000000;
}

#ifdef WIN32

#include "os/include/os_windows.h"

struct timespec
{
    int tv_sec;
    long int tv_nsec;
};

#else /* WIN32 */

#include <sys/time.h>

#endif /* WIN32 */

/**
 * Suspends the current thread for a least a specified amount of seconds.
 * On Linux the sleep is interruptible but is automatically restarted until
 * the specified time is elapsed. On Windows, the sleep is uninterruptible.
 * When the value is zero, there is no system call.
 *
 * @param seconds  number of seconds to wait.
 *                 Between 0 and 3600 (1 hour).
 *
 * @return 0 in case of success or negative error code in case of error.
 *
 * @os_replace{Linux, sleep}
 * @os_replace{Windows, SleepEx}
 */
int os_sleep(uint32_t seconds);

/**
 * Suspends the current thread for a least a specified amount of milliseconds.
 * On Linux the sleep is interruptible but is automatically restarted until
 * the specified time is elapsed. On Windows, the sleep is uninterruptible.
 * When the value is zero, there is no system call.
 *
 * @param mseconds  number of milliseconds to wait.
 *                  Between 0 and 3,600,000 (1 hour).
 *
 * @return 0 in case of success or negative error code in case of error.
 *
 * @os_replace{Linux, usleep}
 * @os_replace{Windows, SleepEx}
 */
int os_millisleep(uint32_t mseconds);

/**
 * Suspends the current thread for a least a specified amount of microseconds.
 * On Linux the sleep is interruptible but is automatically restarted until
 * the specified time is elapsed. On Windows, the sleep is uninterruptible.
 * When the value is zero, there is no system call.
 *
 * WARNING! On Windows the resolution is only one millisecond. There is an
 * integer division by 1000 of the useconds value.
 *
 * @param useconds  number of microseconds to wait.
 *                  Between 0 and 3,600,000,000 (1 hour).
 *
 * @return 0 in case of success or negative error code in case of error.
 *
 * @os_replace{Linux, usleep, nanosleep}
 */
int os_microsleep(uint32_t useconds);

/**
 * gives time of a monotonic non configurable clock.
 * Original date of clock is arbitrary, this fuction is mainly usefull to
 * compute how much time was spent in a specific part of code by applying a
 * diff between begining date and ending date
 *
 * @param tp input buffer to retrieve date
 *
 * return 0 in case of success or negative error code in case of failure.
 *
 * @os_replace{Linux, clock_gettime, gettimeofday}
 * @os_replace{Windows, GetTickCount, GetTickCount64}
 */
int os_get_monotonic_time(struct timespec *tp);

/**
 * Return the time expressed in seconds and microseconds since
 * midnight, January 1, 1970.
 *
 * @param[out] tv  Current time expressed as a timeval
 *
 * @return 0 if successful, negative error code in case of error
 *
 * @os_replace{Linux, gettimeofday}
 * @os_replace{Windows, gettimeofday, GetSystemTimeAsFileTime}
 */
int os_gettimeofday(struct timeval *tv);


/**
 * Return the difference between two timeval structures.
 * The result is normalized so that "tv_usec" has a value in the range
 * [0:999999].
 *
 * @param[in] lhs  First timeval
 * @param[in] rhs  Second timeval
 *
 * @return Difference as a timeval
 */
struct timeval os_timeval_diff(const struct timeval *lhs,
                               const struct timeval *rhs);

/**
 * Get the number of milliseconds since the Unix epoch
 *
 * @return Number of milliseconds since the Unix epoch
 *
 * @os_replace{Linux, gettimeofday}
 * @os_replace{Windows, gettimeofday}
 */
uint64_t os_gettimeofday_msec(void);

/**
 * converts the calendar time timep to broken-time representation, expressed
 * relative to the user's specified time zone.
 * @param[in]  timep
 * @param[out] result
 *
 * return true if conversion is successfull
 *
 * @os_replace{Linux, localtime, localtime_r}
 * @os_replace{Windows, localtime, localtime_s}
 */
bool os_localtime(const time_t *timep, struct tm *result);

#ifdef __cplusplus
}
#endif


/**
 * Convert a time value into a character string.
 *
 * @param[in]  timep  time value as a time_t
 * @param[out] buf    output string (contains exactly 26 characters)
 *
 * @return true if the conversion is successful
 *
 * @os_replace{Linux, ctime, ctime_r}
 * @os_replace{Windows, ctime, ctime_s}
 */
bool os_ctime(const time_t *timep, char *buf);

/**
 * Build the string representation of a date in long ISO format augmented
 * with millisecond precision (YYYY-MM-DD hh:mm:ss.iii where iii are ms).
 *
 * NOTE: This is *not* thread-safe.
 *
 * @param[in] date  Date
 * @param[in] msec  Milliseconds
 *
 * @return Date string
 */
const char *os_date_msec_to_str(const struct tm *date, int msec);

#endif /* _OS_TIME_H */

