/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h> /* for memcpy */

#include "vrt/layout/rain1/src/lay_rain1_metadata.h"
#include "vrt/layout/rain1/src/lay_rain1_request.h"
#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_striping.h"

#include "vrt/virtualiseur/include/vrt_nodes.h"
#include "vrt/virtualiseur/include/vrt_request.h"

#include "log/include/log.h"

#include "common/include/exa_error.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"

int rain1_write_slot_metadata(const rain1_group_t *rxg,
                              const slot_t *slot,
                              unsigned int node_index,
                              const desync_info_t *metadatas)
{
    unsigned int i, step;
    unsigned int nb_write = 0;
    struct rdev_location rdev_loc[3];
    unsigned int nb_rdev_loc;
    bool doing_uptodate;

    EXA_ASSERT(metadatas != NULL);
    EXA_ASSERT(slot != NULL);

    /* Find the physical locations of the metadata block */
    rain1_dzone2rdev(rxg, slot, node_index, rdev_loc, &nb_rdev_loc, 3);

    for (step = 0; step < 2; step++)
    {
        /* We start writing all the uptodate locations, then all the
         * outdated ones. */
        doing_uptodate = step == 0;

        for (i = 0; i < nb_rdev_loc; i++)
        {
            int err;

            if (rdev_loc[i].uptodate != doing_uptodate)
                continue;

	    nb_write++;

            /* FIXME is this smart to write synchronously ?
             * is this smart not to flush cache ?
             */
            err = blockdevice_write(rdev_loc[i].rdev->blockdevice,
                                    (void *)metadatas, METADATA_BLOCK_SIZE,
                                    rdev_loc[i].sector);
            if (err != 0)
                return err;
        }
    }

    if (nb_write == 0)
	return -EIO;

    return EXA_SUCCESS;
}

int rain1_wipe_slot_metadata(const rain1_group_t *rxg, const slot_t *slot)
{
    unsigned int node_index;
    void *buffer = NULL;
    unsigned int slot_metadata_size = EXA_MAX_NODES_NUMBER * METADATA_BLOCK_SIZE;
    unsigned int i;
    struct rdev_location rdev_loc[3];
    unsigned int nb_rdev_loc;

    EXA_ASSERT(slot != NULL);
    EXA_ASSERT(rxg->max_sectors >= BYTES_TO_SECTORS(slot_metadata_size));

    buffer = os_malloc(slot_metadata_size);
    if (buffer == NULL)
        return -ENOMEM;

    for (node_index = 0; node_index < EXA_MAX_NODES_NUMBER; node_index++)
    {
        desync_info_t *node_metadata =
            (desync_info_t *)((char *)buffer + node_index * METADATA_BLOCK_SIZE);
        dzone_metadata_block_init(node_metadata, SYNC_TAG_BLANK);
    }

    /* Find the physical locations of the first metadata block */
    rain1_dzone2rdev(rxg, slot, 0, rdev_loc, &nb_rdev_loc, 3);

    EXA_ASSERT(nb_rdev_loc > 0);

    /* initialize bios */
    for (i = 0; i < nb_rdev_loc; i++)
    {
        int err = blockdevice_write(rdev_loc[i].rdev->blockdevice, buffer,
                                    slot_metadata_size, rdev_loc[i].sector);
        if (err != 0)
        {
            os_free(buffer);
            return err;
        }
    }

    os_free(buffer);

    return EXA_SUCCESS;
}

int rain1_read_slot_metadata (const rain1_group_t *rxg,
                              const slot_t *slot,
                              unsigned int node_index,
                              desync_info_t *metadatas)
{
    struct rdev_location rdev_loc[3];
    unsigned int nb_rdev_loc;
    unsigned int src;

    EXA_ASSERT(metadatas != NULL);
    EXA_ASSERT(slot != NULL);

    /* Find the physical locations of the metadata block */
    rain1_dzone2rdev(rxg, slot, node_index, rdev_loc, &nb_rdev_loc, 3);

    /* Find a readable replica */
    for (src = 0; src < nb_rdev_loc; src++)
	if (rain1_rdev_location_readable(&rdev_loc[src]))
	    break;

    if (src == nb_rdev_loc)
    {
	exalog_error("Metadata block corresponding to node %u"
                     " is not readable", node_index);
	return -EXA_ERR_DEFAULT;
    }

    /* Read the metadata */

    return blockdevice_read(rdev_loc[src].rdev->blockdevice, metadatas,
                           METADATA_BLOCK_SIZE, rdev_loc[src].sector);
}
/**
 * Flush to disk (metadata logical space) the dirty zone metadata of the slot
 *
 * @param[in]    rxg    The rain1 group
 * @param[in]    slot   The slot
 * @param[inout] block  The slot metadata manipulation structure
 *
 * @return EXA_SUCCESS or a negative error code
 */
static int rain1_group_flush_slot_metadata(const rain1_group_t *rxg,
                                           const slot_t *slot,
                                           slot_desync_info_t *block)
{
    bool on_disk_is_synchronized;
    unsigned int dzone;
    desync_info_t metadatas[DZONE_PER_METADATA_BLOCK];
    int ret;

    os_thread_mutex_lock(&block->lock);

    /* The background flushing is just a best-effort optimization.
     * If the IO path is already synchronizing the metadata, we don't do anything.
     */
    if (!block->flush_needed || block->ongoing_flush)
    {
        os_thread_mutex_unlock(&block->lock);
        return EXA_SUCCESS;
    }

    /* compare "in_memory" and "on_disk" version to see if the flush is
     * REALLY needed */
    on_disk_is_synchronized = true;
    for (dzone = 0; dzone < rain1_group_get_dzone_per_slot_count(rxg); dzone++)
    {
        uint16_t in_memory_wpc = block->in_memory_metadata[dzone].write_pending_counter;
        uint16_t on_disk_wpc = block->on_disk_metadata[dzone].write_pending_counter;
        sync_tag_t in_memory_tag = block->in_memory_metadata[dzone].sync_tag;
        sync_tag_t on_disk_tag = block->on_disk_metadata[dzone].sync_tag;

        EXA_ASSERT(desync_info_is_valid(&block->in_memory_metadata[dzone],
                                        rxg->sync_tag));
        EXA_ASSERT(desync_info_is_valid(&block->on_disk_metadata[dzone],
                                        rxg->sync_tag));

        /* The only "expected" desynchronization here is the on_disk version
         * being tagged "pending wite" while the in_memory version is not.
         */
        if (in_memory_wpc == 0 && on_disk_wpc > 0
            && in_memory_tag == on_disk_tag)
        {
            on_disk_is_synchronized = false;
            break;
        }
        else if ((in_memory_wpc > 0 && on_disk_wpc == 0)
                 || in_memory_tag != on_disk_tag)
        {
            exalog_warning("Dirty zones in-memory (pending=%"PRIu16"/tag=%"PRIu16")"
                           "are not correctly synchronized with the dirty zones "
                           "on-disk(pending=%"PRIu16"/tag=%"PRIu16") "
                           "-> forcing synchronization",
                           in_memory_wpc, in_memory_tag, on_disk_wpc, on_disk_tag);

            on_disk_is_synchronized = false;
            break;
        }
    }

    if (on_disk_is_synchronized)
    {
        block->flush_needed = false;
        os_thread_mutex_unlock(&block->lock);
        return EXA_SUCCESS;
    }

    block->ongoing_flush = true;

    memcpy(metadatas, block->in_memory_metadata, sizeof(metadatas));

    os_thread_mutex_unlock(&block->lock);

    /* Write the metadata block to disk (blocking) */
    ret = rain1_write_slot_metadata(rxg, slot, vrt_node_get_local_id(), metadatas);

    os_thread_mutex_lock(&block->lock);

    EXA_ASSERT(block->ongoing_flush);

    /* The "in_memory" and "on_disk" versions of the metadata are now
       synchronized as the "in_memory" version of metadata do not change
       during the metadata writing. */
    if (ret == EXA_SUCCESS)
    {
        memcpy(block->on_disk_metadata, metadatas, sizeof(metadatas));
        block->flush_needed = false;
    }

    /* This function may change the "ongoing_flush" status */
    rain1_schedule_aggregated_metadata(block, ret != EXA_SUCCESS);

    os_thread_mutex_unlock(&block->lock);

    return ret;
}

typedef struct
{
    exa_uuid_t current_subspace_uuid;
    uint64_t current_slot_index;
} rain1_metadata_flush_context_t;

void *rain1_group_metadata_flush_context_alloc(void *layout_data)
{
    rain1_metadata_flush_context_t *context = os_malloc(
            sizeof(rain1_metadata_flush_context_t));

    if (context == NULL)
        return NULL;

    rain1_group_metadata_flush_context_reset(context);
    return context;
}

void rain1_group_metadata_flush_context_free(void *context)
{
    os_free(context);
}

void rain1_group_metadata_flush_context_reset(void *context)
{
    rain1_metadata_flush_context_t *ctx = context;
    uuid_copy(&ctx->current_subspace_uuid, &exa_uuid_zero);
    ctx->current_slot_index = 0;
}

int rain1_group_metadata_flush_step(void *private_data, void *context,
                                    bool *more_work)
{
    rain1_group_t *rxg = (rain1_group_t *)private_data;
    rain1_metadata_flush_context_t *ctx = context;
    assembly_volume_t *subspace = NULL;
    uint64_t slot_index;
    const slot_t *slot;
    slot_desync_info_t *block;
    int err;

    subspace = assembly_group_lookup_volume(&rxg->assembly_group,
                                            &ctx->current_subspace_uuid);

    *more_work = false;

    if (subspace == NULL)
        /* Start at the start of the first subspace */
        subspace = rxg->assembly_group.subspaces;
    else
        /* Continue at the next slot */
        ctx->current_slot_index++;

    if (subspace == NULL)
        /* Nothing to do. */
        return EXA_SUCCESS;

    slot_index = ctx->current_slot_index;

    if (slot_index >= subspace->total_slots_count)
    {
        /* We finished the current subspace. */
        subspace = subspace->next;
        if (subspace == NULL)
            /* We're done. */
            return EXA_SUCCESS;

        /* Start working on the next subspace. */
        slot_index = 0;
        ctx->current_slot_index = 0;
    }

    *more_work = true;

    uuid_copy(&ctx->current_subspace_uuid, &subspace->uuid);

    slot = subspace->slots[slot_index];
    EXA_ASSERT(slot != NULL);

    block = slot->private;

    err = rain1_group_flush_slot_metadata(rxg, slot, block);

    /* wake up the thread to build the woken up requests */
    vrt_thread_wakeup();

    return err;
}

