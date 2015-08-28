/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "os/include/os_random.h"

#include <unit_testing.h>

#define NB_THROWS      65536
#define NB_FREQUENCIES 16
#define MAX_DEVIATION  0.1


ut_test(os_random_init)
{
    os_random_init();
    UT_ASSERT(os_random_is_initialized());
    os_random_cleanup();
}

ut_test(os_random_cleanup)
{
    os_random_init();
    os_random_cleanup();
    UT_ASSERT(! os_random_is_initialized());
}

ut_test(os_drand_dispersion)
{
    int frequencies[NB_FREQUENCIES];
    int i;

    memset(frequencies, 0, sizeof(frequencies));
    os_random_init();

    for (i =0; i < NB_THROWS; i++)
    {
        double n;
        n = os_drand();
        frequencies[(int)(n * NB_FREQUENCIES)]++;
    }

    for (i =0; i < NB_FREQUENCIES; i++)
    {
        UT_ASSERT(frequencies[i] > NB_THROWS / NB_FREQUENCIES * (1 - MAX_DEVIATION));
        UT_ASSERT(frequencies[i] < NB_THROWS / NB_FREQUENCIES * (1 + MAX_DEVIATION));
    }

    os_random_cleanup();
}


ut_test(os_get_random_bytes_dispersion)
{
    int frequencies[NB_FREQUENCIES];
    int i;

    memset(frequencies, 0, sizeof(frequencies));
    os_random_init();

    for (i =0; i < NB_THROWS; i++)
    {
        uint64_t n;
        os_get_random_bytes(&n, sizeof(n));
        frequencies[n % NB_FREQUENCIES]++;
    }

    for (i =0; i < NB_FREQUENCIES; i++)
    {
        UT_ASSERT(frequencies[i] > NB_THROWS / NB_FREQUENCIES * (1 - MAX_DEVIATION));
        UT_ASSERT(frequencies[i] < NB_THROWS / NB_FREQUENCIES * (1 + MAX_DEVIATION));
    }

    os_random_cleanup();
}
