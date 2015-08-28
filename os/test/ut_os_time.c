/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "os/include/os_time.h"

#include <unit_testing.h>


UT_SECTION(os_time_validation)

ut_test(os_timeval_valid)
{
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    UT_ASSERT(TIMEVAL_IS_VALID(&tv));

    tv.tv_sec = 1;
    tv.tv_usec = 1;
    UT_ASSERT(TIMEVAL_IS_VALID(&tv));

    tv.tv_sec = INT32_MAX;  /* INT64_MAX is too big win32 */
    tv.tv_usec = 999999;
    UT_ASSERT(TIMEVAL_IS_VALID(&tv));
}

ut_test(os_timeval_invalid)
{
    struct timeval tv;
    struct timeval *null_pointer = NULL;

    tv.tv_sec = 0;
    tv.tv_usec = -1;
    UT_ASSERT(!TIMEVAL_IS_VALID(&tv));

    tv.tv_sec = 1;
    tv.tv_usec = 1000000;
    UT_ASSERT(!TIMEVAL_IS_VALID(&tv));

    tv.tv_sec = 1;
    tv.tv_usec = UINT32_MAX;
    UT_ASSERT(!TIMEVAL_IS_VALID(&tv));

    tv.tv_sec = -1;
    tv.tv_usec = 999999;
    UT_ASSERT(!TIMEVAL_IS_VALID(&tv));

    UT_ASSERT(!TIMEVAL_IS_VALID(null_pointer));
}

ut_test(os_timespec_valid)
{
    struct timespec tv;

    tv.tv_sec = 0;
    tv.tv_nsec = 0;
    UT_ASSERT(TIMESPEC_IS_VALID(&tv));

    tv.tv_sec = 1;
    tv.tv_nsec = 1;
    UT_ASSERT(TIMESPEC_IS_VALID(&tv));

    tv.tv_sec = INT32_MAX;  /* INT64_MAX is too big win32 */
    tv.tv_nsec = 999999999;
    UT_ASSERT(TIMESPEC_IS_VALID(&tv));
}

ut_test(os_timespec_invalid)
{
    struct timespec tv;
    struct timespec *null_pointer = NULL;

    tv.tv_sec = 0;
    tv.tv_nsec = -1;
    UT_ASSERT(!TIMESPEC_IS_VALID(&tv));

    tv.tv_sec = 1;
    tv.tv_nsec = 1000000000;
    UT_ASSERT(!TIMESPEC_IS_VALID(&tv));

    tv.tv_sec = 1;
    tv.tv_nsec = UINT32_MAX;
    UT_ASSERT(!TIMESPEC_IS_VALID(&tv));

    tv.tv_sec = -1;
    tv.tv_nsec = 999999999;
    UT_ASSERT(!TIMESPEC_IS_VALID(&tv));

    UT_ASSERT(!TIMESPEC_IS_VALID(null_pointer));
}

UT_SECTION(os_timeval_diff)

ut_test(os_timeval_diff_zero)
{
    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    struct timeval diff;

    diff = os_timeval_diff(&tv, &tv);

    UT_ASSERT_EQUAL(0, diff.tv_sec);
    UT_ASSERT_EQUAL(0, diff.tv_usec);
}

ut_test(os_timeval_diff_1_usec)
{
    struct timeval tv1 = {.tv_sec = 0, .tv_usec = 0};
    struct timeval tv2 = {.tv_sec = 0, .tv_usec = 1};
    struct timeval diff;

    diff = os_timeval_diff(&tv2, &tv1);

    UT_ASSERT_EQUAL(0, diff.tv_sec);
    UT_ASSERT_EQUAL(1, diff.tv_usec);
}

ut_test(os_timeval_diff_999999_usecs)
{
    struct timeval tv1 = {.tv_sec = 0, .tv_usec = 0};
    struct timeval tv2 = {.tv_sec = 0, .tv_usec = 999999};
    struct timeval diff;

    diff = os_timeval_diff(&tv2, &tv1);

    UT_ASSERT_EQUAL(0, diff.tv_sec);
    UT_ASSERT_EQUAL(999999, diff.tv_usec);
}

ut_test(os_timeval_diff_normalize_1_usec)
{
    struct timeval tv1 = {.tv_sec = 0, .tv_usec = 999999};
    struct timeval tv2 = {.tv_sec = 1, .tv_usec = 0};
    struct timeval diff;

    diff = os_timeval_diff(&tv2, &tv1);

    UT_ASSERT_EQUAL(0, diff.tv_sec);
    UT_ASSERT_EQUAL(1, diff.tv_usec);
}

ut_test(os_timeval_diff_normalize_999999_usecs)
{
    struct timeval tv1 = {.tv_sec = 0, .tv_usec = 999999};
    struct timeval tv2 = {.tv_sec = 1, .tv_usec = 999998};
    struct timeval diff;

    diff = os_timeval_diff(&tv2, &tv1);

    UT_ASSERT_EQUAL(0, diff.tv_sec);
    UT_ASSERT_EQUAL(999999, diff.tv_usec);
}


UT_SECTION(os_sleep)

/* os_sleep() is tested against the native time() function considered
 * as a reference because time() is a standard C function implemented
 * on all platforms.
 */
ut_test(os_sleep_1s_resolution) __ut_lengthy
{
    time_t t1;
    time_t t2;
    const int nb_sleeps = 20;
    const uint32_t sleep_duration = 1;
    int i;

    /* We make several sleeps in order to increase the resolution of
     * this test. The higher the number of sleeps, the better the
     * test resolution. But for a high resolution, the test will be
     * very long...
     */
    ut_printf("Average resolution is %0.3f second", (float) sleep_duration / nb_sleeps);
    time(&t1);
    for (i = 0; i < nb_sleeps; i++)
        os_sleep(sleep_duration);
    time(&t2);

    UT_ASSERT(t2 - t1 >=  nb_sleeps * sleep_duration);
    UT_ASSERT(t2 - t1 <=  nb_sleeps * sleep_duration * 150 / 100);
}

ut_test(os_sleep_out_of_range)
{
    UT_ASSERT(os_sleep(3600 + 1) == -EINVAL);
}


UT_SECTION(os_millisleep)

ut_test(os_millisleep_40ms) __ut_lengthy
{
    time_t t1;
    time_t t2;
    const int nb_msleeps = 250;
    const uint32_t msleep_duration = 40; /* in milli seconds */
    int i;

    time(&t1);
    for (i = 0; i < nb_msleeps; i++)
        os_millisleep(msleep_duration);
    time(&t2);

    /* The average sleep duration MUST be at least "usleep_duration" long */
    UT_ASSERT((t2 - t1) * 1000 >= nb_msleeps * msleep_duration);

    /* The average sleep duration MUST not be too long. Here, we check
     * that the average sleep was less than 50% longer than requested.
     */
    UT_ASSERT((t2 - t1) * 1000 <= nb_msleeps * msleep_duration * 150 / 100);
}

ut_test(os_millisleep_out_of_range)
{
    UT_ASSERT(os_sleep(3600 * 1000 + 1) == -EINVAL);
}


UT_SECTION(os_microsleep)

ut_test(os_microsleep) __ut_lengthy
{
    time_t t1;
    time_t t2;
    const int nb_microsleeps = 250;
    const uint32_t microsleep_duration = 40 * 1000; /* in microseconds */
    int i;

    time(&t1);
    for (i = 0; i < nb_microsleeps; i++)
        os_microsleep(microsleep_duration);
    time(&t2);

    /* The average sleep duration MUST be at least "microsleep_duration" long */
    UT_ASSERT((t2 - t1) * 1000000 >= nb_microsleeps * microsleep_duration);

    /* The average sleep duration MUST not be too long. Here, we check
     * that the average sleep was less than 50% longer than requested.
     */
    UT_ASSERT((t2 - t1) * 100000 <= nb_microsleeps * microsleep_duration * 150 / 100);
}

UT_SECTION(os_get_monotonic_time)

/* FIXME: It would be nice to set the system clock back during this
 * test.
 */
ut_test(os_get_monotonic_time_monotonicity)
{
    struct timespec after;
    struct timespec before;
    int ret;
    bool is_monotonic;
    int i;

    ret = os_get_monotonic_time(&before);
    UT_ASSERT_EQUAL(ret, 0);

    for (i = 0; i < 100; i++)
    {
        os_millisleep(22);

        ret = os_get_monotonic_time(&after);
        UT_ASSERT_EQUAL(ret, 0);

        is_monotonic = (after.tv_sec == before.tv_sec) ?
                before.tv_nsec < after.tv_nsec :
                before.tv_sec  < after.tv_sec;
        UT_ASSERT(is_monotonic);

        before = after;
    }
}


UT_SECTION(os_gettimeofday)

/* os_gettimeofday() MUST return the number of seconds and
 * milliseconds elapsed since Epoch. This test case compares the
 * tv_sec value returned by os_gettimeofday() with the value provided
 * by the native time() function.
 */
ut_test(os_gettimeofday_epoch)
{
    time_t t1;
    time_t t2;
    struct timeval tv;

    time(&t1);
    os_gettimeofday(&tv);
    time(&t2);

    UT_ASSERT(tv.tv_sec >= t1 && tv.tv_sec <= t2);
}

/* The "tv_usec" member of a "struct timeval" filled by
 * os_gettimeofday() MUST be in the range [0:999999]
 */
ut_test(os_gettimeofday_tv_usec_consistency)
{
    struct timeval tv;
    int i;

    for (i = 0; i < 100; i++)
    {
        os_gettimeofday(&tv);
        UT_ASSERT(tv.tv_usec >= 0);
        UT_ASSERT(tv.tv_usec <= 999999);
        os_millisleep(20);
    }
}


UT_SECTION(os_gettimeofday_msec)

static int bias;

ut_setup()
{
#ifdef WIN32
    /* Get timezone information and set bias to UTC accordingly.
     * From MSDN, UTC = local time + bias
     */
    TIME_ZONE_INFORMATION tzi;
    GetTimeZoneInformation(&tzi);
    /* Convert timezone bias from minutes to hours */
    bias = tzi.Bias / 60;
#else
    /* Make sure the timezone is UTC and set bias to 0 hour */
    setenv("TZ", ":UTC", 1);
    bias = 0;
#endif
}

ut_cleanup()
{
    bias = 0;
}

/* Compare os_gettimeofday_msec() against time() */
ut_test(os_gettimeofday_msec_epoch)
{
    time_t t1;
    time_t t2;
    uint64_t now;

    time(&t1);
    now = os_gettimeofday_msec();
    time(&t2);

    UT_ASSERT(t1 * uint64_t_C(1000) <= now);
    UT_ASSERT((t2 + 1) * uint64_t_C(1000) >= now);
}

ut_test(os_gettimeofday_msec_250ms)
{
    uint64_t t1;
    uint64_t t2;

    t1 = os_gettimeofday_msec();
    os_millisleep(250);
    t2 = os_gettimeofday_msec();

    UT_ASSERT(t2 - t1 >= 250);

    /* 10% tolerance */
    UT_ASSERT(t2 - t1 <= 250 * 110 / 100);
}


UT_SECTION(os_localtime)

ut_setup()
{
#ifdef WIN32
    /* Get timezone information and set bias to UTC accordingly.
     * From MSDN, UTC = local time + bias
     */
    TIME_ZONE_INFORMATION tzi;
    GetTimeZoneInformation(&tzi);
    /* Convert timezone bias from minutes to hours */
    bias = tzi.Bias / 60;
#else
    /* Make sure the timezone is UTC and set bias to 0 hour */
    setenv("TZ", ":UTC", 1);
    bias = 0;
#endif
}

ut_cleanup()
{
    bias = 0;
}

ut_test(os_localtime_epoch)
{
    time_t timep = 0;
    struct tm date;

    UT_ASSERT(os_localtime(&timep, &date));

    UT_ASSERT(date.tm_year == 70);
    UT_ASSERT(date.tm_mon  == 0);
    UT_ASSERT(date.tm_mday == 1);
    UT_ASSERT(date.tm_hour == 0 - bias);
    UT_ASSERT(date.tm_min  == 0);
    UT_ASSERT(date.tm_sec  == 0);
}

ut_test(os_localtime_some_date)
{
    time_t timep = 543216789L;
    struct tm date;

    UT_ASSERT(os_localtime(&timep, &date));

    UT_ASSERT(date.tm_year == 87);
    UT_ASSERT(date.tm_mon  == 2);
    UT_ASSERT(date.tm_mday == 20);
    UT_ASSERT(date.tm_hour == 5 - bias);
    UT_ASSERT(date.tm_min  == 33);
    UT_ASSERT(date.tm_sec  == 9);
}


UT_SECTION(os_ctime)

ut_setup()
{
#ifdef WIN32
    /* Get timezone information and set bias to UTC accordingly.
     * From MSDN, UTC = local time + bias
     */
    TIME_ZONE_INFORMATION tzi;
    GetTimeZoneInformation(&tzi);
    /* Convert timezone bias from minutes to seconds */
    bias = tzi.Bias * 60;
#else
    /* Make sure the timezone is UTC and set bias to 0 hour */
    setenv("TZ", ":UTC", 1);
    bias = 0;
#endif
}

ut_cleanup()
{
    bias = 0;
}

ut_test(os_ctime_Y2K)
{
    /* The following timestamp should trigger the Y2K bug */
    time_t t = 946684800;
    char buf[26];

    t += bias;

    UT_ASSERT(os_ctime(&t, buf));
#ifdef WIN32
    UT_ASSERT_EQUAL(0, strcmp(buf, "Sat Jan 01 00:00:00 2000\n"));
#else
    UT_ASSERT_EQUAL(0, strcmp(buf, "Sat Jan  1 00:00:00 2000\n"));
#endif
}

ut_test(os_ctime_1234567890)
{
    time_t t = 1234567890;
    char buf[26];

    t += bias;

    UT_ASSERT(os_ctime(&t, buf));
    UT_ASSERT_EQUAL(0, strcmp(buf, "Fri Feb 13 23:31:30 2009\n"));
}
