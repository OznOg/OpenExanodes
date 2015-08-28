/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>
#include <stdlib.h>

#include "common/include/exa_names.h"
#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "common/include/exa_math.h"
#include "os/include/os_mem.h"

#include "log/include/log.h"

#include "vrt/common/include/waitqueue.h"
#include "vrt/assembly/src/assembly_group.h"
#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_nodes.h"
#include "vrt/virtualiseur/include/vrt_rebuild.h"

#include "vrt/layout/rain1/include/rain1.h"
#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_check.h"
#include "vrt/layout/rain1/src/lay_rain1_request.h"
#include "vrt/layout/rain1/src/lay_rain1_metadata.h"
#include "vrt/layout/rain1/src/lay_rain1_status.h"
#include "vrt/layout/rain1/src/lay_rain1_superblock.h"
#include "vrt/layout/rain1/src/lay_rain1_sync.h"
#include "vrt/layout/rain1/src/lay_rain1_sync_job.h" /* for sync_job pre-allocation */

#include "os/include/os_error.h"
#include "os/include/os_stdio.h"

#include "vrt/virtualiseur/include/vrt_perf.h"

/* FIXME Temporary, for backward compatibility. */
static void rain1_group_cleanup(void **private_data, const storage_t *storage)
{
    rain1_group_t *rxg = *private_data;

    rain1_group_free(rxg, storage);
    *private_data = NULL;
}

/* FIXME Should be in lay_rain1_group.c, as evidenced by the fact that its
         prototype is published by lay_rain1_group.h */
/**
 * Predicate -- true if the storage is rebuilding
 *              (updating or replicating)
 *
 * @return true if the storage is rebuilding, false otherwise
 */
bool rain1_group_is_rebuilding(const void *layout_data)
{
    const rain1_group_t *rxg = layout_data;
    const rain1_realdev_t *lr;
    int i;

    foreach_rainx_rdev(rxg, lr, i)
        if (rain1_rdev_is_rebuilding(lr))
            return true;

    return false;
}

static int __wipe_slot_metadata(rain1_group_t *rxg, assembly_volume_t *av,
                                uint64_t start, uint64_t end)
{
    uint64_t slot_idx;
    int err = 0;

    for (slot_idx = start; slot_idx <= end; slot_idx++)
    {
        slot_t *slot = av->slots[slot_idx];

        /* Since volume create is called on all nodes, the metadata reset
           process is distributed on all nodes */
        if ((slot_idx % vrt_node_get_upnodes_count()) != vrt_node_get_upnode_id())
            continue;

        err = rain1_wipe_slot_metadata(rxg, slot);
        if (err != 0)
            break;
    }

    return err;
}

static int __rain1_create_subspace(void *private_data, const exa_uuid_t *uuid,
                                   uint64_t size, struct assembly_volume **av,
                                   storage_t *storage)
{
    rain1_group_t *lg = private_data;
    int err;

    err = rain1_create_subspace(lg, uuid, size, av, storage);
    if (err != 0)
        return err;

    err = __wipe_slot_metadata(lg, *av, 0, (*av)->total_slots_count - 1);

    if (err != 0)
        rain1_delete_subspace(lg, av, storage);

    return err;
}

static void __rain1_delete_subspace(void *private_data, struct assembly_volume **av,
                                    storage_t *storage)
{
    rain1_group_t *lg = private_data;
    uint64_t slot_index;

    for (slot_index = 0; slot_index < (*av)->total_slots_count; slot_index++)
    {
        slot_t *slot = (*av)->slots[slot_index];
        slot_desync_info_t *block = slot->private;

        EXA_ASSERT(block != NULL);

        RAINX_PERF_STOP_BEGIN();

        /* Before deleting metadata buffer, write the last state to disk */
        rain1_write_slot_metadata(lg, slot, vrt_node_get_local_id(),
                                  block->in_memory_metadata);
        RAINX_PERF_STOP_END();
    }

    RAINX_PERF_STOP_FLUSH();

    rain1_delete_subspace(lg, av, storage);
}

static int rain1_volume_get_status(const struct vrt_volume *volume)
{
    if (volume->group->status == EXA_GROUP_OFFLINE)
	return VRT_ADMIND_STATUS_DOWN;
    else
	return VRT_ADMIND_STATUS_UP;
}

static uint64_t rain1_volume_get_size(const struct vrt_volume *volume)
{
    const rain1_group_t *lg = RAIN1_GROUP(volume->group);
    const struct assembly_volume *av = volume->assembly_volume;

    return av->total_slots_count * rain1_group_get_slot_data_size(lg);
}

/**
 * Resize (either grow or shrink a volume to a new size. This
 * operation must be called on all nodes.
 *
 * @param[in] volume  The volume to resize
 *
 * @param[in] newsize The new size of the volume in sectors
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
static int rain1_volume_resize(struct vrt_volume *volume, uint64_t newsize,
                               const storage_t *storage)
{
    rain1_group_t *lg = RAIN1_GROUP(volume->group);
    struct assembly_group *ag = &lg->assembly_group;
    assembly_volume_t *av = volume->assembly_volume;
    uint64_t new_nb_slots, old_nb_slots = av->total_slots_count;
    int err;

    new_nb_slots = quotient_ceil64(newsize, rain1_group_get_slot_data_size(lg));

    exalog_debug("resize volume '%s': size = %" PRIu64 " sectors (=%" PRIu64
                 " slots)", volume->name, newsize, new_nb_slots);

    err = rain1_resize_subspace(lg, av, new_nb_slots, storage);
    if (err != 0)
        return err;

    /* When shrinking, there is no new slot to wipe */
    if (new_nb_slots <= old_nb_slots)
        return EXA_SUCCESS;

    err = __wipe_slot_metadata(lg, av, old_nb_slots, new_nb_slots - 1);
    if (err != EXA_SUCCESS)
    {
        int rollback_error = assembly_group_resize_volume(ag, av, old_nb_slots,
                                                          storage);
        EXA_ASSERT_VERBOSE(rollback_error == EXA_SUCCESS,
                           "Resize of volume '%s' failed [%s (%d)] and "
                           "rollback failed [%s (%d)], cannot continue due "
                           "to unrecoverable errors.", volume->name,
                           exa_error_msg(err), err,
                           exa_error_msg(rollback_error), rollback_error);
        return err;
    }

    return EXA_SUCCESS;
}

/**
 * Finalize the group creation.
 *
 * @param[in] group The group currently being created
 *
 * @return EXA_SUCCESS on success, an error code on failure.
 */
/* FIXME Pass in the spof array (and spof count) */
static int
rain1_group_create(storage_t *storage, void **private_data,
                   uint32_t slot_width, uint32_t chunk_size, uint32_t su_size,
                   uint32_t dirty_zone_size, uint32_t blended_stripes,
                   uint32_t nb_spare, char *error_msg)
{
    struct assembly_group *ag;
    struct vrt_realdev *rdev;
    int ret;
    rain1_group_t *rxg;
    storage_rdev_iter_t iter;

    *private_data = NULL;

    if (su_size == 0 || su_size % 8 != 0)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Invalid su_size (%u KiB). Must be multiple of %"PRIu32" KiB and not null.",
		 SECTORS_2_KBYTES(su_size), SECTORS_2_KBYTES(8));
	return -EINVAL;
    }

    if (su_size < BYTES_TO_SECTORS(EXA_MAX_NODES_NUMBER * METADATA_BLOCK_SIZE))
    {
        /* We won't be able to fit metadatas in a single striping unit, which
         * is bad. Let's inform the user with an informative error message:
         * I can't let you do that, Dave.
         */
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Invalid su_size (%u KiB). It must be greater than or equal to %"PRIu32" KiB.",
		 SECTORS_2_KBYTES(su_size), (EXA_MAX_NODES_NUMBER * METADATA_BLOCK_SIZE)/1024);
	return -EINVAL;
    }

    /* Ensure that the chunk size is a multiple of twice the striping
     * unit size (twice due to replication).
     */
    if (chunk_size % (su_size * 2) != 0)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "The chunk size must be a multiple of twice the striping unit size.");
	return -EINVAL;
    }

    if (dirty_zone_size % su_size != 0)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Invalid dirty_zone_size (%u KiB). Must be multiple of su_size (%u KiB).",
		 SECTORS_2_KBYTES(dirty_zone_size), SECTORS_2_KBYTES(su_size));
	return -EINVAL;
    }

    if (nb_spare != 0)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1, "Rain1 does not handle sparing");
	return -EINVAL;
    }

    if (SECTORS_2_KBYTES(dirty_zone_size) < VRT_MIN_DIRTY_ZONE_SIZE)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Dirty zone size too small (%u KiB). Minimum is %u KiB",
		 SECTORS_2_KBYTES(dirty_zone_size), VRT_MIN_DIRTY_ZONE_SIZE);
	return -EINVAL;
    }

    /* check if su_size is a power of 2 */
    if (! IS_POWER_OF_TWO(su_size))
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Invalid striping unit size (%u KiB). Must be a power of 2.",
		 SECTORS_2_KBYTES(su_size));
	return -EINVAL;
    }

    *private_data = rain1_group_alloc();

    if (*private_data == NULL)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1, "Failed allocating group");
	return -ENOMEM;
    }

    rxg = *private_data;

    rxg->nb_rain1_rdevs = storage_get_num_realdevs(storage);

    storage_rdev_iterator_begin(&iter, storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
        rxg->rain1_rdevs[rdev->index] = rain1_alloc_rdev_layout_data(rdev);
        if (rxg->rain1_rdevs[rdev->index] == NULL)
        {
            storage_rdev_iterator_end(&iter);
            return -ENOMEM;
        }
    }
    storage_rdev_iterator_end(&iter);

    if (slot_width > storage->num_spof_groups)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Slot width is too big. The maximum slot width for this group is %" PRIu32,
		 storage->num_spof_groups);
	return -EINVAL;
    }
    else if (slot_width == 0)
    {
	/* Compute a "good" value for slot_width. Experiments have shown
	 * that striping data over more than 6 disks does not improve
	 * throughput.
         *
         * FIXME: use define value or cltunable
	 */
	slot_width = MIN(storage->num_spof_groups, 6);
    }

    /* configure group */
    rxg->su_size = su_size;
    rxg->max_sectors = su_size;
    rxg->logical_slot_size = slot_width * chunk_size / 2;

    if (rxg->logical_slot_size > UINT32_MAX)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Slot size is too big (>= 2TiB). You should decrease the "
		 "slot_width or chunk_size values.");
        return -EINVAL;
    }
    rxg->dirty_zone_size = dirty_zone_size;
    rxg->blended_stripes = blended_stripes;

    if (DZONE_PER_METADATA_BLOCK < rain1_group_get_dzone_per_slot_count(rxg))
    {
        uint32_t min_dzone_size_sectors = quotient_ceil64(rxg->logical_slot_size,
                                            DZONE_PER_METADATA_BLOCK);
        if (min_dzone_size_sectors % su_size != 0)
        {
            /* Round the minimum to the nearest inferior multiple of su_size */
            min_dzone_size_sectors -= min_dzone_size_sectors % su_size;
            /* And increment it so that it's big enough */
            min_dzone_size_sectors += su_size;
            if (SECTORS_2_KBYTES(min_dzone_size_sectors) % su_size != 0)
                min_dzone_size_sectors += su_size;
        }
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
                    "Dirty zone too small. The minimum is %"PRIu32" KiB.",
                    SECTORS_2_KBYTES(min_dzone_size_sectors));
        return -VRT_ERR_DIRTY_ZONE_TOO_SMALL;
    }

    exalog_debug("nb_realdevs=%u", storage_get_num_realdevs(storage));
    exalog_debug("su_size=%u", rxg->su_size);
    exalog_debug("dirty_zone_size=%u", rxg->dirty_zone_size);

    ag = &rxg->assembly_group;

    /* Init slots */
    ret = assembly_group_setup(ag, slot_width, chunk_size);
    if (ret != EXA_SUCCESS)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Cannot initialize disk group slots: %"PRIu64" slots of %u sectors.",
		 assembly_group_get_max_slots_count(ag, storage),
		 ag->slot_size);
        return -EINVAL;
    }

    return EXA_SUCCESS;
}

/**
 * Start a group.
 *
 * This function gets called when the virtualizer 1) has allocated and
 * initialized the struct vrt_group structure, 2) has received all real
 * devices, checked they are correct, and added them to the
 * struct vrt_group structure. However, the group is not yet publicly
 * visible.
 *
 * @param[in]   group        The group to start
 * @param[out]  private_data The layout's private data
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
static int
rain1_group_start(struct vrt_group *group, const storage_t *storage)
{
    rain1_group_t *rxg;

    rxg = group->layout_data;

    EXA_ASSERT(rxg->nb_rain1_rdevs > 0);

    EXA_ASSERT(rxg->su_size != 0);
    rxg->max_sectors = rxg->su_size;

    /*
     * Pre-allocate pages for resync and rebuild because we cannot allocate
     * them during the recovery or rebuilding
     */
    rxg->sync_job_pool = sync_job_pool_alloc(rain1_group_get_sync_job_blksize(rxg),
                                             rain1_group_get_sync_jobs_count(rxg));
    if (rxg->sync_job_pool == NULL)
	return -ENOMEM;

    return EXA_SUCCESS;
}

static int rain1_group_stop(void *private_data)
{
    rain1_group_t *rxg = private_data;

    EXA_ASSERT(rxg);
    sync_job_pool_free(rxg->sync_job_pool);

    return EXA_SUCCESS;
}

/**
 * Compute and update the group status.
 *
 * @param[in] group	The group
 *
 * @return EXA_SUCCESS on success, a negative error code on failure.
 */
static int
rain1_group_compute_status(struct vrt_group *group)
{
    rain1_group_t *rxg = RAIN1_GROUP(group);
    exa_nodeset_t nodes_up;
    int node;

    exalog_debug("compute status of group '%s'", group->name);

    /* Before calling the function that effectively compute the status,
     * 'they' modify the list of nodes to use for resync and update operations.
     * From my point of view all that stuff of 'nodes_resync' and
     * 'nodes_update' is deadly broken (for 'nodes_update' see comment at the
     * end of rain1_compute_status):
     * - 'nodes_resync' is not initialized when the group is created
     * - all the modifications done on the list are only additions and never
     *   removals.
     *
     * As all the disks must be UP for the group being created, we can assume
     * that as some moment the list contains all the nodes involved in the
     * group. And considering that 'they' only add nodes to the list, the
     * list always contains all the nodes involved in the group.
     *
     * FIXME: Try to figure out what should be the behaviour of
     * 'nodes_resync' and fix it.
     */

    /* old comment : Current state (before the recovery) is offline: we don't
     * have the metadata in memory, so we don't try to write them. Simply
     * recompute the new status and exit.
     */
    if (group->status == EXA_GROUP_OFFLINE)
    {
	rain1_compute_status(group);
	return EXA_SUCCESS;
    }

    /* Mark metadata of all nodes UP as relevent for resync and update */
    vrt_node_get_upnodes(&nodes_up);
    exa_nodeset_foreach(&nodes_up, node)
    {
	exa_nodeset_add(&rxg->nodes_resync, node);
	exa_nodeset_add(&rxg->nodes_update, node);
    }

    rain1_compute_status(group);

    return EXA_SUCCESS;
}

static int rain1_group_post_resync_subspace(rain1_group_t *rxg,
                                            assembly_volume_t *subspace)
{
    uint64_t slot_index;

    for (slot_index = 0; slot_index < subspace->total_slots_count; slot_index++)
    {
        int ret;
        slot_desync_info_t *block;
        slot_t *slot = subspace->slots[slot_index];

        EXA_ASSERT(slot != NULL);

        RAINX_PERF_POST_RESYNC_BEGIN();

        block = slot->private;
        block->flush_needed = false;

        ret = rain1_read_slot_metadata(rxg, slot, vrt_node_get_local_id(),
                                       block->on_disk_metadata);
        if (ret != EXA_SUCCESS)
            return ret;

        memcpy(block->in_memory_metadata, block->on_disk_metadata,
                METADATA_BLOCK_SIZE);

        RAINX_PERF_POST_RESYNC_END();
    }

    RAINX_PERF_POST_RESYNC_FLUSH();

    return EXA_SUCCESS;
}

/**
 * Post-resync operation, which reads the metadata to the disk.
 *
 * @param[in] private_data  The layout data
 *
 * @return EXA_SUCCESS on success, a negative error code on failure.
 */
static int
rain1_group_post_resync (void *private_data)
{
    rain1_group_t *rxg = private_data;
    assembly_volume_t *subspace;

    for (subspace = rxg->assembly_group.subspaces;
         subspace != NULL; subspace = subspace->next)
    {
        int err = rain1_group_post_resync_subspace(rxg, subspace);
        if (err != EXA_SUCCESS)
            return err;
    }

    vrt_node_get_upnodes(&rxg->nodes_resync);

    return EXA_SUCCESS;
}

static void rain1_rdev_reset(const void *group_layout_data,
                             const struct vrt_realdev *rdev)
{
    const rain1_group_t *rxg = group_layout_data;
    struct rain1_realdev *lr = RAIN1_REALDEV(rxg, rdev);
    lr->sync_tag = SYNC_TAG_BLANK;
}

/* FIXME: It seems quite clear that
 *    reintegrate ~ acknowledge end of updating <=> mark rdev uptodate
 *    post-reintegrate ~ acknowledge end of replicating <=> mark spare uptodate
 */
static int rain1_rdev_reintegrate(vrt_group_t *group, vrt_realdev_t *rdev)
{
    rain1_group_t *rxg = group->layout_data;
    struct rain1_realdev *lr = RAIN1_REALDEV(rxg, rdev);
    int ret = EXA_SUCCESS;

    EXA_ASSERT(rxg);
    EXA_ASSERT(lr);

    exalog_debug("Reintegrate rdev %d " UUID_FMT " in group %s (rdev status=%d)",
                 rdev->index, UUID_VAL(&rdev->uuid),
                 group->name, rain1_rdev_get_compound_status(rxg, rdev));

    EXA_ASSERT_VERBOSE(rain1_rdev_is_rebuilding(lr), "Device #%d " UUID_FMT
                       " is not rebuilding", rdev->index, UUID_VAL(&rdev->uuid));

    switch (lr->rebuild_desc.type)
    {
    case EXA_RDEV_REBUILD_UPDATING:
        os_thread_rwlock_wrlock(&rxg->status_lock);

        /* tag the reintegrated devive as uptodate */
        lr->sync_tag = rxg->sync_tag;

        os_thread_rwlock_unlock(&rxg->status_lock);

        EXA_ASSERT(!group->suspended);
        EXA_ASSERT(group->status != EXA_GROUP_OFFLINE);
        break;

    case EXA_RDEV_REBUILD_REPLICATING:
        break;

    case EXA_RDEV_REBUILD_NONE:
        EXA_ASSERT(false);
        break;
    }

    /* Clear device rebuilding context */
    rain1_rdev_clear_rebuild_context(lr);

    return ret;
}

static int rain1_rdev_post_reintegrate(vrt_group_t *group, vrt_realdev_t *rdev)
{
    rain1_group_t *rxg = RAIN1_GROUP(group);
    struct rain1_realdev *lr = RAIN1_REALDEV(rxg, rdev);
    int ret = EXA_SUCCESS;

    EXA_ASSERT(rxg);
    EXA_ASSERT(lr);

    exalog_debug("Post reintegrate rdev #%d " UUID_FMT ".",
	   rdev->index, UUID_VAL(&rdev->uuid));

    os_thread_rwlock_wrlock(&rxg->status_lock);

    /* do not compute the status if we are not the last end of rebuilding */
    if (rain1_group_is_rebuilding(rxg))
    {
	os_thread_rwlock_unlock(&rxg->status_lock);
	return EXA_SUCCESS;
    }

    exalog_info("Rebuilding of group '%s' is finished on device "UUID_FMT,
                group->name, UUID_VAL(&rdev->uuid));
    rain1_rebuild_finish(rxg);

    os_thread_rwlock_wrlock(&group->status_lock);
    rain1_compute_status(group);
    os_thread_rwlock_unlock(&group->status_lock);

    os_thread_rwlock_unlock(&rxg->status_lock);

    /* wake up the rebuilding thread in case there is something to rebuild */
    vrt_group_rebuild_thread_resume(group);

    return ret;
}

static int rain1_rdev_get_reintegrate_info(const void *layout_data,
                                           const struct vrt_realdev *rdev,
					   bool *need_reintegrate)
{
    const rain1_group_t *rxg = layout_data;
    struct rain1_realdev *lr = RAIN1_REALDEV(rxg, rdev);
    EXA_ASSERT(lr);
    EXA_ASSERT(need_reintegrate);

    os_thread_mutex_lock(&lr->rebuild_progress.lock);

    *need_reintegrate = rain1_rdev_is_rebuilding(lr) && lr->rebuild_progress.complete;

    os_thread_mutex_unlock(&lr->rebuild_progress.lock);

    return EXA_SUCCESS;
}

static int rain1_rdev_get_rebuild_info(const void *layout_data,
                                       const struct vrt_realdev *rdev,
                                       uint64_t *logical_rebuilt_size,
                                       uint64_t *logical_size_to_rebuild)
{
    const rain1_group_t *rxg = layout_data;
    rain1_realdev_t *lr = RAIN1_REALDEV(rxg, rdev);

    /* XXX obviously, clinfo is crap and needs the values to be reset when
     * rebuild is finished, even if the remaining size to rebuild is 0... */
    if (lr->rebuild_desc.type == EXA_RDEV_REBUILD_NONE)
        *logical_size_to_rebuild = *logical_rebuilt_size = 0;
    else
    {
        uint64_t slots_used_by_subspaces = 0;
        const assembly_volume_t *s;

        for (s = rxg->assembly_group.subspaces; s != NULL; s = s->next)
            slots_used_by_subspaces += s->total_slots_count;

        os_thread_mutex_lock(&lr->rebuild_progress.lock);

        *logical_rebuilt_size = lr->rebuild_progress.nb_slots_rebuilt
                                * rain1_group_get_slot_data_size(rxg);
        *logical_size_to_rebuild = slots_used_by_subspaces
                                   * rain1_group_get_slot_data_size(rxg);

        os_thread_mutex_unlock(&lr->rebuild_progress.lock);
    }
    return EXA_SUCCESS;
}

static uint32_t rain1_get_su_size(const void *private_data)
{
    const rain1_group_t *rxg = private_data;
    EXA_ASSERT(rxg);
    return rxg->su_size;
}

static uint32_t rain1_get_dirty_zone_size(const void *private_data)
{
    const rain1_group_t *rxg = private_data;
    EXA_ASSERT(rxg);

    return rxg->dirty_zone_size;
}

static uint32_t rain1_get_blended_stripes(const void *private_data)
{
    const rain1_group_t *rxg = private_data;
    EXA_ASSERT(rxg);

    return rxg->blended_stripes;
}

/**
 * @return -VRT_ERR_PREVENT_GROUP_OFFLINE if the group will go offline, or
 *         EXA_SUCCESS if the group will not go offline, or
 *         a negative error code on failure
 */
static int
rain1_group_going_offline(const struct vrt_group *group, const exa_nodeset_t *stop_nodes)
{
    size_t nb_not_corrected_spofs;
    size_t i;

    /* Compute the total number of SPOF groups that are not corrected
     * or will not be corrected if the nodes described by "stop_nodes"
     * are stopped.
     *
     * FIXME: corrected is not the right term IMHO
     */
    nb_not_corrected_spofs = 0;
    for (i = 0; i < group->storage->num_spof_groups; i++)
    {
        const spof_group_t *spof_group = &group->storage->spof_groups[i];
        exa_nodeset_t spof_nodes;

        spof_group_get_nodes(spof_group, &spof_nodes);

        /* REMOVE_ME */
        {
            char spof_str[EXA_MAX_NODES_NUMBER + 1];
            char stop_str[EXA_MAX_NODES_NUMBER + 1];

            exa_nodeset_to_bin(&spof_nodes, spof_str);
            exa_nodeset_to_bin(stop_nodes, stop_str);

            exalog_debug("spof group %"PRIspof_id": %s, to stop: %s, disjoint? %d",
                         spof_group->spof_id, spof_str, stop_str,
                         exa_nodeset_disjoint(&spof_nodes, stop_nodes));
        }

        if (rain1_spof_group_has_defect(RAIN1_GROUP(group), spof_group)
            || !exa_nodeset_disjoint(&spof_nodes, stop_nodes))
        {
	    nb_not_corrected_spofs++;
        }
    }

    if (nb_not_corrected_spofs <= 1)
        return EXA_SUCCESS;
    else
        return -VRT_ERR_PREVENT_GROUP_OFFLINE;
}

static uint64_t rain1_get_group_total_capacity(const void *private_data,
                                               const storage_t *storage)
{
    const rain1_group_t *lg = private_data;

    return SECTORS_TO_BYTES(assembly_group_get_max_slots_count(&lg->assembly_group,
                                                               storage)
                            * rain1_group_get_slot_data_size(lg));
}

static uint64_t rain1_get_group_used_capacity(const void *private_data)
{
    const rain1_group_t *lg = private_data;
    const struct assembly_group *ag = &lg->assembly_group;

    return SECTORS_TO_BYTES(assembly_group_get_used_slots_count(ag)
                            * rain1_group_get_slot_data_size(lg));
}

static uint32_t rain1_get_slot_width(const void *private_data)
{
    const rain1_group_t *lg = private_data;
    const struct assembly_group *ag = &lg->assembly_group;

    return ag->slot_width;
}

static int __rain1_serialize(const void *private_data, stream_t *stream)
{
    return rain1_group_serialize((rain1_group_t *)private_data, stream);
}

static int __rain1_deserialize(void **private_data, const storage_t *storage, stream_t *stream)
{
    return rain1_group_deserialize((rain1_group_t **)private_data, storage, stream);
}

static uint64_t __rain1_serialized_size(const void *private_data)
{
    return rain1_group_serialized_size((rain1_group_t *)private_data);
}

static const assembly_group_t *rain1_get_assembly_group(const void *private_data)
{
    const rain1_group_t *rxg = private_data;
    return &rxg->assembly_group;
}

static bool rain1_layout_data_equals(const void *private_data1,
                                     const void *private_data2)
{
    const rain1_group_t *rxg1 = private_data1;
    const rain1_group_t *rxg2 = private_data2;

    return rain1_group_equals(rxg1, rxg2);
}

/**
 * FIXME: that only works if rdev->index'es are not sparse
 */
static int rain1_group_insert_rdev(void *private_data, vrt_realdev_t *rdev)
{
    rain1_group_t *rxg = private_data;
    struct rain1_realdev *lr;

    lr = rain1_alloc_rdev_layout_data(rdev);
    if (lr == NULL)
        return -ENOMEM;

    /* Nothing yet on disk, thus it is uptodate */
    lr->sync_tag = rxg->sync_tag;

    rxg->rain1_rdevs[rdev->index] = lr;
    rxg->nb_rain1_rdevs++;

    return 0;
}

static struct vrt_layout layout_rain1 =
{
    .list =                          LIST_HEAD_INIT(layout_rain1.list),
    .name =                          RAIN1_NAME,
    .group_create =                  rain1_group_create,
    .group_start =                   rain1_group_start,
    .group_stop =                    rain1_group_stop,
    .group_cleanup =                 rain1_group_cleanup,
    .group_compute_status =          rain1_group_compute_status,
    .group_resync =                  rain1_group_resync,
    .group_post_resync =             rain1_group_post_resync,
    .serialize =                     __rain1_serialize,
    .deserialize =                   __rain1_deserialize,
    .serialized_size =               __rain1_serialized_size,
    .get_assembly_group =            rain1_get_assembly_group,
    .layout_data_equals =            rain1_layout_data_equals,
    .group_metadata_flush_step =          rain1_group_metadata_flush_step,
    .group_metadata_flush_context_alloc = rain1_group_metadata_flush_context_alloc,
    .group_metadata_flush_context_free =  rain1_group_metadata_flush_context_free,
    .group_metadata_flush_context_reset = rain1_group_metadata_flush_context_reset,
    .group_going_offline =           rain1_group_going_offline,
    .group_reset =                   rain1_group_reset,
    .group_check =                   rain1_group_check,
    .create_subspace =               __rain1_create_subspace,
    .delete_subspace =               __rain1_delete_subspace,
    .volume_resize =                 rain1_volume_resize,
    .volume_get_status =             rain1_volume_get_status,
    .volume_get_size =               rain1_volume_get_size,
    .group_rebuild_step =            rain1_group_rebuild_step,
    .group_rebuild_context_alloc =   rain1_group_rebuild_context_alloc,
    .group_rebuild_context_free =    rain1_group_rebuild_context_free,
    .group_rebuild_context_reset =   rain1_group_rebuild_context_reset,
    .group_is_rebuilding =           rain1_group_is_rebuilding,
    .build_io_for_req =              rain1_build_io_for_req,
    .init_req =                      rain1_init_req,
    .cancel_req =                    rain1_cancel_req,
    .declare_io_needs =              rain1_declare_io_needs,
    .group_rdev_down =               NULL,
    .group_rdev_up =                 NULL,
    .group_insert_rdev =             rain1_group_insert_rdev,
    .rdev_reset =                    rain1_rdev_reset,
    .rdev_reintegrate =              rain1_rdev_reintegrate,
    .rdev_post_reintegrate =         rain1_rdev_post_reintegrate,
    .rdev_get_reintegrate_info =     rain1_rdev_get_reintegrate_info,
    .rdev_get_rebuild_info =         rain1_rdev_get_rebuild_info,
    .rdev_get_compound_status =      rain1_rdev_get_compound_status,
    .get_slot_width =                rain1_get_slot_width,
    .get_nb_spare =                  NULL,
    .get_su_size =                   rain1_get_su_size,
    .get_dirty_zone_size =           rain1_get_dirty_zone_size,
    .get_blended_stripes =           rain1_get_blended_stripes,
    .get_group_total_capacity =      rain1_get_group_total_capacity,
    .get_group_used_capacity  =      rain1_get_group_used_capacity

};

int rain1_init(int rebuilding_slowdown_ms,
               int degraded_rebuilding_slowdown_ms)
{
    rain1_set_rebuilding_slowdown(rebuilding_slowdown_ms,
                                  degraded_rebuilding_slowdown_ms);
    return vrt_register_layout(&layout_rain1);
}

void rain1_cleanup(void)
{
    vrt_unregister_layout(&layout_rain1);
}
