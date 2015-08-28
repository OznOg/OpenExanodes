/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <stdlib.h>
#include "os/include/os_time.h"

#include "exaperf/src/exaperf_time.h"

static double exaperf_start_time;


double
exaperf_gettime(void)
{
  struct timeval tv;
  double result;
  /* TODO: use a monotonic clock time of os/ instead of gettimeofday */
  os_gettimeofday(&tv);
  result = tv.tv_sec * 1.0 + tv.tv_usec / 1000000.0;
  return result;
}

void
exaperf_time_init (void)
{
  exaperf_start_time = exaperf_gettime();
}
