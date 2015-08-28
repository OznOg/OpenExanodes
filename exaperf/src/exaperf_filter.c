/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_inttypes.h"
#include "os/include/os_string.h"
#include "os/include/os_stdio.h"

#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_filter.h"

struct filter
{
    char str[EXAPERF_MAX_TOKEN_LEN + 1];
    exaperf_filter_t filter_id;
};

static struct filter filters[] =
{
    {"mean", 	EXAPERF_FILTER_MEAN},
    {"min",  	EXAPERF_FILTER_MIN},
    {"max",  	EXAPERF_FILTER_MAX},
    {"raw",  	EXAPERF_FILTER_RAW},
    {"median",  EXAPERF_FILTER_MEDIAN},
    {"stdev", 	EXAPERF_FILTER_STDEV},
    {"detail", 	EXAPERF_FILTER_DETAIL},
    {"none", 	EXAPERF_FILTER_NONE}
};

static bool is_valid_filter(const char *filter,
			    exaperf_filter_t *filter_id)
{
    int i = 0;

    do
    {
	if (strncmp(filter, filters[i].str, EXAPERF_MAX_TOKEN_LEN + 1) == 0)
	{
	    *filter_id = filters[i].filter_id;
	    return true;
	}
	i++;
    } while (filters[i].filter_id != EXAPERF_FILTER_NONE);

    return false;
}


exaperf_err_t
exaperf_filter_build(uint32_t * filter, const char ** values, uint32_t nb_values)
{
    int i = 0;
    exaperf_filter_t filter_tmp = EXAPERF_FILTER_NONE;

    *filter = EXAPERF_FILTER_NONE;

    for (i = 0; i < nb_values; i++)
    {
	if (is_valid_filter(values[i], &filter_tmp))
	{
	    exaperf_filter_add(filter, filter_tmp);
	}
	else
	    return EXAPERF_INVALID_PARAM;
    }

    return EXAPERF_SUCCESS;
}

bool
exaperf_filter_contains(uint32_t filter, exaperf_filter_t value)
{
    return ((filter & value) != 0);
}

bool
exaperf_filter_empty(uint32_t filter)
{
    return (filter == EXAPERF_FILTER_NONE);
}

void
exaperf_filter_add(uint32_t *filter, exaperf_filter_t value)
{
    *filter |= value;
}

void
exaperf_filter_remove(uint32_t *filter, exaperf_filter_t value)
{
    *filter &= ~value;
}
