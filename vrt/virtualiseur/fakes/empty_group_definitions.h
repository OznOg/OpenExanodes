/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_GROUP_FAKES_H__
#define __UT_VRT_GROUP_FAKES_H__

#include "vrt/virtualiseur/include/vrt_group.h"

/* FIXME - FUGLY FAKE */
struct vrt_volume *vrt_group_find_volume(const struct vrt_group *group,
                                         const exa_uuid_t *uuid)
{
    return NULL;
}

void vrt_group_unref(struct vrt_group *group)
{
}

#endif
