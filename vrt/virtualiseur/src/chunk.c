/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/virtualiseur/include/chunk.h"
#include "vrt/virtualiseur/include/storage.h"

#include "common/include/uuid.h"
#include "common/include/exa_assert.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"

/**
 * @param[in] rdev    rdev from which we want a new chunk
 * @param[in] offset  offset of the chunk on the rdev
 *
 * return a new allocated chunk.
 */
static chunk_t *chunk_alloc(struct vrt_realdev *rdev, uint64_t offset)
{
    chunk_t *chunk = os_malloc(sizeof(chunk_t));

    EXA_ASSERT(chunk != NULL);

    chunk->rdev = rdev;
    chunk->offset = offset;

    return chunk;
}

/**
 * free a chunk.
 *
 * @param[in] chunk   the chunk to be freed.
 */
static void __chunk_free(chunk_t *chunk)
{
    os_free(chunk);
}
#define chunk_free(chunk) ( __chunk_free(chunk), (chunk) = NULL)

bool chunk_equals(const chunk_t *a, const chunk_t *b)
{
    vrt_realdev_t *rdev_a = chunk_get_rdev(a);
    vrt_realdev_t *rdev_b = chunk_get_rdev(b);

    return uuid_is_equal(&rdev_a->uuid, &rdev_b->uuid) && a->offset == b->offset;
}

struct vrt_realdev *chunk_get_rdev(const chunk_t *chunk)
{
    EXA_ASSERT(chunk);
    return chunk->rdev;
}

uint64_t chunk_get_offset(const chunk_t *chunk)
{
    EXA_ASSERT(chunk);
    return chunk->offset;

}

static uint64_t index_to_offset(uint32_t index, uint32_t chunk_size)
{
    EXA_ASSERT(chunk_size != 0);

    return (uint64_t)index * chunk_size + VRT_SB_AREA_SIZE;
}

static uint32_t offset_to_index(uint64_t offset, uint32_t chunk_size)
{
    EXA_ASSERT(chunk_size != 0);

    return (offset - VRT_SB_AREA_SIZE) / chunk_size;
}

chunk_t *chunk_get_from_rdev_at_offset(vrt_realdev_t *rdev, uint64_t offset)
{
    uint32_t index;

    index = offset_to_index(offset, rdev->chunks.chunk_size);
    EXA_ASSERT(index <= rdev->chunks.total_chunks_count);

    rdev->chunks.free_chunks = extent_list_remove_value(rdev->chunks.free_chunks, index);
    rdev->chunks.free_chunks_count--;

    return chunk_alloc(rdev, offset);
}

chunk_t *chunk_get_first_free_from_rdev(vrt_realdev_t *rdev)
{
    uint64_t offset;

    if (rdev->chunks.free_chunks == NULL)
        return NULL;

    offset = index_to_offset(rdev->chunks.free_chunks->start,
                             rdev->chunks.chunk_size);

    return chunk_get_from_rdev_at_offset(rdev, offset);
}

void __chunk_put_to_rdev(chunk_t *chunk)
{
    vrt_realdev_t *rdev;
    uint32_t index;

    EXA_ASSERT(chunk != NULL);

    rdev = chunk_get_rdev(chunk);
    EXA_ASSERT(rdev != NULL);

    index = offset_to_index(chunk->offset, rdev->chunks.chunk_size);

    rdev->chunks.free_chunks = extent_list_add_value(rdev->chunks.free_chunks, index);
    rdev->chunks.free_chunks_count++;

    chunk_free(chunk);
}
