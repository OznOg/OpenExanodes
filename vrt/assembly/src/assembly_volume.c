/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/assembly/src/assembly_volume.h"
#include "vrt/assembly/src/assembly_slot.h"

#include "log/include/log.h"

#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"

#include <string.h> /* for memcpy */
#include <errno.h>

assembly_volume_t *assembly_volume_alloc(const exa_uuid_t *uuid)
{
    assembly_volume_t *av;

    av = os_malloc(sizeof(assembly_volume_t));
    if (av == NULL)
        return NULL;

    uuid_copy(&av->uuid, uuid);
    av->slots = NULL;
    av->total_slots_count = 0;

    av->next = NULL;

    return av;
}

/**
 * Free slots
 *
 * @param[in] slots     The slots to free
 * @param[in] start     The first slot to free
 * @param[in] num_slots The number of slots to free
 */
static void __free_slots(slot_t **slots, uint64_t start, uint64_t num_slots)
{
    uint64_t i;

    for (i = start; i < start + num_slots; i++)
    {
        /* FIXME Free layout's private data */
        slot_free(slots[i]);
    }
}

void __assembly_volume_free(assembly_volume_t *av)
{
    if (av == NULL)
        return;

    /* Either we have zero slots, or we have a slots table. */
    EXA_ASSERT(av->total_slots_count == 0 || av->slots != NULL);

    if (av->slots != NULL)
    {
        __free_slots(av->slots, 0, av->total_slots_count);
        os_free(av->slots);
    }

    os_free(av);
}

int assembly_volume_resize(assembly_volume_t *av, const storage_t *storage,
                           uint32_t slot_width, uint64_t new_slots_count)
{
    uint64_t old_slots_count = av->total_slots_count;
    slot_t **old_slots = av->slots;
    slot_t **new_slots;

    if (new_slots_count == old_slots_count)
        return 0;

    /* Allocate a new array to store the list of slots, and copy the
     * existing one.
     */
    new_slots = os_malloc(new_slots_count * sizeof(slot_t *));
    if (new_slots == NULL)
        return -ENOMEM;

    /* Copy the old slots to the new ones */
    memcpy(new_slots, old_slots,
           MIN(new_slots_count, old_slots_count) * sizeof(slot_t *));

    av->slots             = new_slots;
    av->total_slots_count = new_slots_count;

    if (new_slots_count > old_slots_count)
    {
        /* The new size of the volume is *bigger* than the current
         * size. We must reserve new slots for this volume.
         */
        uint64_t idx;

        for (idx = old_slots_count; idx < new_slots_count; idx++)
            av->slots[idx] = slot_make(storage->spof_groups,
                                       storage->num_spof_groups, slot_width);
    }
    else
    {
        /* The new size of the volume is *lower* than the current
         * size. We must release some slots of this volume. */
        __free_slots(old_slots, new_slots_count, old_slots_count - new_slots_count);
    }

    os_free(old_slots);

    return 0;
}

bool assembly_volume_equals(const assembly_volume_t *a, const assembly_volume_t *b)
{
    uint64_t i;

    if (!uuid_is_equal(&a->uuid, &b->uuid))
        return false;

    if (a->total_slots_count != b->total_slots_count)
        return false;

    for (i = 0; i < a->total_slots_count; i++)
        if (!slot_equals(a->slots[i], b->slots[i]))
            return false;

    /* ->next is irrelevant */

    return true;
}

void assembly_volume_map_sector_to_slot(const assembly_volume_t *av, uint64_t slot_size,
                                        uint64_t vsector,
                                        unsigned int *slot_index,
                                        uint64_t *offset_in_slot)
{
    uint64_t volume_slot_index;

    EXA_ASSERT_VERBOSE(vsector < av->total_slots_count * slot_size,
                       "vsector=%" PRIu64 ", av->total_slots_count=%" PRIu64 ", slot_size=%" PRIu64 "\n",
                       vsector, av->total_slots_count, slot_size);

    /* Compute the index of the slot in the volume slot array */
    volume_slot_index = vsector / slot_size;
    EXA_ASSERT(volume_slot_index < av->total_slots_count);

    /* Compute the offset in the slot */
    *offset_in_slot = vsector % slot_size;

    *slot_index = volume_slot_index;
}

int assembly_volume_header_read(av_header_t *header, stream_t *stream)
{
    int r = stream_read(stream, header, sizeof(av_header_t));

    if (r < 0)
        return r;
    else if (r != sizeof(av_header_t))
        return -EIO;

    return 0;
}

uint64_t assembly_volume_serialized_size(const assembly_volume_t *av)
{
    /* All slots have the same size */
    return sizeof(av_header_t)
         + av->total_slots_count * slot_serialized_size(av->slots[0]);
}

int assembly_volume_serialize(const assembly_volume_t *av, stream_t *stream)
{
    av_header_t header;
    uint64_t i;
    int w;

    header.magic = AV_HEADER_MAGIC;
    header.reserved = 0;
    header.uuid = av->uuid;
    header.total_slot_count = av->total_slots_count;

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    for (i = 0; i < av->total_slots_count; i++)
    {
        int err = slot_serialize(av->slots[i], stream);
        if (err != 0)
            return err;
    }

    return 0;
}

int assembly_volume_deserialize(assembly_volume_t **av,
                                   const storage_t *storage, stream_t *stream)
{
    av_header_t header;
    int err = 0;

    err = assembly_volume_header_read(&header, stream);
    if (err != 0)
        return err;

    if (header.magic != AV_HEADER_MAGIC)
        return -VRT_ERR_SB_MAGIC;

    if (header.reserved != 0)
        return -VRT_ERR_SB_CORRUPTION;

    *av = assembly_volume_alloc(&header.uuid);
    if (*av == NULL)
        return -ENOMEM;

    (*av)->slots = os_malloc(header.total_slot_count * sizeof(slot_t *));
    if ((*av)->slots == NULL)
    {
        err = -ENOMEM;
        goto failed;
    }

    /* Total slots count is updated at each slot deserialization instead
     * of being set from the header, because failure to deserialize one
     * slot in the middle of the slots means we'll only have the correctly
     * deserialized slots to free.
     */
    for ((*av)->total_slots_count = 0;
         (*av)->total_slots_count < header.total_slot_count;
         (*av)->total_slots_count++)
    {
        err = slot_deserialize(&(*av)->slots[(*av)->total_slots_count],
                                  storage, stream);
        if (err != 0)
            goto failed;
    }

    return 0;

failed:
    EXA_ASSERT(err != 0);

    if (*av != NULL)
        assembly_volume_free(*av);

    return err;
}
