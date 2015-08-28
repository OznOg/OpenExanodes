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
#include "exaperf/src/exaperf_time.h"

/*
 * !!! NOTE !!!
 *
 * THIS UNIT TEST IS POINTLESS ON WINDOWS, AS WINDOWS SEEMINGLY
 * IS INCAPABLE OF MILLISECOND PRECISION!
 *
 * THE TEST CASES MARKED "LENGTHY" ARE RIDICULOUSLY LONG.
 */

UT_SECTION(duration)
exaperf_sensor_t *duration = NULL;

exaperf_t *eh = NULL;
exaperf_err_t ret;

ut_test(duration_not_in_config)
{
    char *srcdir = getenv("srcdir");
    char path[512];

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_duration.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    duration = exaperf_duration_init(eh, "DUR_NOT_IN_CONFIG", false);
    UT_ASSERT(duration == NULL);

    exaperf_free(eh);
}

ut_test(duration_sequential) __ut_lengthy
{
    char *srcdir = getenv("srcdir");
    char path[512];
    int i;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_duration.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    duration = exaperf_duration_init(eh, "DUR_SEQ", false);
    UT_ASSERT(duration != NULL);

    for (i = 0; i < 5000; i++)
    {
	exaperf_duration_begin(duration);
	os_millisleep(2);
	exaperf_duration_end(duration);
    }

    exaperf_free(eh);
}

ut_test(duration_interleaved_simple) __ut_lengthy
{
    char *srcdir = getenv("srcdir");
    char path[512];
    int i;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_duration.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    duration = exaperf_duration_init(eh, "DUR_PAR", true);
    UT_ASSERT(duration != NULL);

    for (i = 0; i < 5000; i++)
    {
	exaperf_duration_begin(duration);
	os_millisleep(1);
	exaperf_duration_begin(duration);
	os_millisleep(1);
	exaperf_duration_end(duration);
	os_millisleep(1);
	exaperf_duration_end(duration);
    }

    exaperf_free(eh);
}


ut_test(duration_interleaved_complex) __ut_lengthy
{
    char *srcdir = getenv("srcdir");
    char path[512];
    int nb_events = 0, nb_records = 0;
    int i;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_duration.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    duration = exaperf_duration_init(eh, "DUR_PAR", true);
    UT_ASSERT(duration != NULL);

    for (i = 0; i < 100000; i++)
    {
	/* randomly choose between begin and end of event but force the number of
	 * events between 0 and 5 */
	if (nb_events == 0 || (nb_events < 5 && rand() > RAND_MAX/2))
	{
	    exaperf_duration_begin(duration);
	    nb_events ++;
	}
	else
	{
	    exaperf_duration_end(duration);
	    nb_events --;
	}

	os_millisleep(1);

	if (nb_events == 0)
	    nb_records ++;
    }

    UT_ASSERT(nb_records > 0);

    exaperf_free(eh);
}

ut_test(duration_record) __ut_lengthy
{
    char *srcdir = getenv("srcdir");
    char path[512];
    int i;
    double begin = 0, end = 0;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_duration.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    duration = exaperf_duration_init(eh, "DUR_PAR", true);
    UT_ASSERT(duration != NULL);

    for (i = 0; i < 5000; i++)
    {
	begin = exaperf_gettime();
	os_millisleep(2);
	end = exaperf_gettime();
	exaperf_duration_record(duration, end-begin);
    }

    exaperf_free(eh);
}
