/*
 * Copyright 2002, 2011 Seanodes IT http://www.seanodes.com. All rights
 * reserved and protected by French, U.S. and other countries' copyright laws.
 */

#ifndef __EXA_PERF_INSTANCE
#define __EXA_PERF_INSTANCE

#if WITH_PERF

#include "exaperf/include/exaperf.h"

exaperf_t *exa_perf_instance_get(void);

int exa_perf_instance_static_init(void);
void exa_perf_instance_static_clean(void);

#else

#define exa_perf_instance_get() NULL

#define exa_perf_instance_static_init() 0
#define exa_perf_instance_static_clean()

#endif

#endif
