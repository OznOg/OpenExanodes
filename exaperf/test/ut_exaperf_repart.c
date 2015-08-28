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

#define NB_REPART 6
static double limits[NB_REPART] = {-4.1, -2.1, 0.1, 2.1, 4.1, 5.1};

UT_SECTION(repartition)
exaperf_sensor_t *repart = NULL;

exaperf_t *eh = NULL;
exaperf_err_t ret;

ut_test(repartition_not_in_config)
{
    char *srcdir = getenv("srcdir");
    char path[512];

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_repartition.exaperf", srcdir ? srcdir : ".");

    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    repart = exaperf_repart_init(eh, "REPART_NOT_IN_CONFIG", NB_REPART, limits);
    UT_ASSERT(repart == NULL);

    exaperf_free(eh);
}

ut_test(repartition_simple)
{
    char *srcdir = getenv("srcdir");
    char path[512];
    double i;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_repartition.exaperf", srcdir ? srcdir : ".");

    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    repart = exaperf_repart_init(eh, "REPART1", NB_REPART, limits);
    UT_ASSERT(repart != NULL);

    for (i = -5 ; i < 7; i++)
    {
	os_millisleep(750);
	exaperf_repart_add_value(repart, i);
    }

    exaperf_free(eh);
}


ut_test(repartition_template)
{
    char *srcdir = getenv("srcdir");
    char path[512];
    double i;
    exaperf_sensor_t *test[12];
    char name[EXAPERF_MAX_TOKEN_LEN + 1];

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_repartition.exaperf", srcdir ? srcdir : ".");

    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    for (i = 0 ; i < 12 ; i++)
    {
	sprintf(name, "TEST_%g", i+1);
	test[(int)i] = exaperf_repart_init_from_template(eh, "REPART1", name, NB_REPART, limits);
	UT_ASSERT(test[(int)i] != NULL);
    }

    for (i = -5 ; i < 7; i++)
    {
	exaperf_repart_add_value(test[(int)i+5], i);
	os_millisleep(750);
    }

    exaperf_free(eh);
}
