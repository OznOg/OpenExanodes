/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef EXAPERF_TIMEFRAME_H
#define EXAPERF_TIMEFRAME_H

#include "os/include/os_inttypes.h"

typedef struct exaperf_timeframe
{
    double start_time;
    double duration;
} exaperf_timeframe_t;

void exaperf_timeframe_init(exaperf_timeframe_t *tf, double duration);
void exaperf_timeframe_reset(exaperf_timeframe_t *tf);
bool exaperf_timeframe_is_finished(exaperf_timeframe_t *tf);
double exaperf_timeframe_get_duration(exaperf_timeframe_t *tf);

#endif /* EXAPERF_TIMEFRAME_H */
