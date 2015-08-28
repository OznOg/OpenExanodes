/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <stdlib.h>

#include "os/include/os_inttypes.h"
#include "os/include/os_stdio.h"
#include "os/include/os_mem.h"

#include "common/include/exa_assert.h"

#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"

#include "exaperf/src/exaperf_core.h"
#include "exaperf/src/exaperf_sensor.h"
#include "exaperf/src/exaperf_time.h"
#include "exaperf/src/exaperf_distribution.h"


exaperf_err_t
exaperf_distribution_init(exaperf_distribution_t * distribution)
{
    EXA_ASSERT(distribution != NULL);

    distribution->nb_limits = 0;

    return EXAPERF_SUCCESS;
}


exaperf_err_t
exaperf_distribution_add_limit(exaperf_distribution_t * distribution, double new_limit)
{
    EXA_ASSERT(distribution != NULL);

    if (distribution->nb_limits >= EXAPERF_DISTRIBUTION_NBMAX_LIMITS)
	return EXAPERF_INVALID_PARAM;

    /* We ensure the limits are added in increasing order */
    if (distribution->nb_limits > 0 &&
	new_limit <= distribution->limits[distribution->nb_limits-1])
	return EXAPERF_INVALID_PARAM;

    distribution->limits[distribution->nb_limits] = new_limit;

    distribution->population[distribution->nb_limits] = 0;
    distribution->cumul_population[distribution->nb_limits] = 0;

    distribution->population[distribution->nb_limits+1] = 0;
    distribution->cumul_population[distribution->nb_limits+1] = 0;

    distribution->nb_limits++;

    return EXAPERF_SUCCESS;
}

void exaperf_distribution_compute(exaperf_distribution_t * distribution,
				  const exaperf_sample_t * sample)
{
    unsigned int v, i;
    const double *values;

    EXA_ASSERT(distribution != NULL);
    EXA_ASSERT(sample != NULL);

    for (i=0; i<distribution->nb_limits; i++)
	distribution->population[i] = 0;
    distribution->population[distribution->nb_limits] = 0;

    values = exaperf_sample_get_values(sample);

    for (v=0 ; v<exaperf_sample_get_nb_elem(sample) ; v++)
    {
	for (i=0 ; i<distribution->nb_limits  ; i++)
	{
	    if (values[v] <= distribution->limits[i])
	    {
		distribution->population[i]++;
		distribution->cumul_population[i]++;
		break;
	    }
	}
	if (i == distribution->nb_limits)
	{
	    distribution->population[i]++;
	    distribution->cumul_population[i]++;
	}
    }
}

unsigned int exaperf_distribution_get_nb_limits(const exaperf_distribution_t * distribution)
{
    EXA_ASSERT(distribution != NULL);

    return distribution->nb_limits;
}

double exaperf_distribution_get_limit(const exaperf_distribution_t * distribution, unsigned int i)
{
    EXA_ASSERT(distribution != NULL);
    EXA_ASSERT(i < distribution->nb_limits);

    return distribution->limits[i];
}

unsigned int exaperf_distribution_get_population(const exaperf_distribution_t * distribution, unsigned int i)
{
    EXA_ASSERT(distribution != NULL);
    EXA_ASSERT(i <= distribution->nb_limits);

    return distribution->population[i];
}

unsigned int exaperf_distribution_get_sum_population(const exaperf_distribution_t * distribution)
{
    unsigned int i;
    unsigned int sum = 0;

    EXA_ASSERT(distribution != NULL);

    for (i=0; i < distribution->nb_limits; i++)
	sum += distribution->population[i];

    return sum;
}

unsigned int exaperf_distribution_get_cumul_population(const exaperf_distribution_t * distribution, unsigned int i)
{
    EXA_ASSERT(distribution != NULL);
    EXA_ASSERT(i <= distribution->nb_limits);

    return distribution->cumul_population[i];
}

unsigned int exaperf_distribution_get_sum_cumul_population(const exaperf_distribution_t * distribution)
{
    unsigned int i;
    unsigned int sum = 0;

    EXA_ASSERT(distribution != NULL);

    for (i=0; i < distribution->nb_limits; i++)
	sum += distribution->cumul_population[i];

    return sum;
}

