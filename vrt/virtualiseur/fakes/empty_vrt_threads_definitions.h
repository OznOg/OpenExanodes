/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __FAKE_VRT_THREADS_H__
#define __FAKE_VRT_THREADS_H__

#include "vrt/virtualiseur/include/vrt_rebuild.h"
#include "vrt/virtualiseur/include/vrt_metadata.h"

int vrt_group_rebuild_thread_start(struct vrt_group *group) { return 0; }
int vrt_group_metadata_thread_start(struct vrt_group *group) { return 0; }

void vrt_group_metadata_thread_suspend(vrt_group_t *group) {}
void vrt_group_metadata_thread_resume(vrt_group_t *group) {}
void vrt_group_rebuild_thread_suspend(vrt_group_t *group) {}
void vrt_group_rebuild_thread_resume(vrt_group_t *group) {}

void vrt_group_metadata_thread_cleanup(struct vrt_group *group) {}
void vrt_group_rebuild_thread_cleanup(struct vrt_group *group) {}

void vrt_stats_restart(struct vrt_stats_volume *stats){}

#endif
