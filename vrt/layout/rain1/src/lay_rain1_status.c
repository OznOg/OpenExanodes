/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */



#include "vrt/layout/rain1/src/lay_rain1_status.h"

#include "os/include/os_inttypes.h"

#include "common/include/exa_names.h"
#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"

#include "log/include/log.h"

#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_nodes.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/assembly/src/assembly_group.h"

#include "vrt/layout/rain1/src/lay_rain1_metadata.h"
#include "vrt/layout/rain1/src/lay_rain1_sync_tag.h"

#ifdef WITH_MONITORING
#include "monitoring/md_client/include/md_notify.h"
#endif


/* FIXME EXTERN CRAP */
/* shared for monitoring needs */
extern ExamsgHandle vrt_msg_handle;


exa_realdev_status_t rain1_rdev_get_compound_status(const void *layout_data,
                                                    const struct vrt_realdev *rdev)
{
    const rain1_group_t *rxg = layout_data;
    exa_realdev_status_t compound_status;
    const rain1_realdev_t *lr;

    EXA_ASSERT(rdev);

    /* get the 'common part' of compound status */
    compound_status = rdev_get_compound_status(rdev);

    /* add the rain1 OK variations */
    if (compound_status != EXA_REALDEV_OK)
        return compound_status;

    lr = RAIN1_REALDEV(rxg, rdev);

    switch (lr->rebuild_desc.type)
    {
    case EXA_RDEV_REBUILD_REPLICATING:
        return EXA_REALDEV_REPLICATING;

    case EXA_RDEV_REBUILD_UPDATING:
        return EXA_REALDEV_UPDATING;

    case EXA_RDEV_REBUILD_NONE:
        {
            sync_tag_t sync_tag = rxg->sync_tag;

            if (!rain1_rdev_is_uptodate(lr, sync_tag))
                return EXA_REALDEV_OUTDATED;

            return EXA_REALDEV_OK;
        }
    }
    EXA_ASSERT(false);
    return -1;
}

/**
 * Predicate -- Does a SPOF group needs to be updated?
 *
 * @param[in] rxg         the group layout
 * @param[in] spof_group  SPOF group to check
 *
 * @return true if the SPOF group needs to be updated.
 */
static bool spof_group_needs_updating(const rain1_group_t *rxg,
                                      const spof_group_t *spof_group)
{
    uint32_t i;

    for (i = 0; i < spof_group->nb_realdevs; i++)
    {
        const struct vrt_realdev *rdev = spof_group->realdevs[i];
        const rain1_realdev_t *lr = RAIN1_REALDEV(rxg, rdev);
        sync_tag_t sync_tag = rxg->sync_tag;

        if (rdev_is_ok(rdev) && !rain1_rdev_is_uptodate(lr, sync_tag))
            return true;
    }

    return false;
}

/**
 * Predicate -- get a spof group that needs updating.
 *
 * @param[in]  lg                rain1 group data
 * @param[in] spof_group         Array of spof_group
 * @param[in] nb_spof_groups     number_of spof group in array.
 *
 * @return a spof group that need to be updated (if any) or NULL.
 */
static spof_group_t *rain1_group_get_need_update_spof(const rain1_group_t *lg,
                                                      spof_group_t spof_groups[],
                                                      uint32_t nb_spof_groups)
{
    uint32_t i;

    for (i = 0; i < nb_spof_groups; i++)
        if (spof_group_needs_updating(lg, &spof_groups[i]))
            return &spof_groups[i];

    return NULL;
}

/**
 * Prepare an updating operation
 *
 * @param[in]  ag                assembly group
 * @param[in]  lg                rain1 group data
 * @param[in] spof_group         The SPOF group to update
 */
static void rain1_group_prepare_updating(const struct assembly_group *ag,
                                         const rain1_group_t *lg,
                                         const spof_group_t *spof_group)
{
    uint32_t i;

    for (i = 0; i < spof_group->nb_realdevs; i++)
    {
        struct vrt_realdev *rdev = spof_group->realdevs[i];
        struct rain1_realdev *lr = RAIN1_REALDEV(lg, rdev);

        if (rdev_is_ok(rdev) && !rain1_rdev_is_uptodate(lr, lg->sync_tag))
        {
            exalog_debug("New updating: update disk: index = %d, UUID = " UUID_FMT,
                         rdev->index, UUID_VAL(&rdev->uuid));

            rain1_rdev_init_rebuild_context(lr, EXA_RDEV_REBUILD_UPDATING,
                                            lg->sync_tag);
        }
    }
}


/**
 * Compute the new rebuilding status of a group and of its disks.
 * This function determines which maintenance operation should be
 * performed (updating or replicating one or several disks).
 * The priority of maintenance operations is the following:
 * - an interrupted replicating (top priority)
 * - a new updating
 * - a new replicating
 *
 * @param[in]  ag                assembly group
 * @param[in]  lg                rain1 group data
 */
static void rain1_compute_rebuilding_status(struct assembly_group *ag,
                                            rain1_group_t *lg,
                                            storage_t *storage)
{
    const spof_group_t *spof_group = NULL;

    EXA_ASSERT(lg != NULL && ag != NULL);

    /* Look for a new updating. */
    spof_group = rain1_group_get_need_update_spof(lg, storage->spof_groups,
                                                  storage->num_spof_groups);
    if (spof_group != NULL)
    {
        rain1_group_prepare_updating(ag, lg, spof_group);
        return;
    }
}

static void rain1_update_sync_tag(rain1_group_t *rxg)
{
    rain1_realdev_t *lr;
    int i;
    sync_tag_t new_tag = sync_tag_inc(rxg->sync_tag);

    foreach_rainx_rdev(rxg, lr, i)
    {
        /* The uptodate devices that are still UP must have their tag
         * incremented as well */
        if (rdev_is_up(lr->rdev)
            && sync_tag_is_equal(lr->sync_tag, rxg->sync_tag))
        {
            exalog_debug("Marking rdev "UUID_FMT" with new sync_tag %"PRIsync_tag,
                        UUID_VAL(&lr->uuid), new_tag);
            lr->sync_tag = new_tag;
        }
        else
            exalog_debug("NOT marking rdev "UUID_FMT" with new sync_tag.",
                        UUID_VAL(&lr->uuid));

        /* The device that are so outdated that their tag is not even
         * comparable with the new tag are considered as blank.
         * It avoids that a very outdated device magically becomes uptodate
         * due to the tag wraping.
         */
        if (!sync_tags_are_comparable(lr->sync_tag, new_tag))
            lr->sync_tag = SYNC_TAG_BLANK;
    }

   exalog_debug("Marking group with new sync_tag %"PRIsync_tag, new_tag);
   rxg->sync_tag = new_tag;
}

bool rain1_spof_group_has_defect(const rain1_group_t *rxg, const spof_group_t *spof_group)
{
    size_t i;

    /* The SPOF group has defect if any of its devices... */
    for (i = 0; i < spof_group->nb_realdevs; i++)
    {
	const struct vrt_realdev *rdev = spof_group->realdevs[i];
        const rain1_realdev_t *lr = RAIN1_REALDEV(rxg, rdev);
        sync_tag_t sync_tag = rxg->sync_tag;

	/* ... is down or corrupted */
	if (! rdev_is_ok(rdev))
	    return true;

	/* ... or is not up-to-date */
	if (!rain1_rdev_is_uptodate(lr, sync_tag))
	    return true;
    }
    return false;
}

void rain1_compute_status(struct vrt_group *group)
{
    rain1_group_t *lg = RAIN1_GROUP(group);
    struct assembly_group *ag = &lg->assembly_group;
    size_t nb_not_corrected_spofs;
    rain1_realdev_t *lr;
    int i;

    /* Compute the status of the group.
     * This status depends on the number of SPOF groups that are not working
     * fine (down or outdated).
     */
    nb_not_corrected_spofs = 0;
    for (i = 0; i < group->storage->num_spof_groups; i++)
    {
	if (rain1_spof_group_has_defect(lg, &group->storage->spof_groups[i]))
	    nb_not_corrected_spofs++;
    }

    if (nb_not_corrected_spofs == 0)
    {
	group->status = EXA_GROUP_OK;
	exalog_debug("Status of group '%s' is OK", group->name);
#ifdef WITH_MONITORING
	md_client_notify_diskgroup_ok(vrt_msg_handle, &group->uuid, group->name);
#endif

	/* FIXME: There could be no 'not_corrected' spof groups while an
	 * updating operation is on-going. In this case, we should have
	 * the group status set to EXA_GROUP_REBUILDING ... if we keep using
	 * EXA_GROUP_REBUILDING.
	 */
    }
    else if (nb_not_corrected_spofs == 1)
    {
	group->status = EXA_GROUP_DEGRADED;
	exalog_debug("Status of group '%s' is DEGRADED", group->name);
#ifdef WITH_MONITORING
	md_client_notify_diskgroup_degraded(vrt_msg_handle, &group->uuid, group->name);
#endif
    }
    else
    {
	group->status = EXA_GROUP_OFFLINE;
	exalog_debug("Status of group '%s' is OFFLINE (%" PRIzu " SPOFs not corrected)",
		     group->name, nb_not_corrected_spofs);
#ifdef WITH_MONITORING
	md_client_notify_diskgroup_offline(vrt_msg_handle, &group->uuid, group->name);
#endif
    }

    exalog_debug("Clearing Rebuild context for each of the group "UUID_FMT" rdevs",
                UUID_VAL(&group->uuid));
    /* Cleanup previous rebuilding status */
    foreach_rainx_rdev(lg, lr, i)
    {
        rain1_rdev_clear_rebuild_context(lr);
    }
    /* Compute a new oudate tag (greater than all the outdate tags already
     * used) and mark all the disk newly down or blank with it.
     */
    if (group->status != EXA_GROUP_OFFLINE)
    {
	rain1_update_sync_tag(lg);

        /* Determine which maintenance operation will be performed. */
        rain1_compute_rebuilding_status(ag, lg, group->storage);
    }

    /* This section of code reset the list of nodes that will collect
     * the metadata for the future update operations. If and only if all the
     * disks are UP (implying all the nodes involved in the group are UP),
     * 'nodes_update' is copied from the list of the UP nodes (including all
     * the nodes involved in the group). Considering that the only other
     * place where 'nodes_update' is modified is rain1_group_compute_status
     * which can only add nodes to 'nodes_update' (don't ask why), we can
     * conclude that 'nodes_update' always contains all the nodes involved in
     * the group... not very usefull.
     *
     * FIXME: If 'nodes_update' has any usage somewhere, it MUST be fixed
     * because it is clearly broken. If it doesnt, it should be removed.
     * I (MD) think it should be removed.
     */
    foreach_rainx_rdev(lg, lr, i)
	if (!rdev_is_ok(lr->rdev))
	{
            exalog_debug("Cannot cleanup nodes_update nodeset because rdev "
                         UUID_FMT " is not OK", UUID_VAL(&lr->rdev->uuid));
	    return;
	}

    vrt_node_get_upnodes(&lg->nodes_update);
}


void rain1_rebuild_finish(rain1_group_t *lg)
{
    EXA_ASSERT(lg);

    /* Rebuilding is finished */
    EXA_ASSERT(!rain1_group_is_rebuilding(lg));
}
