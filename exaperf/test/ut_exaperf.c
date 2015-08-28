/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include <stdlib.h>
#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"

UT_SECTION(exaperf_alloc_init_free)

exaperf_t *eh = NULL;
exaperf_err_t ret;

UT_SECTION(exaperf_init)

static void __print(const char *fmt, ...)
{
}

ut_test(invalid_param_returns_INVALID_PARAM)
{
    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    UT_ASSERT_EQUAL(EXAPERF_INVALID_PARAM, exaperf_init(NULL, "dummy", __print));
    UT_ASSERT_EQUAL(EXAPERF_INVALID_PARAM, exaperf_init(eh, NULL, __print));
    UT_ASSERT_EQUAL(EXAPERF_INVALID_PARAM, exaperf_init(eh, "dummy", NULL));
}

ut_test(normal)
{
    char *srcdir = getenv("srcdir");
    char path[512];

    eh = exaperf_alloc();
    UT_ASSERT(eh != NULL);

    sprintf(path, "%s/configfiles/config1.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    exaperf_free(eh);
}
