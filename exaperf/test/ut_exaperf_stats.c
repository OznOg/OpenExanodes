/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>

#include "exaperf/src/exaperf_stats.h"

/**************************************************************/
UT_SECTION(exaperf_stats_basic_stat)

exaperf_basic_stat_t bs;

ut_test(normal)
{
    double values[] = {1.0, 2.0, 3.0, 4.0, 5.0};

    bs = exaperf_compute_basic_stat(values, 5);

    UT_ASSERT(bs.min == 1.0);
    UT_ASSERT(bs.max == 5.0);
    UT_ASSERT(bs.mean == 3.0);
    UT_ASSERT(bs.median == 3.0);
    /* FIXME: Test the standard deviation here */
}
