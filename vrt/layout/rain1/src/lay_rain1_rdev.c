/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h> /* for memset */

#include "vrt/layout/rain1/src/lay_rain1_rdev.h"
#include "vrt/layout/rain1/src/lay_rain1_group.h"

#include "vrt/virtualiseur/include/vrt_group.h"

#include "os/include/os_mem.h"

void rain1_rdev_init_rebuild_context(struct rain1_realdev *lr,
                                     rdev_rebuild_type_t type,
                                     sync_tag_t sync_tag)
{
    EXA_ASSERT(type == EXA_RDEV_REBUILD_UPDATING
               || type == EXA_RDEV_REBUILD_REPLICATING);

    lr->rebuild_desc.type = type;
    lr->rebuild_desc.sync_tag = sync_tag;

    os_thread_mutex_lock(&lr->rebuild_progress.lock);

    lr->rebuild_progress.complete = FALSE;
    lr->rebuild_progress.nb_slots_rebuilt = 0;

    os_thread_mutex_unlock(&lr->rebuild_progress.lock);
}

void rain1_rdev_clear_rebuild_context(struct rain1_realdev *lr)
{
    lr->rebuild_desc.type = EXA_RDEV_REBUILD_NONE;

    os_thread_mutex_lock(&lr->rebuild_progress.lock);

    lr->rebuild_progress.complete = FALSE;
    lr->rebuild_progress.nb_slots_rebuilt = 0;

    os_thread_mutex_unlock(&lr->rebuild_progress.lock);
}

exa_bool_t rain1_rdev_is_uptodate(const rain1_realdev_t *lr, sync_tag_t sync_tag)
{
    return sync_tag_is_equal(lr->sync_tag, sync_tag);
}

exa_bool_t rain1_rdev_is_updating(const struct rain1_realdev *lr)
{
    return lr->rebuild_desc.type == EXA_RDEV_REBUILD_UPDATING;
}

exa_bool_t rain1_rdev_is_replicating(const struct rain1_realdev *lr)
{
    return lr->rebuild_desc.type == EXA_RDEV_REBUILD_REPLICATING;
}

exa_bool_t rain1_rdev_is_writable(const rain1_group_t *rxg, const struct vrt_realdev *rdev)
{
    const rain1_realdev_t *lr = RAIN1_REALDEV(rxg, rdev);
    sync_tag_t sync_tag = rxg->sync_tag;

    return rdev_is_ok(rdev)
        && (rain1_rdev_is_uptodate(lr, sync_tag) || rain1_rdev_is_updating(lr));
}

exa_bool_t rain1_rdev_is_rebuilding(const struct rain1_realdev *lr)
{
    return rain1_rdev_is_replicating(lr) || rain1_rdev_is_updating(lr);
}

exa_bool_t rain1_rdev_location_readable(const struct rdev_location *rdev_loc)
{
    return rdev_loc->uptodate;
}

exa_bool_t rain1_rdev_location_update_needed(const struct rdev_location *rdev_loc)
{
    return rdev_loc->never_replicated;
}

struct rain1_realdev *rain1_alloc_rdev_layout_data(vrt_realdev_t *rdev)
{
    struct rain1_realdev *lr = os_malloc(sizeof(struct rain1_realdev));
    if (lr == NULL)
	return NULL;

    memset(lr, 0, sizeof(struct rain1_realdev));

    /* FIXME storing the rdev here means we can get rid of most of the
     * iterations of the storage, and just iterate over rxg->rain1_rdevs[]->rdev.
     */
    lr->rdev = rdev;

    /* Store the rdev uuid as this is the only link with rdev when we need to
     * find it out after reread SB */
    uuid_copy(&lr->uuid, &rdev->uuid);

    lr->mine = rdev->local;

    os_thread_mutex_init(&lr->rebuild_progress.lock);
    lr->rebuild_progress.complete = FALSE;
    lr->rebuild_progress.nb_slots_rebuilt = 0;

    lr->sync_tag = SYNC_TAG_ZERO;

    return lr;
}

void rain1_free_rdev_layout_data(struct rain1_realdev *lr)
{
    os_free(lr);
}

bool rain1_realdev_equals(const rain1_realdev_t *lr1, const rain1_realdev_t *lr2)
{
    if (!uuid_is_equal(&lr1->uuid, &lr2->uuid))
        return false;

    if (lr1->sync_tag != lr2->sync_tag)
        return false;

    return true;
}
