/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_SYNC_H__
#define __RAIN1_SYNC_H__


#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/virtualiseur/include/vrt_group.h"

/** Numbers of sectors between two checkpoints of the replicating or
    updating process */
#define RAIN1_PARTIAL_REINTEGRATE_SECTORS_INTERVAL_REPLICATE (2 << 21)
#define RAIN1_PARTIAL_REINTEGRATE_SECTORS_INTERVAL_UPDATE    (3 << 21)

void rain1_set_rebuilding_slowdown(int _rebuilding_slowdown_ms,
                                   int _degraded_rebuilding_slowdown_ms);

int rain1_group_resync(struct vrt_group *group, const exa_nodeset_t *nodes);

int rain1_group_rebuild_step(void *context, bool *more_work);
void *rain1_group_rebuild_context_alloc(struct vrt_group *group);
void rain1_group_rebuild_context_free(void *context);
void rain1_group_rebuild_context_reset(void *context);

#endif
