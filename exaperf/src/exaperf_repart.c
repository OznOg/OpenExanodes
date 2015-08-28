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
#include "exaperf/src/exaperf_repart.h"
#include "exaperf/src/exaperf_distribution.h"

exaperf_sensor_t *exaperf_repart_init(exaperf_t *eh,
				      const char *name,
				      unsigned int size,
				      const double *limits)
{
    return exaperf_repart_init_from_template(eh, name,
					     name, size, limits);
}

exaperf_sensor_t *exaperf_repart_init_from_template(exaperf_t *eh,
						    const char *template_name,
						    const char *name,
						    unsigned int size,
						    const double *limits)
{
    int i;
    exaperf_sensor_t *sensor = exaperf_sensor_retrieve(eh, template_name, name);

    if (sensor == NULL)
	return NULL;

    exaperf_sensor_init(sensor, EXAPERF_SENSOR_REPART);

    for (i=0 ; i<size  ; i++)
	exaperf_distribution_add_limit(&sensor->distribution, limits[i]);

    return sensor;
}

void exaperf_repart_add_value(exaperf_sensor_t *sensor, double value)
{
    if (sensor == NULL)
	return;

    EXA_ASSERT(sensor->kind == EXAPERF_SENSOR_REPART);

    exaperf_sensor_lock(sensor);
    exaperf_sensor_set(sensor, value);
    exaperf_sensor_unlock(sensor);
}
