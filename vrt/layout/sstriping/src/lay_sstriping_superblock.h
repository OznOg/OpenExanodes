/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __LAY_SSTRIPING_SUPERBLOCK_H__
#define __LAY_SSTRIPING_SUPERBLOCK_H__

#include "vrt/layout/sstriping/src/lay_sstriping_group.h"
#include "os/include/os_inttypes.h"

typedef enum { SSTRIPING_HEADER_MAGIC = 0xB1B6B8B7 } sstriping_header_magic_t;

typedef struct
{
    sstriping_header_magic_t magic;
    uint32_t su_size;
    uint64_t logical_slot_size;
} sstriping_header_t;

int sstriping_header_read(sstriping_header_t *header, stream_t *stream);

uint64_t sstriping_group_serialized_size(const sstriping_group_t *ssg);
int sstriping_group_serialize(const sstriping_group_t *ssg, stream_t *stream);
int sstriping_group_deserialize(sstriping_group_t **ssg, const storage_t *storage,
                               stream_t *stream);

#endif /* __LAY_SSTRIPING_SUPERBLOCK_H__ */
