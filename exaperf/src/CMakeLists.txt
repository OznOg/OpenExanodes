#
# Copyright 2002, 2008 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(exaperf STATIC
  exaperf_time.c
  exaperf_timeframe.c
  exaperf_config.c
  exaperf_filter.c
  exaperf_tools.c
  exaperf_stats.c
  exaperf_sample.c
  exaperf_sensor_template.c
  exaperf_sensor.c
  exaperf_counter.c
  exaperf_distribution.c
  exaperf_repart.c
  exaperf_duration.c
  exaperf_core.c
  exaperf.c)

if (NOT WIN32)
  set(LIBMATH m)
endif (NOT WIN32)

target_link_libraries(exaperf
    exa_common_user
    ${LIBMATH})
