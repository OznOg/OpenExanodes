/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"
#include "os/include/os_mem.h"
#include "common/include/exa_names.h"

#include "log/include/log.h"

#include "os/include/os_error.h"
#include "os/include/os_stdio.h"

#include "vrt/assembly/src/assembly_group.h"
#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"

#include "vrt/layout/sstriping/include/sstriping.h"
#include "vrt/layout/sstriping/src/lay_sstriping.h"
#include "vrt/layout/sstriping/src/lay_sstriping_group.h"

#ifdef WITH_MONITORING
#include "monitoring/md_client/include/md_notify.h"
#endif

/* FIXME EXTERN CRAP */
/* shared for monitoring needs */
extern ExamsgHandle vrt_msg_handle;

/* FIXME Temporary, for backward compatibility. */
void sstriping_group_cleanup(void **private_data, const storage_t *storage)
{
    sstriping_group_t *ssg = *private_data;
    sstriping_group_free(ssg,storage);
    *private_data = NULL;
}

static int sstriping_create_subspace(void *private_data, const exa_uuid_t *uuid,
                                     uint64_t size, assembly_volume_t **av,
                                     storage_t *storage)
{
     sstriping_group_t *lg = private_data;
     assembly_group_t *ag = &lg->assembly_group;
     uint64_t nb_slots;

     EXA_ASSERT(size > 0);
     nb_slots = quotient_ceil64(size, lg->logical_slot_size);

    exalog_debug("creating subspace: size = %"PRIu64" sectors"
                 " (= %" PRIu64 " slots)", size, nb_slots);

    return assembly_group_reserve_volume(ag, uuid, nb_slots, av, storage);
}

static void sstriping_delete_subspace(void *layout_data, assembly_volume_t **av,
                                      storage_t *storage)
{
    sstriping_group_t *lg = layout_data;
    assembly_group_t *ag = &lg->assembly_group;

    assembly_group_release_volume(ag, *av, storage);
}

static int
sstriping_volume_get_status(const vrt_volume_t *volume)
{
    if (volume->group->status == EXA_GROUP_OFFLINE)
	return VRT_ADMIND_STATUS_DOWN;
    else
	return VRT_ADMIND_STATUS_UP;
}

static uint64_t sstriping_volume_get_size(const vrt_volume_t *volume)
{
    const sstriping_group_t *lg = SSTRIPING_GROUP(volume->group);
    const assembly_volume_t *av = volume->assembly_volume;

    return av->total_slots_count * lg->logical_slot_size;
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
static int
sstriping_volume_resize(vrt_volume_t *volume, uint64_t newsize,
                        const storage_t *storage)
{

    sstriping_group_t *lg = SSTRIPING_GROUP(volume->group);
    assembly_group_t *ag = &lg->assembly_group;
    uint64_t new_nb_slots;

    new_nb_slots = quotient_ceil64(newsize, lg->logical_slot_size);

    exalog_debug("resize volume '%s': size = %" PRIu64 " sectors (=%" PRIu64 " slots)",
                 volume->name, newsize, new_nb_slots);

    return assembly_group_resize_volume(ag, volume->assembly_volume, new_nb_slots,
                                        storage);
}


/**
 * Finalize the group creation.
 *
 * @param[in] group The group currently being created
 *
 * @return EXA_SUCCESS on success, an error code on failure.
 */
static int
sstriping_group_create(storage_t *storage, void **private_data,
                       uint32_t slot_width, uint32_t chunk_size, uint32_t su_size,
                       uint32_t dirty_zone_size, uint32_t blended_stripes,
                       uint32_t nb_spare, char *error_msg)
{
    sstriping_group_t *lg;
    assembly_group_t *ag;
    int ret;

    if (su_size == 0 || su_size % 8 != 0)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Invalid su_size (%u KiB). Must be multiple of 4 KiB and not null.\n",
		 SECTORS_2_KBYTES(su_size));
	return -EINVAL;
    }

    /* check if su_size is a power of 2 */
    if (! IS_POWER_OF_TWO(su_size))
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Invalid striping unit size (%u KiB). Must be a not a power of 2.\n",
		 SECTORS_2_KBYTES(su_size));
	return -EINVAL;
    }

    /* Ensure that the chunk size is a multiple of the striping unit size. */
    if (chunk_size % su_size != 0)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "The chunk size must be a multiple of the striping unit size.\n");
	return -EINVAL;
    }

    if (nb_spare != 0)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "sstriping layout does not support spare.\n");
	return -EINVAL;
    }

    *private_data = sstriping_group_alloc();
    if (*private_data == NULL)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1, "Failed allocating group");
	return -ENOMEM;
    }


    lg = *private_data;
    ag = &lg->assembly_group;

    lg->su_size = su_size;

    if (slot_width > storage->num_spof_groups)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Slot width is too big. The maximum slot width for this group is %" PRIu32 "\n",
		 storage->num_spof_groups);
	return -EINVAL;
    }
    else if (slot_width == 0)
    {
	/* Compute a "good" value for slot_width. Experiments have shown
	 * that striping data over more than 6 disks does not improve
	 * throughput.
	 */
	slot_width = MIN(storage->num_spof_groups, 6);
    }
    lg->logical_slot_size = (uint64_t) slot_width * chunk_size;
    if (lg->logical_slot_size > UINT32_MAX)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Slot size is too big (>= 2TiB). You should decrease the "
		 "slot_width or chunk_size values.\n");
	return -EINVAL;
    }

    /* Init slots */
    ret = assembly_group_setup(ag, slot_width, chunk_size);
    if (ret != EXA_SUCCESS)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "Failed to init slots.\n");
	return ret;
    }

    return EXA_SUCCESS;
}

/**
 * Start a group.
 *
 * This function gets called when the virtualizer 1) has allocated and
 * initialized the vrt_group_t structure, 2) has received all real
 * devices, checked they are correct, and added them to the
 * vrt_group_t structure. However, the group is not yet publicly
 * visible.
 *
 * @param[in] group The group to start
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
static int
sstriping_group_start(vrt_group_t *group, const storage_t *storage)
{
    sstriping_group_t *lg;
    vrt_realdev_t *rdev;
    storage_rdev_iter_t iter;

    /*
     * Load group information
     */
    lg = group->layout_data;
    EXA_ASSERT(lg);

    /* Mark the slots used for (future) metadata as busy. These slots are at
     * the beginning of the group.
     *
     * FIXME: The slot reservation is an assembly modification. It should be
     * done once for all at the group creation (we could imagine reserving new
     * metadata slots also at the volume creation) and stored by the assembly
     * layer itself.
     *
     * FIXME: These slots being stored at the beginning or at the end or
     * anywhere else is not a layout problem. It should be the assembly layer
     * that decides.
     */

    group->status = EXA_GROUP_OK;
#ifdef WITH_MONITORING
    md_client_notify_diskgroup_ok(vrt_msg_handle, &group->uuid, group->name);
#endif
    storage_rdev_iterator_begin(&iter, group->storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
	if (!rdev_is_ok(rdev))
	{
	    exalog_debug("rdev " UUID_FMT " is not OK => group '%s' is OFFLINE",
                         UUID_VAL(&rdev->uuid), group->name);
	    group->status = EXA_GROUP_OFFLINE;
#ifdef WITH_MONITORING
	    md_client_notify_diskgroup_offline(vrt_msg_handle, &group->uuid, group->name);
#endif
	}
    }
    storage_rdev_iterator_end(&iter);

    return EXA_SUCCESS;
}

static int
sstriping_group_stop(void *private_data)
{
    sstriping_group_t *lg = private_data;

    assembly_group_cleanup(&lg->assembly_group);

    return EXA_SUCCESS;
}


static int
sstriping_group_rdev_down(vrt_group_t *group)
{
    if (group->status == EXA_GROUP_OK)
    {
	exalog_debug("set group '%s' in ERROR state", group->name);
	group->status = EXA_GROUP_OFFLINE;
#ifdef WITH_MONITORING
	md_client_notify_diskgroup_offline(vrt_msg_handle, &group->uuid,
					   group->name);
#endif
    }

    return EXA_SUCCESS;
}

static int sstriping_group_rdev_up(vrt_group_t *group)
{
    vrt_realdev_t *rdev;
    storage_rdev_iter_t iter;

    storage_rdev_iterator_begin(&iter, group->storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
	if (!rdev_is_ok(rdev))
	{
	    /* There's still at least one rdev down in the group.
             * Don't go back to OK.
             */
            storage_rdev_iterator_end(&iter);
	    return EXA_SUCCESS;
	}
    storage_rdev_iterator_end(&iter);

    exalog_debug("set group '%s' in OK state", group->name);
    group->status = EXA_GROUP_OK;
#ifdef WITH_MONITORING
    md_client_notify_diskgroup_ok(vrt_msg_handle, &group->uuid,
				  group->name);
#endif

    return EXA_SUCCESS;
}


/**
 * @return -VRT_ERR_PREVERNT_GROUP_OFFLINE if the group will go offline, or
 *         EXA_SUCCESS if the group will not go offline, or
 *         a negative error code on failure
 */
static int
sstriping_group_going_offline(const vrt_group_t *group, const exa_nodeset_t *stop_nodes)
{
    vrt_realdev_t *rdev;

    storage_rdev_iter_t iter;

    storage_rdev_iterator_begin(&iter, group->storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
        exa_nodeid_t node_id = vrt_rdev_get_nodeid(rdev);

	if (exa_nodeset_contains(stop_nodes, node_id))
        {
            storage_rdev_iterator_end(&iter);
	    return -VRT_ERR_PREVENT_GROUP_OFFLINE;
        }
    }
    storage_rdev_iterator_end(&iter);

    return EXA_SUCCESS;
}

/**
 * Compose the 'old' real device status
 *
 * @param[in] rdev	the real device
 *
 * @return the compound status of the device
 */
static exa_realdev_status_t sstriping_rdev_get_compound_status(
                        const void *layout_data,
                        const vrt_realdev_t *rdev)
{
    return rdev_get_compound_status(rdev);
}


static uint32_t sstriping_get_su_size(const void *private_data)
{
    const sstriping_group_t *lg = private_data;
    EXA_ASSERT(lg);
    return lg->su_size;
}

static uint64_t sstriping_get_group_total_capacity(const void *private_data,
                                                   const storage_t *storage)
{
    const sstriping_group_t *lg = private_data;
    const assembly_group_t *ag = &lg->assembly_group;
    uint64_t max_slots_count = assembly_group_get_max_slots_count(ag, storage);

    /* Metadata slot are removed from total because the user cannot get
     * this space anyway */
    return SECTORS_TO_BYTES(max_slots_count * lg->logical_slot_size);
}

static uint64_t sstriping_get_group_used_capacity(const void *private_data)
{
    const sstriping_group_t *lg = private_data;
    const struct assembly_group *ag = &lg->assembly_group;

    return SECTORS_TO_BYTES(assembly_group_get_used_slots_count(ag)
                    * lg->logical_slot_size);
}

static uint32_t sstriping_get_slot_width(const void *private_data)
{
    const sstriping_group_t *lg = private_data;
    const assembly_group_t *ag = &lg->assembly_group;

    return ag->slot_width;
}

static int __sstriping_serialize(const void *private_data, stream_t *stream)
{
    return sstriping_group_serialize((sstriping_group_t *)private_data, stream);
}

static int __sstriping_deserialize(void **private_data, const storage_t *storage, stream_t *stream)
{
    return sstriping_group_deserialize((sstriping_group_t **)private_data, storage, stream);
}

static uint64_t __sstriping_serialized_size(const void *private_data)
{
    return sstriping_group_serialized_size(private_data);
}

static const assembly_group_t *sstriping_get_assembly_group(const void *private_data)
{
    const sstriping_group_t *ssg = private_data;
    return &ssg->assembly_group;
}

static bool sstriping_layout_data_equals(const void *private_data1,
                                         const void *private_data2)
{
    const sstriping_group_t *ssg1 = private_data1;
    const sstriping_group_t *ssg2 = private_data2;

    return sstriping_group_equals(ssg1, ssg2);
}

static struct vrt_layout layout_sstriping =
{
    .list =                          LIST_HEAD_INIT(layout_sstriping.list),
    .name =                          SSTRIPING_NAME,
    .group_create =                  sstriping_group_create,
    .group_start =                   sstriping_group_start,
    .group_stop =                    sstriping_group_stop,
    .group_cleanup =                 sstriping_group_cleanup,
    .group_compute_status =          NULL,
    .group_resync =                  NULL,
    .group_post_resync =             NULL,
    .serialize =                     __sstriping_serialize,
    .deserialize =                   __sstriping_deserialize,
    .serialized_size =               __sstriping_serialized_size,
    .get_assembly_group =            sstriping_get_assembly_group,
    .layout_data_equals =            sstriping_layout_data_equals,
    .group_metadata_flush_step =     NULL,
    .group_going_offline =           sstriping_group_going_offline,
    .group_reset =                   NULL,
    .group_check =                   NULL,
    .create_subspace =               sstriping_create_subspace,
    .delete_subspace =               sstriping_delete_subspace,
    .volume_resize =                 sstriping_volume_resize,
    .volume_get_status =             sstriping_volume_get_status,
    .volume_get_size =               sstriping_volume_get_size,
    .group_rebuild_step =            NULL,
    .group_is_rebuilding =           NULL,
    .build_io_for_req =              sstriping_build_io_for_req,
    .init_req =                      sstriping_init_req,
    .cancel_req =                    sstriping_cancel_req,
    .get_slot_width =                sstriping_get_slot_width,
    .declare_io_needs =              sstriping_declare_io_needs,
    .group_rdev_down =               sstriping_group_rdev_down,
    .group_rdev_up =                 sstriping_group_rdev_up,
    .rdev_reset =                    NULL,
    .rdev_reintegrate =              NULL,
    .rdev_post_reintegrate =         NULL,
    .rdev_get_reintegrate_info =     NULL,
    .rdev_get_rebuild_info =         NULL,
    .rdev_get_compound_status =      sstriping_rdev_get_compound_status,
    .get_su_size =                   sstriping_get_su_size,
    .get_group_total_capacity =      sstriping_get_group_total_capacity,
    .get_group_used_capacity  =      sstriping_get_group_used_capacity
};

int sstriping_init(void)
{
    return vrt_register_layout(&layout_sstriping);
}

void sstriping_cleanup(void)
{
    vrt_unregister_layout(&layout_sstriping);
}
