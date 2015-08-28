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

ut_test(duration_sequential) __ut_lengthy
{
    char *srcdir = getenv("srcdir");
    char path[512];
    unsigned int i, j;

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config_duration.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    duration = exaperf_duration_init(eh, "DUR_SEQ", false);
    UT_ASSERT(duration != NULL);

    for (i = 0; i < 10; i++)
    {
        for (j = 0; j < 500; j++)
        {
            exaperf_duration_begin(duration);
            os_millisleep(2);
            exaperf_duration_end(duration);
        }

        exaperf_sensor_flush(duration);
    }

    exaperf_free(eh);
}
