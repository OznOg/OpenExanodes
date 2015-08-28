/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXAPERF_FILTER
#define EXAPERF_FILTER

#include "os/include/os_inttypes.h"

#include "exaperf/include/exaperf.h"

/* TODO Add function returning filter name ("mean", "max", etc) from
        symbolic constant */

typedef enum exaperf_filter
{
    EXAPERF_FILTER_NONE 	= 0x00,
    EXAPERF_FILTER_MEAN 	= 0x01,
    EXAPERF_FILTER_MIN 		= 0x02,
    EXAPERF_FILTER_MAX 		= 0x04,
    EXAPERF_FILTER_RAW 		= 0x08,
    EXAPERF_FILTER_MEDIAN 	= 0x10,
    EXAPERF_FILTER_STDEV 	= 0x20,
    EXAPERF_FILTER_DETAIL 	= 0x40
} exaperf_filter_t;

exaperf_err_t exaperf_filter_build(uint32_t * filter, const char ** values, uint32_t nb_values);
bool exaperf_filter_contains(uint32_t filter, exaperf_filter_t value);
bool exaperf_filter_empty(uint32_t filter);
void exaperf_filter_add(uint32_t *filter, exaperf_filter_t value);
void exaperf_filter_remove(uint32_t *filter, exaperf_filter_t value);
#endif /* EXAPERF_FILTER */
