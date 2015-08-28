/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>

#include "exaperf/src/exaperf_sample.h"

/**************************************************************/
UT_SECTION(normal)

exaperf_err_t err;
exaperf_sample_t sample;

ut_test(alloc_empty)
{
    err = exaperf_sample_init(&sample, 10);
    UT_ASSERT(err == EXAPERF_SUCCESS);

    UT_ASSERT(exaperf_sample_is_empty(&sample));

    exaperf_sample_clear(&sample);
}

ut_test(full)
{
    err = exaperf_sample_init(&sample, 3);
    UT_ASSERT(err == EXAPERF_SUCCESS);

    UT_ASSERT(exaperf_sample_is_empty(&sample));
    UT_ASSERT(!exaperf_sample_is_full(&sample));

    exaperf_sample_add(&sample, 20.0);
    UT_ASSERT(!exaperf_sample_is_full(&sample));

    exaperf_sample_add(&sample, 20.0);
    UT_ASSERT(!exaperf_sample_is_full(&sample));

    exaperf_sample_add(&sample, 20.0);
    UT_ASSERT(exaperf_sample_is_full(&sample));

    exaperf_sample_clear(&sample);
}

ut_test(add_reset_size)
{
    err = exaperf_sample_init(&sample, 3);
    UT_ASSERT(err == EXAPERF_SUCCESS);

    exaperf_sample_add(&sample, 10.0);
    UT_ASSERT(exaperf_sample_get_nb_elem(&sample) == 1);
    exaperf_sample_add(&sample, 20.0);
    UT_ASSERT(exaperf_sample_get_nb_elem(&sample) == 2);
    exaperf_sample_add(&sample, 30.0);
    UT_ASSERT(exaperf_sample_get_nb_elem(&sample) == 3);

    exaperf_sample_reset(&sample);
    UT_ASSERT(exaperf_sample_get_nb_elem(&sample) == 0);

    exaperf_sample_clear(&sample);
}

ut_test(full_lost)
{
    err = exaperf_sample_init(&sample, 3);
    UT_ASSERT(err == EXAPERF_SUCCESS);

    exaperf_sample_add(&sample, 10.0);
    exaperf_sample_add(&sample, 10.0);
    exaperf_sample_add(&sample, 10.0);
    UT_ASSERT(exaperf_sample_is_full(&sample));

    exaperf_sample_add(&sample, 10.0);
    exaperf_sample_add(&sample, 10.0);
    UT_ASSERT(exaperf_sample_is_full(&sample));
    UT_ASSERT(exaperf_sample_get_nb_elem(&sample) == 3);
    UT_ASSERT(exaperf_sample_get_nb_lost(&sample) == 2);
    exaperf_sample_clear(&sample);
}

ut_test(get_values)
{
    const double *values;
    uint32_t nb;

    err = exaperf_sample_init(&sample, 3);
    UT_ASSERT(err == EXAPERF_SUCCESS);

    exaperf_sample_add(&sample, 10.0);
    UT_ASSERT(exaperf_sample_get_nb_elem(&sample) == 1);
    exaperf_sample_add(&sample, 20.0);
    UT_ASSERT(exaperf_sample_get_nb_elem(&sample) == 2);
    exaperf_sample_add(&sample, 30.0);
    UT_ASSERT(exaperf_sample_get_nb_elem(&sample) == 3);

    nb = exaperf_sample_get_nb_elem(&sample);
    UT_ASSERT(nb == 3);

    values = exaperf_sample_get_values(&sample);
    UT_ASSERT(values[0] == 10.0);
    UT_ASSERT(values[1] == 20.0);
    UT_ASSERT(values[2] == 30.0);
}
