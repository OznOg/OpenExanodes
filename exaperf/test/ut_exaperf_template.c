/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include <stdlib.h>

#include "os/include/os_time.h"

#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"


UT_SECTION(template)
exaperf_sensor_t *counter_1 = NULL;
exaperf_sensor_t *counter_2 = NULL;

exaperf_t *eh = NULL;
exaperf_err_t ret;

ut_test(counter_same_name)
{
    char *srcdir = getenv("srcdir");
    char path[512];
    int i;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/template.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);
    counter_1 = exaperf_counter_init_from_template(eh, "SENSOR1", "counter");
    UT_ASSERT(counter_1 != NULL);
    counter_2 = exaperf_counter_init_from_template(eh, "SENSOR2", "counter");
    UT_ASSERT(counter_2 != NULL);

    exaperf_counter_set(counter_1, 0);
    exaperf_counter_set(counter_2, 5000);
    for (i = 0; i < 5000; i++)
    {
	exaperf_counter_inc(counter_1);
	exaperf_counter_dec(counter_2);
	os_millisleep(1);
    }

    exaperf_free(eh);
}
