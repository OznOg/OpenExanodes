/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h> /* for memset */

#include "common/include/exa_error.h"
#include "common/include/exa_env.h"
#include "common/include/uuid.h"

#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_error.h"

#include "log/include/log.h"

#include "vrt/virtualiseur/include/vrt_nodes.h" /* for vrt_node_get_upnodes_xxx */
#include "vrt/common/include/file_stream.h"

#include "vrt/layout/rain1/src/lay_rain1_sync.h"

#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_status.h"
#include "vrt/layout/rain1/src/lay_rain1_striping.h"
#include "vrt/layout/rain1/src/lay_rain1_sync_tag.h"
#include "vrt/layout/rain1/src/lay_rain1_sync_job.h"
#include "vrt/layout/rain1/src/lay_rain1_metadata.h"

#include "vrt/virtualiseur/include/vrt_perf.h"

typedef uint8_t mblock_bitfield_t[ALIGN_SUP(DZONE_PER_METADATA_BLOCK, 8, uint8_t) / 8];

#define MBLOCK_SET_BIT(bit_field, index)					\
    do { (bit_field)[(index) / 8] |= 1 << ((index) % 8) ; } while (0)
#define MBLOCK_GET_BIT(bit_field, index)			\
    ((bit_field)[(index) / 8] & (1 << ((index) % 8)))

typedef struct
{
    exa_uuid_t rdev_uuid;
    mblock_bitfield_t dzone_to_rebuild;
} rdev_sync_context_t;

typedef struct {
    /** Per-rdev dirty zones to be used during rebuild */
    /* FIXME: As in fact we deal with the 'intesection' of rdevs with the slot
     * it should be chunk_context. */
    rdev_sync_context_t rdev_context[NBMAX_DISKS_PER_GROUP];
    int num_rdevs;
} rebuild_data_t;

typedef struct {
    bool first_call;
    uint64_t next_su;
    uint64_t next_part;
} slot_sync_context_t;

static int rebuilding_slowdown_ms = 0;
static int degraded_rebuilding_slowdown_ms = 0;

void rain1_set_rebuilding_slowdown(int _rebuilding_slowdown_ms,
                                   int _degraded_rebuilding_slowdown_ms)
{
    rebuilding_slowdown_ms = _rebuilding_slowdown_ms;
    degraded_rebuilding_slowdown_ms = _degraded_rebuilding_slowdown_ms;
}

static rdev_sync_context_t *rebuild_data_get_rdev_context(
                                rebuild_data_t *rebuild_data,
                                const exa_uuid_t *uuid)
{
    int i = 0;
    for (i = 0; i < rebuild_data->num_rdevs; i++)
        if (uuid_is_equal(uuid, &rebuild_data->rdev_context[i].rdev_uuid))
            return &rebuild_data->rdev_context[i];

    EXA_ASSERT_VERBOSE(0, "didn't find rdev "UUID_FMT, UUID_VAL(uuid));
    return NULL;
}

static bool su_needs_rebuild(rain1_group_t *rxg,
                             rebuild_data_t *rebuild_data,
                             const slot_t *slot,
                             uint64_t logical_su_in_slot)
{
    struct rdev_location rdev_loc[3];
    unsigned int nb_rdev_loc, i;

    uint64_t sector = logical_su_in_slot * rxg->su_size;
    uint64_t slot_metadata_size = rain1_group_get_slot_metadata_size(rxg);

    rain1_slot_raw2rdev(rxg, slot, sector, rdev_loc, &nb_rdev_loc, 3);

    if (nb_rdev_loc < 2)
        return false;

    for (i = 0; i < nb_rdev_loc; i++)
    {
        struct vrt_realdev *rdev = rdev_loc[i].rdev;
        struct rain1_realdev *lr = RAIN1_REALDEV(rxg, rdev);
        if (lr->mine && rain1_rdev_is_rebuilding(lr)
            && !rdev_loc[i].uptodate)
        {
            rdev_sync_context_t *rdev_ctx =
                    rebuild_data_get_rdev_context(rebuild_data, &lr->uuid);
            uint64_t dzone_index;

            EXA_ASSERT(!lr->rebuild_progress.complete);

            /* always rebuild the metadata zones */
            if (sector < slot_metadata_size)
                return true;

            /* the dirty zones are computed based on the data space */
            dzone_index = (sector - slot_metadata_size) / rxg->dirty_zone_size;
            if (MBLOCK_GET_BIT(rdev_ctx->dzone_to_rebuild, dzone_index))
                return true;

            if (rain1_rdev_location_update_needed(&rdev_loc[i]))
                return true;
        }
    }

    return false;
}

static bool su_needs_resync(rain1_group_t *rxg,
                            const mblock_bitfield_t *dzone_to_resync,
                            const slot_t *slot,
                            uint64_t logical_su_in_slot)
{
    struct rdev_location rdev_loc[3];
    unsigned int nb_rdev_loc;

    uint64_t sector = logical_su_in_slot * rxg->su_size;
    uint64_t dzone_index;
    uint64_t slot_metadata_size = rain1_group_get_slot_metadata_size(rxg);

    rain1_slot_raw2rdev(rxg, slot, sector, rdev_loc, &nb_rdev_loc, 3);

    if (nb_rdev_loc < 2)
        return false;

    /* never resync the metadata zones
     *
     * Considering that the dz metadatas are written before the data, if it was
     * being written (possible metadata desynchronization), it means that the
     * data were synchronized (no replica yet written) and so we don't care if
     * the dz metadata are synchronized or not.
     *
     * Rq: when all the metadatas were in dedicated slots these slots were
     * resynced just like the others and considering that their own metadata
     * where never modified, they were not resynced.
     */
    if (sector < slot_metadata_size)
        return false;

    /* the dirty zones are computed based on the data space */
    dzone_index = (sector - slot_metadata_size) / rxg->dirty_zone_size;

    return MBLOCK_GET_BIT(*dzone_to_resync, dzone_index);
}


 /**
  * Find the next striping unit and part of striping unit that must be syncronized.
  * If blksize is smaller than the striping unit size, then we need several steps
  * to copy a single striping units. That's what 'next_part' is for.
  *
  * @param[in] rxg           The layout data
  * @param[in] slot          The slot
  * @param[in] rebuild       true if rebuilding, false if resyncing
  * @param[in] blksize       The size of the blocks used to make the
  *                          rebuilding or resync
  * @param[in:out] ctx       synchronization context to track progression.
  *
  * Rq: The input values next_su/next_part are relevant only if the condition
  *     first_call is false.
  *
  * @return true if it found a su/part to synchronize
  */
static bool get_next_su_and_part_to_sync(rain1_group_t *rxg,
                                         const slot_t *slot,
                                         bool rebuild, void *data,
                                         unsigned int blksize,
                                         slot_sync_context_t *ctx)
{
    unsigned int i;

    EXA_ASSERT(rxg->su_size % blksize == 0);

    /* First we look if there is more resync to do on current su
     * (if this is the first job there is nothing to continue)
     */
    if (!ctx->first_call && ctx->next_part + 1 < rxg->su_size / blksize)
    {
        ctx->next_part++;
        return true;
    }

    for (i = ctx->first_call ? 0 : ctx->next_su + 1;
         i < rxg->logical_slot_size / rxg->su_size; i++)
    {
        if ((rebuild && su_needs_rebuild(rxg, data, slot, i)) ||
            (!rebuild && su_needs_resync(rxg, data, slot, i)))
        {
            ctx->next_su    = i;
            ctx->next_part  = 0;
            ctx->first_call = false;
            return true;
        }
    }

    return false;
}

void rain1_slot_sync_context_init(slot_sync_context_t *ctx)
{
    ctx->first_call = true;
    ctx->next_part  = 0;
    ctx->next_su    = 0;
}

/**
 * Synchronize the real devices of DZONE_PER_METADATA_BLOCK dirty zones. This
 * function is used for 3 different types of synchronization:
 *
 * - resync, during which we have to synchronize an uptodate replica with all
 *   other replicas of the same striping unit. Such a synchronization occurs
 *   when dest_rdev is NULL.
 *
 * - update, during which we have to synchronize the full contents of a
 *   specific real device's dirty zone using other uptodate replicas. Such a a
 *   synchronization occurs when dest_rdev is not NULL, and in that case
 *   dest_rdev is the device to be updated.
 *
 * - replicate, during which we have to synchronize the contents of the spare
 *   space of a specific real device's dirty zone using other uptodate
 *   replicas. Such a a synchronization occurs when dest_rdev is not NULL, and
 *   in that case dest_rdev is the device to be updated.
 *
 * The code for update and replicate is exactly the same. It's simply that the
 * rain1_group2rdev() function will return rdev locations that are not uptodate
 * for the complete dirty zone in the case of update, while it will only return
 * rdev locations that are not uptodate for spare spaces in the case of
 * replicate.
 *
 * The caller specify if the dzones are dirty. If a dirty zone is dirty, all
 * its su will be synchronized. If not, only su tagged as "never replicated"
 * will be synchronized.
 *
 * @param[in] slot         Index of the slot to synchronize
 * @param[in] lg           The rain1 group data
 * @param[in] rebuild      Is the synchronization triggered by a rebuilding
 *                         (if not it means it has been triggered by a resync)
 * @param[in] slowdown_ms  Amount of ms to sleep when a job finishes.
 * @param[in] data         Opaque data passed to rebuild or resync.
 *
 * @return EXA_SUCESS on success, a negative error code on failure
 */
static int synchronize_slot(const slot_t *slot, rain1_group_t *lg,
                            bool rebuild, int slowdown_ms, void *data)
{
    slot_sync_context_t ctx;
    unsigned int njobs, blksize;
    sync_job_t *jobs;
    int error = EXA_SUCCESS;
    int i;

    njobs   = lg->sync_job_pool->nb_jobs;
    blksize = lg->sync_job_pool->block_size;
    jobs    = lg->sync_job_pool->jobs;

    EXA_ASSERT(jobs != NULL && njobs > 0);

    /* Initialize the jobs */
    sync_job_pool_init(lg->sync_job_pool);

    rain1_slot_sync_context_init(&ctx);

    while (sync_job_pool_is_any_active(lg->sync_job_pool))
    {
        sync_job_t *job = NULL;

  	for (i = 0; i < njobs; i++)
 	    if (jobs[i].completed && jobs[i].active)
 		break;

        /* If no job has finished a step, wait for completion */
        if (i == njobs)
        {
            sync_job_pool_wait_step_completion(lg->sync_job_pool);
            continue;
        }

        job = &jobs[i];

        EXA_ASSERT(SYNC_JOB_STEP_IS_VALID(job->step));
        switch(job->step)
        {
        case SYNC_JOB_IDLE:
            /* Look for some synchronization work to do */
            if (!get_next_su_and_part_to_sync(lg, slot, rebuild, data,
                                              blksize, &ctx))
            {
                exalog_debug("Job %i is no more active", i);
                job->error  = EXA_SUCCESS;
                job->active = false;
                break;
            }

            sync_job_prepare(job, lg, rebuild, slot, ctx.next_su, ctx.next_part);

            if (rebuild)
                if (sync_job_lock(job, blksize) != EXA_SUCCESS)
                    break;

            exalog_debug("Job %i is reading su=%"PRIu64" and part=%"PRIu64,
                         i, job->su, job->part);

            sync_job_read(job, blksize);
            break;

        case SYNC_JOB_WRITE:
            exalog_debug("Job %i is writing su=%"PRIu64" and part=%"PRIu64,
                         i, job->su, job->part);
            sync_job_write(job, blksize);
            break;

        case SYNC_JOB_UNLOCK:
            exalog_debug("Job %i complete treatment of su=%"PRIu64" and part=%"PRIu64,
                         i, ctx.next_su, ctx.next_part);

            if (rebuild)
                if (sync_job_unlock(job, blksize) != EXA_SUCCESS)
                    break;

            if (slowdown_ms != 0)
                os_millisleep(slowdown_ms);

            job->step = SYNC_JOB_IDLE;
            break;
        }
    }

    for (i = 0; i < njobs; i++)
        if (jobs[i].error != EXA_SUCCESS)
        {
            error = jobs[i].error;
            exalog_debug("Job %i finished with error %s (%d)",
                         i, exa_error_msg(error), error);
        }

    sync_job_pool_clear(lg->sync_job_pool);

    return error;
}

/**
 * Resync a slot.
 *
 * @param[in] slot    Slot to resync.
 * @param[in] lg      RainX group data
 * @param[in] nodes   Nodes whose pending writes zones must be resynched
 *
 * return EXA_SUCCESS or a negative error code in case of failure.
 */
static int slot_resync(const slot_t *slot, rain1_group_t *lg,
                       const exa_nodeset_t *nodes)
{
    mblock_bitfield_t dzone_to_resync;
    uint node, j;
    int err;
    bool do_resync = false;

    desync_info_t merged_metadatas[DZONE_PER_METADATA_BLOCK];

    dzone_metadata_block_init(merged_metadatas, SYNC_TAG_BLANK);

    exa_nodeset_foreach(nodes, node)
    {
        desync_info_t metadata[DZONE_PER_METADATA_BLOCK];

        err = rain1_read_slot_metadata(lg, slot, node, metadata);
        if (err != EXA_SUCCESS)
            return err;

        dzone_metadata_block_merge(merged_metadatas, metadata);
    }

    /* Iterate over each dirty zone of the slot to find those that were
     * being written and need to be synchronized */
    memset(dzone_to_resync, 0x0, sizeof(dzone_to_resync));

    /* We dimension merged_metadatas using DZONE_PER_METADATA_BLOCK,
     * then access up to rain1_group_get_dzone_per_slot_count(lg) elements.
     * Es kann nicht merdieren.
     */
    EXA_ASSERT(DZONE_PER_METADATA_BLOCK >= rain1_group_get_dzone_per_slot_count(lg));

    for (j = 0; j < rain1_group_get_dzone_per_slot_count(lg); j++)
    {
        EXA_ASSERT(desync_info_is_valid(&merged_metadatas[j], lg->sync_tag));

        if (merged_metadatas[j].write_pending_counter != 0)
        {
            MBLOCK_SET_BIT(dzone_to_resync, j);
            do_resync = true;

            /* Now that the dirty zone has been resynced, reset the write
               pending counter. We also set the sync_tag bit if it wasn't
               so that the dirty zone will be rebuilt when the potential
               missing disk will come back. */
            merged_metadatas[j].write_pending_counter = 0;
            merged_metadatas[j].sync_tag = lg->sync_tag;
        }
    }

    if (!do_resync)
        return EXA_SUCCESS;

    err = synchronize_slot(slot, lg, false /* not rebuilding */,
                           0 /* no slowdown */, &dzone_to_resync);
    if (err != EXA_SUCCESS)
        return err;

    exa_nodeset_foreach(nodes, node)
    {
        err = rain1_write_slot_metadata(lg, slot, node, merged_metadatas);
        if (err != EXA_SUCCESS)
            return err;
    }

    return err;
}

static int rain1_group_resync_subspace(rain1_group_t *lg, assembly_volume_t *subspace,
                                       const exa_nodeset_t *nodes)
{
    uint64_t slot_index;

    for (slot_index = 0; slot_index < subspace->total_slots_count; slot_index++)
    {
        const slot_t *slot;
        int err;

	/* Each node handles only some metadata blocks */
	if ((slot_index % vrt_node_get_upnodes_count()) != vrt_node_get_upnode_id())
	    continue;

        slot = subspace->slots[slot_index];
        EXA_ASSERT(slot != NULL);

        exalog_debug("Resync of slot %"PRIu64" in subspace "UUID_FMT,
                     slot_index, UUID_VAL(&subspace->uuid));

        RAINX_PERF_RESYNC_SLOT_BEGIN();

        err = slot_resync(slot, lg, nodes);

        RAINX_PERF_RESYNC_SLOT_END();

        if (err != EXA_SUCCESS)
        {
            exalog_debug("Failed to resync slot %"PRIu64" in subspace "UUID_FMT
                     ": %s (%d)", slot_index, UUID_VAL(&subspace->uuid),
                     exa_error_msg(err), err);
            return err;
        }
    }
    return EXA_SUCCESS;
}

/**
 * Resync after the crash of a given node. It has to be called on a
 * single node.
 *
 * @param[in] group The group to recover.
 *
 * @return EXA_SUCCESS on success, a negative error code on failure.
 */
int rain1_group_resync(struct vrt_group *group, const exa_nodeset_t *nodes)
{
    rain1_group_t *lg = RAIN1_GROUP(group);
    assembly_volume_t *subspace;

    EXA_ASSERT(group->suspended);

    /* Do not try to resync a group in error */
    if (group->status == EXA_GROUP_OFFLINE)
	return -VRT_ERR_GROUP_OFFLINE;

    for (subspace = lg->assembly_group.subspaces; subspace != NULL;
         subspace = subspace->next)
    {
        int err = rain1_group_resync_subspace(lg, subspace, nodes);
        if (err != EXA_SUCCESS)
        {
            exalog_error("Failed to synchronize subspace "UUID_FMT": %s (%d)",
                         UUID_VAL(&subspace->uuid), exa_error_msg(err), err);
            return err;
        }
    }

    RAINX_PERF_RESYNC_SLOT_FLUSH();

    return EXA_SUCCESS;
}

/**
 * For a given slot, read that metadata block for all nodes, and fill the
 * dzone_to_rebuild bitfield. Bits will be set to 1 if the dirty zone must be
 * rebuilt, 0 otherwise.
 *
 * @param[in] lg             The rain1 group
 * @param[in] slot           The slot
 * @param[out] rebuild_data  The rebuilding description
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
static int rain1_group_rebuild_get_dirty_zones(rain1_group_t *lg,
                                               const slot_t *slot,
                                               rebuild_data_t *rebuild_data)
{
    struct rain1_realdev *lr;
    unsigned int node;
    int ret, i;

    foreach_rainx_rdev(lg, lr, i)
    {
	if (lr->mine && rain1_rdev_is_rebuilding(lr))
	{
	    rdev_sync_context_t *rdev_ctx =
                    rebuild_data_get_rdev_context(rebuild_data, &lr->uuid);
	    memset(rdev_ctx->dzone_to_rebuild, 0x0,
                   sizeof(rdev_ctx->dzone_to_rebuild));
	}
    }

    for (node = 0; node < EXA_MAX_NODES_NUMBER; node ++)
    {
	desync_info_t metadata[DZONE_PER_METADATA_BLOCK];
	unsigned int dzone_index;

        /* FIXME: this section of code is common between updating and replicating
         * but this test (if correctly named) seems to be related only to updating.
         */
	if (!exa_nodeset_contains(&lg->nodes_update, node))
	    continue;

        ret = rain1_read_slot_metadata(lg, slot, node, metadata);
        if (ret != EXA_SUCCESS)
            return ret;

	for (dzone_index = 0;
             dzone_index < rain1_group_get_dzone_per_slot_count(lg);
             dzone_index++)
        {
            int i;

            EXA_ASSERT(desync_info_is_valid(&metadata[dzone_index], lg->sync_tag));

            foreach_rainx_rdev(lg, lr, i)
	    {
                rdev_sync_context_t *rdev_ctx = NULL;

		if (!lr->mine)
		    continue;

                rdev_ctx = rebuild_data_get_rdev_context(rebuild_data, &lr->uuid);

		if (rain1_rdev_is_updating(lr))
		{
		    /* Update if the metadata sync_tag asks to do so */
		    if (sync_tag_is_greater(metadata[dzone_index].sync_tag, lr->sync_tag))
			MBLOCK_SET_BIT(rdev_ctx->dzone_to_rebuild, dzone_index);
                }
		else
                {
                    /* Replicate all the dirty zones that were written at least once */
		    if (sync_tag_is_greater(metadata[dzone_index].sync_tag, SYNC_TAG_BLANK))
                        MBLOCK_SET_BIT(rdev_ctx->dzone_to_rebuild, dzone_index);
                }
	    }
        }
    }

    return EXA_SUCCESS;
}

static void update_rebuild_progression(const rain1_group_t *lg,
                                       unsigned int nb_slots_rebuilt)
{
    struct rain1_realdev *lr;
    int i;

    foreach_rainx_rdev(lg, lr, i)
        if (lr->mine && rain1_rdev_is_rebuilding(lr))
	{
            os_thread_mutex_lock(&lr->rebuild_progress.lock);
            lr->rebuild_progress.nb_slots_rebuilt = nb_slots_rebuilt;
            os_thread_mutex_unlock(&lr->rebuild_progress.lock);
  	}
}

/**
 * Initialize a rebuilding data if there a rebuilding is needed.
 * In case of no rebuilding on local disk is needed, the content of the
 * rdev_context_array is not modified and the function return false.
 *
 * @param[out] rebuild_data   data nedeed during rebuilding
 * @param[in]  lg             rain1 group data
 *
 * return true is any device was found to be rebuilt, false is nothing to
 * rebuild.
 */
static bool rdev_context_array_init(rebuild_data_t *rebuild_data,
                                    rain1_group_t *lg)
{
    struct rain1_realdev *lr;
    bool local_rdev_to_rebuild = false;
    int i;

    rebuild_data->num_rdevs = 0;

    /* Look if a local device must be rebuilt */
    foreach_rainx_rdev(lg, lr, i)
    {
	/* Apparently we assert that the rebuilding has not begun */
	if (lr->mine && rain1_rdev_is_rebuilding(lr))
	{
	    local_rdev_to_rebuild = true;
 	}
        uuid_copy(&rebuild_data->rdev_context[i].rdev_uuid, &lr->uuid);
        memset(rebuild_data->rdev_context[i].dzone_to_rebuild,
               0xEE, sizeof(rebuild_data->rdev_context[i].dzone_to_rebuild));
    }
    rebuild_data->num_rdevs = i;

    return local_rdev_to_rebuild;
}

typedef enum {
#define RAIN_REBUILD_STEP__FIRST RAIN1_REBUILD_BEGIN
    RAIN1_REBUILD_BEGIN = 55,
    RAIN1_REBUILD_SLOTS,
    RAIN1_REBUILD_FINISH
#define RAIN_REBUILD_STEP__LAST RAIN1_REBUILD_FINISH
} rain1_rebuild_step_t;

#define RAIN1_REBUILD_STEP_IS_VALID(x) \
    ((x) >= RAIN_REBUILD_STEP__FIRST && (x) <= RAIN_REBUILD_STEP__LAST)

typedef struct {
    vrt_group_t *group;
    exa_uuid_t current_subspace_uuid;
    uint64_t current_slot_index;
    uint64_t nb_slots_synchronized;
    rebuild_data_t rebuild_data;
    rain1_rebuild_step_t step;
    sync_tag_t sync_tag;
} rain1_rebuild_context_t;

void *rain1_group_rebuild_context_alloc(struct vrt_group *group)
{
    rain1_rebuild_context_t *ctx = os_malloc(sizeof(rain1_rebuild_context_t));

    if (ctx == NULL)
        return NULL;

    ctx->group = group;
    rain1_group_rebuild_context_reset(ctx);
    return ctx;
}

void rain1_group_rebuild_context_reset(void *context)
{
    rain1_rebuild_context_t *ctx = context;

    ctx->sync_tag = SYNC_TAG_BLANK;
    uuid_copy(&ctx->current_subspace_uuid, &exa_uuid_zero);
    ctx->current_slot_index = 0;
    ctx->nb_slots_synchronized = 0;
    ctx->step = RAIN1_REBUILD_BEGIN;
}

void rain1_group_rebuild_context_invalidate(void *context)
{
    rain1_rebuild_context_t *ctx = context;

    update_rebuild_progression(RAIN1_GROUP(ctx->group), 0);
    rain1_group_rebuild_context_reset(ctx);
}

void rain1_group_rebuild_context_free(void *context)
{
    os_free(context);
}

static int rain1_group_rebuild_slot(rain1_group_t *rxg, const slot_t *slot,
                                    rain1_rebuild_context_t *ctx)
{
    int slowdown_ms;
    int ret = rain1_group_rebuild_get_dirty_zones(rxg, slot, &ctx->rebuild_data);
    if (ret != EXA_SUCCESS)
	return ret;

    if (ctx->group->status == EXA_GROUP_DEGRADED)
        slowdown_ms = degraded_rebuilding_slowdown_ms;
    else
        slowdown_ms = rebuilding_slowdown_ms;

    return synchronize_slot(slot, rxg, true /* rebuilding */, slowdown_ms,
                           &ctx->rebuild_data);
}

static int rain1_group_rebuild_next_slot(rain1_rebuild_context_t *ctx)
{
    rain1_group_t *lg;
    const slot_t *slot;
    assembly_volume_t *subspace;
    int err;

    EXA_ASSERT(ctx->group != NULL);
    lg = RAIN1_GROUP(ctx->group);

    subspace = assembly_group_lookup_volume(&lg->assembly_group,
                                            &ctx->current_subspace_uuid);

    if (subspace == NULL)
        /* Start at the first subspace */
        subspace = lg->assembly_group.subspaces;
    else
        /* Continue at the next slot */
        ctx->current_slot_index++;

    if (subspace == NULL)
    {
        /* Nothing to do. */
        ctx->step = RAIN1_REBUILD_FINISH;
        return EXA_SUCCESS;
    }

    if (ctx->current_slot_index >= subspace->total_slots_count)
    {
        /* We finished the current subspace. */
        subspace = subspace->next;
        if (subspace == NULL)
        {
            /* We're done. */
            ctx->step = RAIN1_REBUILD_FINISH;
            return EXA_SUCCESS;
        }

        /* Start working on the next subspace. */
        ctx->current_slot_index = 0;
    }

    uuid_copy(&ctx->current_subspace_uuid, &subspace->uuid);

    slot = subspace->slots[ctx->current_slot_index];
    EXA_ASSERT (slot != NULL);

    err = rain1_group_rebuild_slot(lg, slot, ctx);
    if (err != EXA_SUCCESS)
    {
        exalog_error("Failed to synchronize slot %"PRIu64" of subspace "
                     UUID_FMT ": %s (%d)", ctx->current_slot_index,
                     UUID_VAL(&subspace->uuid),
                     exa_error_msg(err), err);
        return err;
    }

    ctx->nb_slots_synchronized++;
    update_rebuild_progression(lg, ctx->nb_slots_synchronized);

    return EXA_SUCCESS;
}

static int rain1_group_rebuild_finish(rain1_rebuild_context_t *ctx)
{
    rain1_group_t *lg;
    struct rain1_realdev *lr;
    int i;

    EXA_ASSERT(ctx->group != NULL);
    lg = RAIN1_GROUP(ctx->group);

    foreach_rainx_rdev(lg, lr, i)
    {
	if (lr->mine && rain1_rdev_is_rebuilding(lr))
	{
	    /* Rebuilding is completed, mark the device for a complete
	       reintegrate */
	    os_thread_mutex_lock(&lr->rebuild_progress.lock);
	    lr->rebuild_progress.complete = TRUE;
	    os_thread_mutex_unlock(&lr->rebuild_progress.lock);
	}
    }

    exalog_info("Trigger checkup on end of rebuild of group "UUID_FMT,
                 UUID_VAL(&ctx->group->uuid));

    vrt_msg_reintegrate_device();

    return EXA_SUCCESS;
}

/**
 * Rebuild all local devices of the specified group. In rain1, the rebuilding
 * process can be either an update or a replication.
 */
int rain1_group_rebuild_step(void *context, bool *more_work)
{
    rain1_rebuild_context_t *ctx = context;
    rain1_group_t *lg;

    EXA_ASSERT(ctx->group);
    lg = RAIN1_GROUP(ctx->group);

    if (sync_tag_is_greater(lg->sync_tag, ctx->sync_tag))
        rain1_group_rebuild_context_reset(ctx);

    ctx->sync_tag = lg->sync_tag;

    /*
     * Do the rebuilding according to the dirty zone counters.
     *
     * Basically, the idea is to travel through the metadata of all
     * nodes of the cluster, because we must take into account all
     * writes that have been done by all nodes. We must read the
     * metadata of the current node (the one holding the real device to
     * rebuild), because this node could have made writes, dirtying
     * zones (in the case of the node being up, with only the real
     * device being down).
     *
     * As we don't want to rebuild several times the same dirty zone (a
     * dirty zone can be marked as dirty by several nodes), we use the
     * dzone_rebuild array to keep track of the dirty zones that needs
     * to be rebuilt.
     */

    EXA_ASSERT(RAIN1_REBUILD_STEP_IS_VALID(ctx->step));

    switch (ctx->step)
    {
    case RAIN1_REBUILD_BEGIN:
        *more_work = false;
        if (ctx->group->status == EXA_GROUP_OFFLINE
            || !rain1_group_is_rebuilding(lg))
            return EXA_SUCCESS;

        if (!rdev_context_array_init(&ctx->rebuild_data, lg))
            return EXA_SUCCESS;

        *more_work = true;
        ctx->step = RAIN1_REBUILD_SLOTS;
        /* Fallback: start rebuild first slot */
    case RAIN1_REBUILD_SLOTS:
        *more_work = true;
        return rain1_group_rebuild_next_slot(ctx);
    case RAIN1_REBUILD_FINISH:
        *more_work = false;
        return rain1_group_rebuild_finish(ctx);
    }

    EXA_ASSERT(false);
    return -EINVAL;
}

