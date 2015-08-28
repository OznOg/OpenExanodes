/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>

#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_inttypes.h"

#include "common/include/exa_assert.h"
#include "exaperf/src/exaperf_sample.h"


exaperf_err_t
exaperf_sample_init(exaperf_sample_t *sample, uint32_t sample_size)
{

    if (sample_size != 0)
    {
	sample->values = os_malloc(sizeof(double) * sample_size);
	if (sample->values == NULL)
	{
	    return EXAPERF_MALLOC_FAILED;
	}
    }
    else
    {
	sample->values = NULL;
    }

    sample->size = sample_size;
    exaperf_sample_reset(sample);

    return EXAPERF_SUCCESS;
}

void
exaperf_sample_clear(exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);

    if (sample->values != NULL)
    {
	os_free(sample->values);
	sample->values = NULL;
    }
}

void
exaperf_sample_reset(exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);

    sample->nb_lost = 0;
    sample->nb_elem = 0;
    sample->nb_total = 0;

    sample->sum = 0;
    sample->min = 0;
    sample->max = 0;
}

bool
exaperf_sample_is_empty(const exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);

    if (sample->nb_elem == 0)
	return true;
    else
	return false;
}

bool
exaperf_sample_is_full(const exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);

    if (sample->nb_elem == sample->size)
	return true;
    else
	return false;
}

void
exaperf_sample_add(exaperf_sample_t *sample, double value)
{
    EXA_ASSERT(sample != NULL);

    if (exaperf_sample_is_full(sample))
	sample->nb_lost++;
    else
    {
	EXA_ASSERT(sample->values != NULL);
	sample->nb_elem++;
	sample->values[sample->nb_elem - 1] = value;
    }

    if (sample->nb_total == 0)
    {
	sample->sum = value;
	sample->min = value;
	sample->max = value;
    }
    else
    {
	sample->sum += value;
	if (sample->min > value)
	    sample->min = value;
	else if (sample->max < value)
	    sample->max = value;
    }

    sample->nb_total++;
}

void
exaperf_sample_add_multiple(exaperf_sample_t *sample, double sum_values, unsigned int nb_values)
{
    double mean_values;

    EXA_ASSERT(sample != NULL);

    mean_values = sum_values / (double) nb_values;

    if (sample->nb_total == 0)
    {
	sample->sum = sum_values;
	sample->min = mean_values;
	sample->max = mean_values;
    }
    else
    {
	sample->sum += sum_values;
	if (sample->min > mean_values)
	    sample->min = mean_values;
	else if (sample->max < mean_values)
	    sample->max = mean_values;
    }

    sample->nb_lost += nb_values;
    sample->nb_total += nb_values;
}

uint32_t
exaperf_sample_get_nb_elem(const exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);

    return sample->nb_elem;
}

uint32_t exaperf_sample_get_nb_lost(const exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);

    return sample->nb_lost;
}

double exaperf_sample_get_mean(const exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);

    return sample->sum/(double)sample->nb_total;
}

double exaperf_sample_get_min(const exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);

    return sample->min;
}

double exaperf_sample_get_max(const exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);

    return sample->max;
}

const double * exaperf_sample_get_values(const exaperf_sample_t *sample)
{
    EXA_ASSERT(sample != NULL);
    EXA_ASSERT(sample->values != NULL);

    return sample->values;
}
