/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h> /* for memset */

#include "vrt/assembly/src/assembly_slot.h"

#include "vrt/virtualiseur/include/spof_group.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"

#include "os/include/os_error.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"

void assembly_slot_map_sector_to_rdev(const slot_t *slot, unsigned int chunk_index,
                                      uint64_t offset, struct vrt_realdev **rdev,
                                      uint64_t *rsector)
{
    /* Find the disk */
    *rdev = chunk_get_rdev(slot->chunks[chunk_index]);

    /* Compute the sector number on the disk */
    *rsector = chunk_get_offset(slot->chunks[chunk_index]) + offset;
}


typedef struct
{
     uint32_t free_count;
     spof_group_t *spof_group;
} spof_info_t;

/**
 * @brief Comparison function for SPOF groups whose criterion is the
 *        number of free chunks. It is invoked by the quick sort.
 *
 * @attention The comparison is inverted in order to sort in
 *            descending order
 *
 * In case the number of free chunks is the same in both spof groups, we
 * force the order by using the SPOF id as a second criteria.
 * This is made necessary because, from a qsort POV, returning 0 means
 * "whatever order between those two" meaning that it may place one before
 * the other arbitrarily... But in our situation, the order MUST always
 * remain the same.
 *
 * @param[in] p1 First element to compare
 * @param[in] p2 Second element to compare
 *
 * @return 0 if the SPOF groups have the same number of chunks, > 0 if
 *         the second SPOF group has more free chunks than the first
 *         one, < 0 otherwise
 */
static int spof_info_compare(const void *p1, const void *p2)
{
    const spof_info_t *spof_info1 = p1;
    const spof_info_t *spof_info2 = p2;

    if (spof_info1->free_count == spof_info2->free_count)
        return spof_info2->spof_group->spof_id - spof_info1->spof_group->spof_id;

    return spof_info2->free_count - spof_info1->free_count;
}

static void generic_make_slot(struct slot *slot,
                              spof_group_t *spof_groups,
                              uint32_t nb_spof_groups, uint32_t slot_width)
{
    spof_info_t spof_info[nb_spof_groups];
    uint32_t i;

    EXA_ASSERT(spof_groups != NULL && nb_spof_groups != 0);

    slot->chunks = os_malloc(slot_width * sizeof(chunk_t *));
    EXA_ASSERT(slot->chunks != NULL);

    /* Create a working copy of the array of SPOF groups */
    for (i = 0; i < nb_spof_groups; i++)
    {
        spof_info[i].free_count = spof_group_free_chunk_count(&spof_groups[i]);
        spof_info[i].spof_group = &spof_groups[i];
    }

    qsort(spof_info, nb_spof_groups, sizeof(spof_info_t), spof_info_compare);

    for (i = 0; i < slot_width; i++)
        slot->chunks[i] = spof_group_get_chunk(spof_info[i].spof_group);

    slot->width = slot_width;
}

struct slot *slot_make(spof_group_t *spof_groups,
                       uint32_t nb_spof_groups, uint32_t slot_width)
{
    struct slot *slot;

    EXA_ASSERT(slot_width > 0);

    slot = os_malloc(sizeof(struct slot));
    EXA_ASSERT(slot != NULL);

    memset(slot, 0xDD, sizeof(struct slot));

    generic_make_slot(slot, spof_groups, nb_spof_groups, slot_width);

    slot->private = NULL;

    return slot;
}

void __slot_free(struct slot *slot)
{
    int i;

    if (slot == NULL)
        return;

    /* FIXME Re-enable this once the leaks have been fixed! */
#if 0
    /* The private data must have been released by its owner */
    EXA_ASSERT(slot->private == NULL);
#endif

    for (i = 0; i < slot->width; i++)
        spof_group_put_chunk(slot->chunks[i]);

    os_free(slot->chunks);
    os_free(slot);
}

bool slot_equals(const slot_t *a, const slot_t *b)
{
    uint32_t i;

    /* FIXME is that a correct behaviour ? */
    if (a == NULL || b == NULL)
        return a == b;

    if (a->width != b->width)
        return false;

    for (i = 0; i < a->width; i++)
        if (!chunk_equals(a->chunks[i], b->chunks[i]))
            return false;

    return true;
}

int chunk_header_read(chunk_header_t *header, stream_t *stream)
{
    int r = stream_read(stream, header, sizeof(chunk_header_t));

    if (r < 0)
        return r;
    else if (r != sizeof(chunk_header_t))
        return -EIO;

    return 0;
}

static int __chunk_serialize(const chunk_t *chunk, stream_t *stream)
{
    chunk_header_t header;
    vrt_realdev_t *rdev;
    int w;

    rdev = chunk_get_rdev(chunk);
    EXA_ASSERT(rdev != NULL);

    header.rdev_uuid = rdev->uuid;
    header.offset = chunk->offset;

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    return 0;
}

static int __chunk_deserialize(chunk_t **chunk, const storage_t *storage,
                               stream_t *stream)
{
    chunk_header_t header;
    vrt_realdev_t *rdev;
    int err;

    err = chunk_header_read(&header, stream);
    if (err != 0)
        return err;

    /* XXX The chunk should be gotten from (and allocated by) storage */
    rdev = storage_get_rdev(storage, &header.rdev_uuid);
    if (rdev == NULL)
        return -VRT_ERR_SB_CORRUPTION;

    *chunk = chunk_get_from_rdev_at_offset(rdev, header.offset);
    if (*chunk == NULL)
        return -ENOMEM;

    return 0;
}

int slot_header_read(slot_header_t *header, stream_t *stream)
{
    int r = stream_read(stream, header, sizeof(slot_header_t));

    if (r < 0)
        return r;
    else if (r != sizeof(slot_header_t))
        return -EIO;

    return 0;
}

uint64_t slot_serialized_size(const slot_t *slot)
{
    EXA_ASSERT(slot != NULL);

    return sizeof(slot_header_t) + slot->width * sizeof(chunk_header_t);
}

int slot_serialize(const slot_t *slot, stream_t *stream)
{
    slot_header_t slot_header;
    int w;
    uint32_t i;

    slot_header.width = slot->width;

    w = stream_write(stream, &slot_header, sizeof(slot_header));
    if (w < 0)
        return w;
    else if (w != sizeof(slot_header))
        return -EIO;

    for (i = 0; i < slot->width; i++)
    {
        int err = __chunk_serialize(slot->chunks[i], stream);
        if (err != 0)
            return err;
    }

    return 0;
}

int slot_deserialize(slot_t **slot, const storage_t *storage, stream_t *stream)
{
    slot_header_t slot_header;
    uint32_t i;
    int err = 0;

    err = slot_header_read(&slot_header, stream);
    if (err != 0)
        goto failed;

    *slot = os_malloc(sizeof(slot_t));
    if (*slot == NULL)
        return -ENOMEM;

    (*slot)->width = slot_header.width;

    (*slot)->chunks = os_malloc(slot_header.width * sizeof(chunk_t *));
    if ((*slot)->chunks == NULL)
    {
        os_free(*slot);
        err = -ENOMEM;
        goto failed;
    }

    for (i = 0; i < slot_header.width; i++)
        (*slot)->chunks[i] = NULL;

    for (i = 0; i < slot_header.width; i++)
    {
        err = __chunk_deserialize(&(*slot)->chunks[i], storage, stream);
        if (err != 0)
            goto failed;
    }

    return 0;

failed:

    EXA_ASSERT(err != 0);

    if (*slot != NULL)
    {
        for (i = 0; i < slot_header.width; i++)
            if ((*slot)->chunks[i])
                chunk_put_to_rdev((*slot)->chunks[i]);

        os_free((*slot)->chunks);

        os_free(*slot);
    }

    return err;
}

int slot_dump(const slot_t *slot, stream_t *stream)
{
    uint32_t i;
    int n;

    n = stream_printf(stream, "free:  %s\n", slot != NULL ? "true" : "false");
    if (n < 0)
        return n;

    if (slot == NULL)
        return 0;

    n = stream_printf(stream, "width: %"PRIu32"\n", slot->width);
    if (n < 0)
        return n;

    for (i = 0; i < slot->width; i++)
    {
        chunk_t *c = slot->chunks[i];
        vrt_realdev_t *rdev = chunk_get_rdev(c);
        n = stream_printf(stream, "chunk #%"PRIu32": "UUID_FMT" @ %"PRIu64"\n",
                          i, UUID_VAL(&rdev->uuid), c->offset);
        if (n < 0)
            return n;
    }

    return 0;
}
