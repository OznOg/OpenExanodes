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


UT_SECTION(counter)
exaperf_sensor_t *counter_1 = NULL;
exaperf_sensor_t *counter_2 = NULL;

exaperf_t *eh = NULL;
exaperf_err_t ret;

ut_test(counter_base)
{
    char *srcdir = getenv("srcdir");
    char path[512];
    int i;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config1.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);
    counter_1 = exaperf_counter_init(eh, "SENSOR1");
    UT_ASSERT(counter_1 != NULL);

    exaperf_counter_set(counter_1, 0);
    for (i = 0; i < 5000; i++)
    {
	exaperf_counter_inc(counter_1);
	exaperf_counter_dec(counter_1);
	exaperf_counter_add(counter_1, 10);
	os_millisleep(1);
    }

    exaperf_free(eh);
}

ut_test(counter_not_in_config)
{
    char *srcdir = getenv("srcdir");
    char path[512];

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config1.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);
    counter_1 = exaperf_counter_init(eh, "SENSOR_NOT_IN_CONFIG");
    UT_ASSERT(counter_1 == NULL);

    exaperf_free(eh);
}

ut_test(counter_name_size)
{
    char *srcdir = getenv("srcdir");
    char path[512];

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_counter_name_size.exaperf",
	    srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);
    counter_1 = exaperf_counter_init(eh, "SENSOR_VERY_LONG_NAME");
    UT_ASSERT(counter_1 != NULL);
    counter_2 = exaperf_counter_init(eh, "SENSOR_SHORT");
    UT_ASSERT(counter_2 != NULL);

    exaperf_free(eh);
}

ut_test(lots_of_counters)
{
    char *srcdir = getenv("srcdir");
    char path[512];
    char name[EXAPERF_MAX_TOKEN_LEN + 1];
    exaperf_sensor_t *test[10];
    int i;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_lots_of_counters.exaperf",
	    srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    for (i = 0; i < 10; i++)
    {
	sprintf(name, "TEST_%d", i+1);
	test[i] = exaperf_counter_init(eh, name);
	UT_ASSERT(test[i] != NULL);
	exaperf_counter_set(test[i], 0);
    }

    exaperf_free(eh);
}

/*************************************************************/
UT_SECTION(counter_template)
ut_test(template)
{
    char *srcdir = getenv("srcdir");
    char path[512];
    char name[EXAPERF_MAX_TOKEN_LEN + 1];
    exaperf_sensor_t *test[10];
    int i;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_template.exaperf",
	    srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    for (i = 0; i < 10; i++)
    {
	sprintf(name, "TEST_%d", i+1);
	test[i] = exaperf_counter_init_from_template(eh, "TEMPLATE", name);
	UT_ASSERT(test[i] != NULL);
    }

    for (i = 0; i < 10; i++)
    {
	exaperf_counter_set(test[i], i);
	os_sleep(1);
    }

    exaperf_free(eh);
}
