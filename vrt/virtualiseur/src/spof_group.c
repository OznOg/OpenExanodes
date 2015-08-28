/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/virtualiseur/include/spof_group.h"

#include "common/include/exa_assert.h"

#include "os/include/os_error.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"

void spof_group_init(spof_group_t *spof_group)
{
    spof_group_cleanup(spof_group);
}

void spof_group_cleanup(spof_group_t *spof_group)
{
    uint32_t i;

    spof_group->spof_id = SPOF_ID_NONE;
    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        spof_group->realdevs[i] = NULL;

    spof_group->nb_realdevs = 0;
}

int spof_group_add_rdev(spof_group_t *spof_group, vrt_realdev_t *rdev)
{
    EXA_ASSERT(spof_group != NULL);
    EXA_ASSERT(rdev != NULL);

    if (spof_group->nb_realdevs >= NBMAX_DISKS_PER_SPOF_GROUP)
        return -ENOSPC;

    spof_group->realdevs[spof_group->nb_realdevs] = rdev;
    spof_group->nb_realdevs++;

    return 0;
}

int spof_group_del_rdev(spof_group_t *spof_group, const vrt_realdev_t *rdev)
{
    uint32_t i, j;

    EXA_ASSERT(spof_group != NULL);
    EXA_ASSERT(rdev != NULL);

    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        if (spof_group->realdevs[i] == rdev)
        {
            for (j = i; j < NBMAX_DISKS_PER_SPOF_GROUP - 1; j++)
                spof_group->realdevs[j] = spof_group->realdevs[j + 1];

            spof_group->realdevs[NBMAX_DISKS_PER_SPOF_GROUP - 1] = NULL;
            spof_group->nb_realdevs--;

            return 0;
        }

    return -ENOENT;
}

void spof_group_get_nodes(const spof_group_t *spof_group, exa_nodeset_t *nodes)
{
    int i;

    exa_nodeset_reset(nodes);
    for (i = 0; i < spof_group->nb_realdevs; i++)
        exa_nodeset_add(nodes, spof_group->realdevs[i]->node_id);
}

/* FIXME This should disappear once the spof_groups are totally contained
 * in the storage. */
spof_group_t *spof_group_lookup(spof_id_t id, spof_group_t spofs[],
                                      uint32_t num_spofs)
{
    uint32_t i;

    for (i = 0; i < num_spofs; i++)
    {
        if (spofs[i].spof_id == id)
            return &spofs[i];
    }

    return NULL;
}

/**
 * @brief Finds the least used device in a list
 * Select the real dev with the least used chunks and at least one chunk free.
 *
 * @param[in] realdevs      The list of devices.
 * @param[in] nb_realdevs   The number of devices in the list
 *
 * @return the least used device
 */
static struct vrt_realdev *least_used_rdev(struct vrt_realdev **realdevs,
                                           uint32_t nb_realdevs)
{
    struct vrt_realdev *rdev = NULL;
    uint32_t min_used = 0;
    uint32_t i;

    for (i = 0; i < nb_realdevs; i++)
    {
        uint32_t nb_used;

        EXA_ASSERT(realdevs[i] != NULL);

        if (realdevs[i]->chunks.free_chunks_count == 0)
            continue;

        nb_used = realdevs[i]->chunks.total_chunks_count
                  - realdevs[i]->chunks.free_chunks_count;

        /* if first rdev with a free chunk or the least used so far */
        if (rdev == NULL || nb_used < min_used)
        {
            rdev = realdevs[i];
            min_used = nb_used;
        }
    }

    EXA_ASSERT(rdev != NULL);
    EXA_ASSERT(rdev->chunks.free_chunks_count > 0);

    return rdev;
}

chunk_t *spof_group_get_chunk(spof_group_t *spof_group)
{
    struct vrt_realdev *rdev;

    rdev = least_used_rdev(spof_group->realdevs, spof_group->nb_realdevs);
    EXA_ASSERT(rdev != NULL);

    return chunk_get_first_free_from_rdev(rdev);
}

void __spof_group_put_chunk(chunk_t *chunk)
{
    chunk_put_to_rdev(chunk);
}

/**
 * @brief Return the number of free chunks a SPOF group
 *
 * @param[in] spof_group SPOF group
 *
 * @return The number of free chunks
 */
uint32_t spof_group_free_chunk_count(const spof_group_t *spof_group)
{
    uint32_t i;
    uint32_t sum = 0;

    for (i = 0; i < spof_group->nb_realdevs; i++)
        sum += spof_group->realdevs[i]->chunks.free_chunks_count;

    return sum;
}

uint64_t spof_group_total_chunk_count(const spof_group_t *spof_group)
{
    uint32_t i;
    uint64_t sum = 0;

    for (i = 0; i < spof_group->nb_realdevs; i++)
        sum += spof_group->realdevs[i]->chunks.total_chunks_count;

    return sum;
}
