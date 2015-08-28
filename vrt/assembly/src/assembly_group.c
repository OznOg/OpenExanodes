/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>
#include <string.h> /* for memcpy */

#include "os/include/os_inttypes.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "os/include/os_mem.h"
#include "log/include/log.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/spof_group.h"
#include "vrt/assembly/src/assembly_group.h"
#include "vrt/assembly/src/assembly_volume.h"
#include "vrt/assembly/src/assembly_slot.h"
#include "vrt/assembly/include/assembly_prediction.h"

#include "common/include/exa_math.h"

/* FIXME Clarify roles of _init(), _cleanup(), _setup(), etc */


static void __assembly_group_insert_volume(assembly_group_t *ag, assembly_volume_t *av)
{
    assembly_volume_t *s;

    EXA_ASSERT(av != NULL);
    EXA_ASSERT(av->next == NULL);

    s = ag->subspaces;
    while (s != NULL && s->next != NULL)
        s = s->next;

    if (s == NULL)
        ag->subspaces = av;
    else
        s->next = av;

    ag->num_subspaces++;
}

static void __assembly_group_remove_volume(assembly_group_t *ag, assembly_volume_t *av)
{
    assembly_volume_t *s, *prev;

    EXA_ASSERT(ag->subspaces != NULL);

    s = ag->subspaces;
    prev = NULL;
    while (s != NULL && s != av)
    {
        prev = s;
        s = s->next;
    }
    EXA_ASSERT(s != NULL);

    if (prev != NULL)
        prev->next = s->next;
    else
        ag->subspaces = s->next;

    av->next = NULL;

    ag->num_subspaces--;
}

void assembly_group_init(assembly_group_t *ag)
{
    ag->initialized = true;

    ag->slot_size  = 0;
    ag->slot_width = 0;

    ag->subspaces = NULL;
    ag->num_subspaces = 0;
}

void assembly_group_cleanup(assembly_group_t *ag)
{
    assembly_volume_t *v;

    v = ag->subspaces;
    while (v != NULL)
    {
        assembly_volume_t *next = v->next;

        __assembly_group_remove_volume(ag, v);
        assembly_volume_free(v);

        v = next;
    }

    ag->subspaces = NULL;
    ag->num_subspaces = 0;
}

bool assembly_group_equals(const assembly_group_t *a, const assembly_group_t *b)
{
    assembly_volume_t *av1, *av2;

    if (a->initialized != b->initialized)
        return false;

    if (a->slot_size != b->slot_size)
        return false;

    if (a->slot_width != b->slot_width)
        return false;

    if (a->num_subspaces != b->num_subspaces)
        return false;

    av1 = a->subspaces;
    av2 = b->subspaces;
    while (av1 != NULL && av2 != NULL)
    {
        if (!assembly_volume_equals(av1, av2))
            return false;

        av1 = av1->next;
        av2 = av2->next;
    }
    if (av1 != NULL || av2 != NULL)
        return false;

    return true;
}

int assembly_group_setup(assembly_group_t *ag, uint32_t slot_width,
                         uint32_t chunk_size)
{
    EXA_ASSERT(ag->initialized);

    ag->slot_width = slot_width;
    ag->slot_size  = slot_width * chunk_size;

    return EXA_SUCCESS;
}

uint64_t assembly_group_get_used_slots_count(const assembly_group_t *ag)
{
    uint64_t slots_used_by_subspaces = 0;
    assembly_volume_t *s;

    EXA_ASSERT(ag != NULL);
    for (s = ag->subspaces; s != NULL; s = s->next)
        slots_used_by_subspaces += s->total_slots_count;

    return slots_used_by_subspaces;
}

uint64_t assembly_group_get_max_slots_count(const assembly_group_t *ag,
                                            const storage_t *storage)
{
    return assembly_group_get_available_slots_count(ag, storage)
           + assembly_group_get_used_slots_count(ag);
}

uint64_t assembly_group_get_available_slots_count(const assembly_group_t *ag,
                                                  const storage_t *storage)
{
    uint64_t per_spof_chunk_count[storage->num_spof_groups];
    uint32_t i;

    for (i = 0; i < storage->num_spof_groups; i++)
        per_spof_chunk_count[i] = spof_group_free_chunk_count(&storage->spof_groups[i]);

    return assembly_predict_max_slots_without_sparing(storage->num_spof_groups,
                                                      ag->slot_width,
                                                      per_spof_chunk_count);
}

uint32_t assembly_group_get_slot_width(const assembly_group_t *ag)
{
    EXA_ASSERT(ag != NULL);

    return ag->slot_width;
}

void assembly_group_map_sector_to_slot(const assembly_group_t *ag,
                                       const assembly_volume_t *av,
                                       uint64_t slot_size,
                                       uint64_t vsector,
                                       const slot_t **slot,
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

    /* Find the slot */
    *slot = av->slots[volume_slot_index];
}

/**
 *  Reserves more slots from the group's slots, or releases unused
 *  ones.
 */
int assembly_group_resize_volume(assembly_group_t *ag, assembly_volume_t *av,
                                 uint64_t new_slots_count, const storage_t *storage)
{
    if (new_slots_count > assembly_group_get_max_slots_count(ag, storage))
        return -VRT_ERR_NOT_ENOUGH_FREE_SC;

    return assembly_volume_resize(av, storage, ag->slot_width, new_slots_count);
}

assembly_volume_t *assembly_group_lookup_volume(const assembly_group_t *ag,
                                                const exa_uuid_t *uuid)
{
    assembly_volume_t *s;

    s = ag->subspaces;
    while (s != NULL && !uuid_is_equal(&s->uuid, uuid))
        s = s->next;

    return s;
}

int assembly_group_reserve_volume(assembly_group_t *ag, const exa_uuid_t *uuid,
                                     uint64_t nb_slots, assembly_volume_t **av,
                                     storage_t *storage)

{
    int err;

    *av = assembly_volume_alloc(uuid);
    if (*av == NULL)
        return -ENOMEM;

    /* Allocating all new slots is like resizing from 0 to nb_slots */
    err = assembly_group_resize_volume(ag, *av, nb_slots, storage);
    if (err != 0)
    {
        assembly_volume_free(*av);
        return err;
    }

    __assembly_group_insert_volume(ag, *av);

    return 0;
}

void __assembly_group_release_volume(assembly_group_t *ag, assembly_volume_t *av,
                                        const storage_t *storage)
{
    int err;

    __assembly_group_remove_volume(ag, av);

    /* Release is like realloc'ing to 0 slots */
    /* FIXME never fails ? */
    err = assembly_group_resize_volume(ag, av, 0, storage);
    EXA_ASSERT(err == EXA_SUCCESS);

    assembly_volume_free(av);
}

int assembly_group_header_read(ag_header_t *header, stream_t *stream)
{
    int r;

    r = stream_read(stream, header, sizeof(ag_header_t));
    if (r < 0)
        return r;
    else if (r != sizeof(ag_header_t))
        return -EIO;

    return 0;
}

uint64_t assembly_group_serialized_size(const assembly_group_t *ag)
{
    uint64_t total_subspaces_size;
    assembly_volume_t *av;

    total_subspaces_size = 0;
    av = ag->subspaces;
    while (av != NULL)
    {
        total_subspaces_size += assembly_volume_serialized_size(av);
        av = av->next;
    }

    return sizeof(ag_header_t) + total_subspaces_size;
}

int assembly_group_serialize(const assembly_group_t *ag, stream_t *stream)
{
    ag_header_t header;
    int w;
    const assembly_volume_t *av;
    int err;

    header.magic = AG_HEADER_MAGIC;
    header.format = AG_HEADER_FORMAT;
    header.slot_size = ag->slot_size;
    header.slot_width = ag->slot_width;
    header.num_subspaces = ag->num_subspaces;

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    av = ag->subspaces;
    while (av != NULL)
    {
        err = assembly_volume_serialize(av, stream);
        if (err != 0)
            return err;

        av = av->next;
    }

    return 0;
}

/* FIXME 1st param should be 'assembly_group **' like all other
         deserialization functions, but it's not possible right now as a
         group's assembly_group is contained in the group (not just a
         pointer) */
int assembly_group_deserialize(assembly_group_t *ag, const storage_t *storage,
                               stream_t *stream)
{
    ag_header_t header;
    uint32_t k;
    int err;

    err = assembly_group_header_read(&header, stream);
    if (err != 0)
        return err;

    if (header.magic != AG_HEADER_MAGIC)
        return -VRT_ERR_SB_MAGIC;

    if (header.format != AG_HEADER_FORMAT)
        return -VRT_ERR_SB_FORMAT;

    ag->initialized = true;
    ag->slot_size = header.slot_size;
    ag->slot_width = header.slot_width;
    ag->subspaces = NULL;
    /* ag->num_subspaces *must* be set to zero since it is
       incremented as subspaces are inserted (further down) */
    ag->num_subspaces = 0;

    for (k = 0; k < header.num_subspaces; k++)
    {
        assembly_volume_t *av;

        err = assembly_volume_deserialize(&av, storage, stream);
        if (err != 0)
            goto failed;

        /* No need to perform an explicit reservation of the subspace's
           slots since the slots' reservation status have been set by the
           slot deserialization */

        __assembly_group_insert_volume(ag, av);
    }

    return 0;

failed:
    EXA_ASSERT(err != 0);

    assembly_group_cleanup(ag);

    return err;
}

