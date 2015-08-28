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

void test_init(void);


int main(void)
{
    exaperf_t *eh_client = NULL;
    exaperf_sensor_t *counter = NULL;

    test_init();

    assert(eh_client != NULL);

    counter = exaperf_counter_init(eh_client, "SENSOR1");
    assert(counter != NULL);

    exaperf_free(eh_client);

    return 0;
}
