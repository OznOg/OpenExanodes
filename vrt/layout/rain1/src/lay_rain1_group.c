/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h> /* for memset */

#include "vrt/layout/rain1/src/lay_rain1_desync_info.h"
#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_rdev.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_thread.h"

rain1_group_t *rain1_group_alloc(void)
{
    rain1_group_t *rxg;

    rxg = os_malloc(sizeof(rain1_group_t));
    if (rxg == NULL)
	return NULL;

    /* FIXME that's crap, we should explicitely initialize all fields */
    memset(rxg, 0, sizeof(rain1_group_t));

    assembly_group_init(&rxg->assembly_group);

    rxg->sync_tag = SYNC_TAG_ZERO;
    os_thread_rwlock_init(&rxg->status_lock);

    return rxg;
}

void __rain1_group_free(rain1_group_t *rxg, const storage_t *storage)
{
    assembly_volume_t *s;
    int i;

    if (rxg == NULL)
        return;

    /* FIXME This is crap but needed to free RainX's private data
             in the slots */
    s = rxg->assembly_group.subspaces;
    while (s != NULL)
    {
        assembly_volume_t *next = s->next;
        rain1_delete_subspace(rxg, &s, storage);
        s = next;
    }

    assembly_group_cleanup(&rxg->assembly_group);

    for (i = 0; i < rxg->nb_rain1_rdevs; i++)
        os_free(rxg->rain1_rdevs[i]);

    os_thread_rwlock_destroy(&rxg->status_lock);

    os_free(rxg);
}

int rain1_create_subspace(rain1_group_t *rxg, const exa_uuid_t *uuid, uint64_t size,
                          struct assembly_volume **av, storage_t *storage)
{
    uint64_t nb_slots, slot_index;
    int err = 0;

    EXA_ASSERT(size > 0);

    nb_slots = quotient_ceil64(size, rain1_group_get_slot_data_size(rxg));

    err = assembly_group_reserve_volume(&rxg->assembly_group, uuid, nb_slots,
                                           av, storage);
    if (err != 0)
        return err;

    /* Allocate RainX's per-slot private data */
    for (slot_index = 0; slot_index < (*av)->total_slots_count; slot_index++)
    {
        slot_t *slot = (*av)->slots[slot_index];

        slot->private = slot_desync_info_alloc(rxg->sync_tag);
        if (slot->private == NULL)
        {
            err = -ENOMEM;
            break;
        }
    }

    if (err != 0)
        rain1_delete_subspace(rxg, av, storage);

    return err;
}

void rain1_delete_subspace(rain1_group_t *rxg, struct assembly_volume **av,
                           const storage_t *storage)
{
    uint64_t slot_index;

    for (slot_index = 0; slot_index < (*av)->total_slots_count; slot_index++)
    {
        slot_t *slot = (*av)->slots[slot_index];
        slot_desync_info_free(slot->private);
    }

    assembly_group_release_volume(&rxg->assembly_group, *av, storage);
}

int rain1_resize_subspace(rain1_group_t *rxg, assembly_volume_t *av,
                          uint64_t new_nb_slots, const storage_t *storage)
{
    uint64_t old_nb_slots, slot_idx;
    int err;

    old_nb_slots = av->total_slots_count;

    if (new_nb_slots < old_nb_slots)
        for (slot_idx = new_nb_slots; slot_idx < old_nb_slots; slot_idx++)
        {
            slot_t *slot = av->slots[slot_idx];
            slot_desync_info_free(slot->private);
        }

    err = assembly_group_resize_volume(&rxg->assembly_group, av, new_nb_slots,
                                       storage);
    EXA_ASSERT(err == 0);

    if (new_nb_slots > old_nb_slots)
        for (slot_idx = old_nb_slots; slot_idx < new_nb_slots; slot_idx++)
        {
            slot_t *slot = av->slots[slot_idx];
            slot->private = slot_desync_info_alloc(rxg->sync_tag);
        }

    return 0;
}

uint64_t rain1_group_get_dzone_per_slot_count(const rain1_group_t *rxg)
{
    return quotient_ceil64(rxg->logical_slot_size, rxg->dirty_zone_size);
}

/* In sectors */
#define RAIN1_DZONE_SYNC_BLK_SIZE (128 * 2)

/* FIXME: sync_jobs block size and number is not understandable
 *
 * What does this 8 means ??? why not using more jobs ?
 * Why 128 KB ??? Is it linked to the NBD locking ???
 *
 * Is it a coincidence that the NBD manages messages of 128 KB (with no tuning)
 * and that the SU by default is 1024 KB (= 8 x 128 KB) ?
 */

unsigned int rain1_group_get_sync_job_blksize(const rain1_group_t *rxg)
{
    return MIN(rxg->su_size, RAIN1_DZONE_SYNC_BLK_SIZE);
}

unsigned int rain1_group_get_sync_jobs_count(const rain1_group_t *rxg)
{
    unsigned int blksize = rain1_group_get_sync_job_blksize(rxg);

    /* Legacy comment: We cannot lock more than NBMAX_DISK_LOCKED_ZONES areas
     * simultaneously on a disk, so we cannot use more jobs than that
     *
     * FIXME: this limitation means that the locking interface is quite poor and that
     * there is good chances that it cannot handle the limit cases properly.
     * Anyway, NBMAX_DISK_LOCKED_ZONES = 32 and it is the size of 2 an uint64 arrays
     * It could be set to a bigger value without pb.
     */

    return MIN(NBMAX_DISK_LOCKED_ZONES,
               RAIN1_DZONE_SYNC_BLK_SIZE * 8 / blksize);
}

bool rain1_group_equals(const rain1_group_t *rxg1,
                        const rain1_group_t *rxg2)
{
    int i;

    if (rxg1->blended_stripes != rxg2->blended_stripes)
        return false;
    if (rxg1->su_size != rxg2->su_size)
        return false;
    if (rxg1->max_sectors != rxg2->max_sectors)
        return false;
    if (rxg1->sync_tag != rxg2->sync_tag)
        return false;
    if (rxg1->logical_slot_size != rxg2->logical_slot_size)
        return false;
    if (rxg1->dirty_zone_size != rxg2->dirty_zone_size)
        return false;
    if (!exa_nodeset_equals(&rxg1->nodes_resync, &rxg2->nodes_resync))
        return false;
    if (!exa_nodeset_equals(&rxg1->nodes_update, &rxg2->nodes_update))
        return false;

    for (i = 0; i < rxg1->nb_rain1_rdevs; i++)
        if (!rain1_realdev_equals(rxg1->rain1_rdevs[i], rxg2->rain1_rdevs[i]))
            return false;

    if (!assembly_group_equals(&rxg1->assembly_group, &rxg2->assembly_group))
        return false;

    return true;
}
