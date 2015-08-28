/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"

void print_fct(const char *fmt, ...)
{
    return;
}


void test_init(void)
{
    exaperf_t *eh_client;
    exaperf_err_t err;
    char *srcdir = getenv("srcdir");
    char path[512];

    eh_client = exaperf_alloc();
    assert(eh_client != NULL);

    sprintf(path, "%s/configfiles/config1.exaperf", srcdir ? srcdir : ".");
    err = exaperf_init(eh_client, path, print_fct);

    assert(err == EXAPERF_SUCCESS);
}
