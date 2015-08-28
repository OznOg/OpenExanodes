/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_MODULE_FAKES_H__
#define __UT_VRT_MODULE_FAKES_H__

#include "common/include/uuid.h"

/* FIXME - FUGLY FAKE */
struct vrt_group *vrt_get_group_from_uuid(const exa_uuid_t *uuid)
{
    return NULL;
}

#endif
