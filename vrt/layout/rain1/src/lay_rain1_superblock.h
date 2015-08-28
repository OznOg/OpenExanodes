/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __LAY_RAIN1_SUPERBLOCK_H__
#define __LAY_RAIN1_SUPERBLOCK_H__

#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_sync_tag.h"

#include "vrt/virtualiseur/include/storage.h"

#include "vrt/virtualiseur/include/vrt_group.h"

#include "vrt/common/include/spof.h"
#include "vrt/common/include/vrt_stream.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"

#include "os/include/os_inttypes.h"

typedef struct
 {
    exa_uuid_t uuid;
    sync_tag_t sync_tag;
} rain1_rdev_header_t;

int rain1_rdev_header_read(rain1_rdev_header_t *header, stream_t *stream);

typedef enum { RAIN1_HEADER_MAGIC = 0xA2A3A4A5 } rain1_header_magic_t;

typedef struct
{
    rain1_header_magic_t magic;
    uint32_t blended_stripes;
    uint32_t su_size;
    uint32_t max_sectors;
    sync_tag_t sync_tag;
    uint64_t logical_slot_size;
    uint32_t dirty_zone_size;
    exa_nodeset_t nodes_resync;
    exa_nodeset_t nodes_update;
    uint32_t nb_rdevs;
} rain1_header_t;

int rain1_header_read(rain1_header_t *header, stream_t *stream);

uint64_t rain1_group_serialized_size(const rain1_group_t *rxg);
int rain1_group_serialize(const rain1_group_t *rxg, stream_t *stream);
int rain1_group_deserialize(rain1_group_t **rxg, const storage_t *storage,
                               stream_t *stream);

#endif /* __LAY_RAIN1_SUPERBLOCK_H__ */
