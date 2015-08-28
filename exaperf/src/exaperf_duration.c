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
#include "os/include/os_inttypes.h"

#include "common/include/exa_assert.h"
#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"

#include "exaperf/src/exaperf_core.h"
#include "exaperf/src/exaperf_sensor.h"
#include "exaperf/src/exaperf_time.h"
#include "exaperf/src/exaperf_duration.h"

static void reset_duration(exaperf_duration_t *duration)
{
    duration->begin_count = 0;
    duration->end_count = 0;
    duration->begin_time_cumul = 0.;
    duration->end_time_cumul = 0.;
}

exaperf_sensor_t *
exaperf_duration_init(exaperf_t *eh,
		      const char *name,
		      bool interleaved)
{
    return exaperf_duration_init_from_template(eh, name,
					       name, interleaved);
}


exaperf_sensor_t *
exaperf_duration_init_from_template(exaperf_t *eh,
				    const char *template_name,
				    const char *name,
				    bool interleaved)
{
    exaperf_sensor_t *sensor = exaperf_sensor_retrieve(eh, template_name, name);
    exaperf_duration_t *duration = NULL;

    if (sensor == NULL)
	return NULL;

    exaperf_sensor_init(sensor, EXAPERF_SENSOR_DURATION);
    duration = exaperf_sensor_duration(sensor);

    duration->interleaved = interleaved;
    reset_duration(duration);

    return sensor;
}


void
exaperf_duration_begin(exaperf_sensor_t * sensor)
{
    double current_time;
    exaperf_duration_t *duration = NULL;

    if (sensor == NULL)
	return;

    EXA_ASSERT(sensor->kind == EXAPERF_SENSOR_DURATION);
    duration = exaperf_sensor_duration(sensor);

    current_time = exaperf_gettime();

    if (duration->interleaved)
    {
	EXA_ASSERT(duration->end_count <= duration->begin_count);

	duration->begin_time_cumul += current_time;
	duration->begin_count++;
    }
    else
    {
	EXA_ASSERT(duration->begin_count == 0);

	duration->begin_time_cumul = current_time;
	duration->begin_count = 1;
    }
}


void
exaperf_duration_end(exaperf_sensor_t * sensor)
{
    double current_time;
    exaperf_duration_t *duration = NULL;

    if (sensor == NULL)
	return;

    EXA_ASSERT(sensor->kind == EXAPERF_SENSOR_DURATION);
    duration = exaperf_sensor_duration(sensor);

    current_time = exaperf_gettime();

    exaperf_sensor_lock(sensor);
    if (duration->interleaved)
    {
	EXA_ASSERT(duration->end_count < duration->begin_count);

	duration->end_time_cumul += current_time;
	duration->end_count++;

	/* record the values only when the number of ends match the number of begins */
	if (duration->end_count == 1 && duration->begin_count == 1)
	{
	    exaperf_sensor_set(sensor, duration->end_time_cumul - duration->begin_time_cumul);
	    reset_duration(duration);
	}
	else if (duration->end_count == duration->begin_count)
	{
	    exaperf_sensor_set_multiple(sensor,
					duration->end_time_cumul - duration->begin_time_cumul,
					duration->end_count);
	    reset_duration(duration);
	}
    }
    else
    {
	EXA_ASSERT(duration->begin_count == 1);

	exaperf_sensor_set(sensor, current_time - duration->begin_time_cumul);
	reset_duration(duration);

    }
    exaperf_sensor_unlock(sensor);
}

void
exaperf_duration_record(exaperf_sensor_t * sensor, double value)
{
    if (sensor == NULL)
	return;

    EXA_ASSERT(sensor->kind == EXAPERF_SENSOR_DURATION);

    exaperf_sensor_lock(sensor);
    exaperf_sensor_set(sensor, value);
    exaperf_sensor_unlock(sensor);
}
