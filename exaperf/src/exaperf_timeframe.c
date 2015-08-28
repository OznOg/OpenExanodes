/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_inttypes.h"

#include "exaperf/src/exaperf_time.h"
#include "exaperf/src/exaperf_timeframe.h"

void
exaperf_timeframe_init(exaperf_timeframe_t *tf, double duration)
{
    tf->duration = duration;
}

void
exaperf_timeframe_reset(exaperf_timeframe_t *tf)
{
    tf->start_time = exaperf_gettime();
}

bool
exaperf_timeframe_is_finished(exaperf_timeframe_t *tf)
{
    double current_time = exaperf_gettime();
    double finish_time = tf->start_time + tf->duration;

    if (current_time > finish_time)
	return true;
    else
	return false;
}

double
exaperf_timeframe_get_duration(exaperf_timeframe_t *tf)
{
    return tf->duration;
}
