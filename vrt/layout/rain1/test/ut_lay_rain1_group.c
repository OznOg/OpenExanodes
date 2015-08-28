/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/layout/rain1/src/lay_rain1_group.h"

#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"
#include "vrt/virtualiseur/fakes/empty_request_definitions.h"

ut_test(alloc_free_rain1_group)
{
    rain1_group_t *rxg;

    rxg = rain1_group_alloc();
    UT_ASSERT(rxg != NULL);

    rain1_group_free(rxg, NULL /* FIXME */);
    UT_ASSERT(rxg == NULL);
}
