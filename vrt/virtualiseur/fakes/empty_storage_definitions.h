/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_STORAGE_FAKES_H__
#define __UT_VRT_STORAGE_FAKES_H__

#include <stdlib.h>

vrt_realdev_t *storage_get_rdev(const storage_t *storage, const exa_uuid_t *uuid)
{
    return NULL;
}

#endif
