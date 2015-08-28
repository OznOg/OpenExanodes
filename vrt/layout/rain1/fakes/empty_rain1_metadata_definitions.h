/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_RAIN1_METADATA_FAKES_H__
#define __UT_RAIN1_METADATA_FAKES_H__

#include "vrt/layout/rain1/src/lay_rain1_request.h"

int rain1_wipe_slot_metadata(struct rain1_group *rxg, const slot_t *slot)
{
    return 0;
}
void dzone_metadata_block_init(desync_info_t *metadatas,
                               sync_tag_t uptodate_tag)
{
}

int rain1_write_slot_metadata(struct rain1_group *rxg,
                              const slot_t *slot,
                              unsigned int node_index,
                              const desync_info_t *metadatas)
{
    return 0;
}

int rain1_read_slot_metadata (struct rain1_group *rxg,
                              const slot_t *slot,
                              unsigned int node_id,
                              desync_info_t *metadatas)
{
    return 0;
}
int rain1_group_metadata_flush_step(void *private_data, struct vrt_group *group)
{
    return 0;
}

#endif
