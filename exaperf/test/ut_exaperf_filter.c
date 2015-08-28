/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include <string.h>

#include "os/include/strlcpy.h"
#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_filter.h"

/**************************************************************/
UT_SECTION(exaperf_filter_build)

ut_test(empty)
{
    uint32_t filter = 0;

    filter = EXAPERF_FILTER_NONE;
    UT_ASSERT(exaperf_filter_empty(filter));

    filter = EXAPERF_FILTER_MEAN;
    UT_ASSERT(!exaperf_filter_empty(filter));
}

ut_test(add)
{
    uint32_t filter = 0;

    exaperf_filter_add(&filter, EXAPERF_FILTER_MEAN);
    UT_ASSERT(filter == 0x01);
    exaperf_filter_add(&filter, EXAPERF_FILTER_MEDIAN);
    UT_ASSERT(filter == 0x11);
}

ut_test(contains)
{
    uint32_t filter = 0;

    exaperf_filter_add(&filter, EXAPERF_FILTER_MEAN);
    UT_ASSERT(exaperf_filter_contains(filter, EXAPERF_FILTER_MEAN));

    exaperf_filter_add(&filter, EXAPERF_FILTER_MEDIAN);
    UT_ASSERT(exaperf_filter_contains(filter, EXAPERF_FILTER_MEDIAN));
}

ut_test(remove)
{
    uint32_t filter = 0;

    exaperf_filter_add(&filter, EXAPERF_FILTER_RAW);
    UT_ASSERT(exaperf_filter_contains(filter, EXAPERF_FILTER_RAW));

    exaperf_filter_add(&filter, EXAPERF_FILTER_MEDIAN);
    UT_ASSERT(exaperf_filter_contains(filter, EXAPERF_FILTER_MEDIAN));

    exaperf_filter_remove(&filter, EXAPERF_FILTER_RAW);
    UT_ASSERT(!exaperf_filter_contains(filter, EXAPERF_FILTER_RAW));
    UT_ASSERT(exaperf_filter_contains(filter, EXAPERF_FILTER_MEDIAN));

    exaperf_filter_remove(&filter, EXAPERF_FILTER_MEDIAN);
    UT_ASSERT(exaperf_filter_empty(filter));
}

ut_test(build_each_value)
{
    uint32_t filter = 0;
    exaperf_err_t  err;
    char values_buf[EXAPERF_PARAM_NBMAX_VALUES * (EXAPERF_MAX_TOKEN_LEN + 1)];
    char * values[EXAPERF_PARAM_NBMAX_VALUES];
    int i;

    for (i=0; i<EXAPERF_PARAM_NBMAX_VALUES; i++)
	values[i] = &values_buf[i * (EXAPERF_MAX_TOKEN_LEN + 1)];

    strlcpy(values[0], "mean", EXAPERF_MAX_TOKEN_LEN + 1);
    err = exaperf_filter_build(&filter, (const char **) values, 1);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(EXAPERF_FILTER_MEAN == filter);

    strlcpy(values[0], "min", EXAPERF_MAX_TOKEN_LEN + 1);
    err = exaperf_filter_build(&filter, (const char **) values, 1);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(EXAPERF_FILTER_MIN == filter);

    strlcpy(values[0], "max", EXAPERF_MAX_TOKEN_LEN + 1);
    err = exaperf_filter_build(&filter, (const char **) values, 1);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(EXAPERF_FILTER_MAX == filter);

    strlcpy(values[0], "raw", EXAPERF_MAX_TOKEN_LEN + 1);
    err = exaperf_filter_build(&filter, (const char **) values, 1);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(EXAPERF_FILTER_RAW == filter);

    strlcpy(values[0], "median", EXAPERF_MAX_TOKEN_LEN + 1);
    err = exaperf_filter_build(&filter, (const char **) values, 1);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(EXAPERF_FILTER_MEDIAN == filter);

    strlcpy(values[0], "stdev", EXAPERF_MAX_TOKEN_LEN + 1);
    err = exaperf_filter_build(&filter, (const char **) values, 1);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(EXAPERF_FILTER_STDEV == filter);
}

ut_test(build_all_in_one)
{
    uint32_t filter = 0;
    exaperf_err_t  err;
    char values_buf[EXAPERF_PARAM_NBMAX_VALUES * (EXAPERF_MAX_TOKEN_LEN + 1)];
    char * values[EXAPERF_PARAM_NBMAX_VALUES];
    int i;

    for (i=0; i<EXAPERF_PARAM_NBMAX_VALUES; i++)
	values[i] = &values_buf[i * (EXAPERF_MAX_TOKEN_LEN + 1)];

    strlcpy(values[0], "mean",   EXAPERF_MAX_TOKEN_LEN + 1);
    strlcpy(values[1], "min",    EXAPERF_MAX_TOKEN_LEN + 1);
    strlcpy(values[2], "max",    EXAPERF_MAX_TOKEN_LEN + 1);
    strlcpy(values[3], "raw",    EXAPERF_MAX_TOKEN_LEN + 1);
    strlcpy(values[4], "median", EXAPERF_MAX_TOKEN_LEN + 1);
    strlcpy(values[5], "stdev",  EXAPERF_MAX_TOKEN_LEN + 1);

    err = exaperf_filter_build(&filter, (const char **) values, 6);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(filter == (EXAPERF_FILTER_MEAN | EXAPERF_FILTER_MIN
                         | EXAPERF_FILTER_MAX | EXAPERF_FILTER_RAW
                         | EXAPERF_FILTER_MEDIAN | EXAPERF_FILTER_STDEV));
}

ut_test(build_none)
{
    uint32_t filter = 0;
    exaperf_err_t  err;
    char values_buf[EXAPERF_PARAM_NBMAX_VALUES * (EXAPERF_MAX_TOKEN_LEN + 1)];
    char * values[EXAPERF_PARAM_NBMAX_VALUES];
    int i;

    for (i=0; i<EXAPERF_PARAM_NBMAX_VALUES; i++)
	values[i] = &values_buf[i * (EXAPERF_MAX_TOKEN_LEN + 1)];

    strlcpy(values[0], "toto", EXAPERF_MAX_TOKEN_LEN + 1);
    err = exaperf_filter_build(&filter, (const char **) values, 1);

    UT_ASSERT(err == EXAPERF_INVALID_PARAM);
    UT_ASSERT(0x0 == filter);
}

