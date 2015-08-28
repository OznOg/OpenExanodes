/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_CHUNK_H__
#define __VRT_CHUNK_H__

#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/common/include/vrt_stream.h"

#include "os/include/os_inttypes.h"

/**
 * The chunk representation inside the VRT
 */
typedef struct chunk
{
    struct vrt_realdev *rdev;
    uint64_t offset;
} chunk_t;

chunk_t *chunk_get_first_free_from_rdev(vrt_realdev_t *rdev);
chunk_t *chunk_get_from_rdev_at_offset(vrt_realdev_t *rdev, uint64_t offset);

void __chunk_put_to_rdev(chunk_t *chunk);
#define chunk_put_to_rdev(chunk) ( __chunk_put_to_rdev(chunk), (chunk) = NULL)

/**
 * Tell whether a chunk is equal to another.
 *
 * @param[in] a  Chunk
 * @param[in] b  Chunk
 *
 * @return true if a and b are equal, false otherwise
 */
bool chunk_equals(const chunk_t *a, const chunk_t *b);

struct vrt_realdev *chunk_get_rdev(const chunk_t *chunk);

uint64_t chunk_get_offset(const chunk_t *chunk);

#endif /* __VRT_CHUNK_H__ */
