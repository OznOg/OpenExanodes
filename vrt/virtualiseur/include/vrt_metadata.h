/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __VRT_METADATA_H__
#define __VRT_METADATA_H__


#include "vrt/virtualiseur/include/vrt_group.h"

int vrt_group_metadata_thread_start(struct vrt_group *group);
void vrt_group_metadata_thread_cleanup(struct vrt_group *group);
void vrt_group_metadata_thread_resume(vrt_group_t *group);
void vrt_group_metadata_thread_suspend(vrt_group_t *group);

#endif /* __VRT_METADATA_H__ */
