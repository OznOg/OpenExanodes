/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>

#include "common/include/exa_math.h"
#include "common/include/exa_error.h"

#include "log/include/log.h"

#include "os/include/strlcpy.h"

#include "vrt/virtualiseur/include/vrt_common.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_msg.h"

#include "vrt/virtualiseur/src/vrt_module.h"

#include "os/include/os_error.h"
#include "os/include/os_stdio.h"

static int vrt_info_group_info(struct vrt_group_info *result,
                               const exa_uuid_t *group_uuid)
{
    struct vrt_group *group; /* FIXME: constify */
    const struct vrt_layout *layout;

    group = vrt_get_group_from_uuid(group_uuid);

    if (!group)
        return -ENOENT;

    layout = group->layout;
    EXA_ASSERT(layout);

    uuid_copy(&result->uuid, group_uuid);
    strlcpy(result->name, group->name, EXA_MAXSIZE_GROUPNAME + 1);

    result->status = group->status;

    if (layout->group_is_rebuilding)
        result->is_rebuilding = layout->group_is_rebuilding(group->layout_data);
    else
        result->is_rebuilding = FALSE;

    result->usable_capacity = BYTES_TO_KBYTES(vrt_group_total_capacity(group));
    result->used_capacity   = BYTES_TO_KBYTES(vrt_group_used_capacity(group));

    EXA_ASSERT(result->usable_capacity >= result->used_capacity);

    result->slot_width = group->layout->get_slot_width(group->layout_data);
    result->chunk_size = group->storage->chunk_size;

    if (layout->get_nb_spare)
        layout->get_nb_spare(group->layout_data, &result->nb_spare,
                             &result->nb_spare_available);
    else
    {
        result->nb_spare = -1;
        result->nb_spare_available = -1;
    }

    if (layout->get_su_size)
        result->su_size = SECTORS_2_KBYTES(layout->get_su_size(group->layout_data));

    if (layout->get_dirty_zone_size)
        os_snprintf(result->dirty_zone_size,
		    sizeof(result->dirty_zone_size), "%u",
		    SECTORS_2_KBYTES(layout->get_dirty_zone_size(group->layout_data)));

    if (layout->get_blended_stripes)
        os_snprintf(result->blended_stripes,
		    sizeof(result->blended_stripes), "%s",
		    layout->get_blended_stripes(group->layout_data) ?
		    ADMIND_PROP_TRUE : ADMIND_PROP_FALSE);

    strlcpy(result->layout_name, layout->name, sizeof(result->layout_name));

    vrt_group_unref(group);

    return EXA_SUCCESS;
}

static void vrt_info_rdev_info(struct vrt_realdev_info *result,
                               const exa_uuid_t *group_uuid,
                               const exa_uuid_t *disk_uuid)
{
    struct vrt_group *group;
    struct vrt_realdev *realdev;

    group = vrt_get_group_from_uuid(group_uuid);
    if (group) /* FIXME: we should invert the test and log an error */
    {
        realdev = storage_get_rdev(group->storage, disk_uuid);
        if (realdev)
        {
            uuid_copy(&result->group_uuid, group_uuid);
            uuid_copy(&result->uuid, &realdev->uuid);

            /* FIXME: replace this size by the size actualy used in the assembly */
            result->size = SECTORS_2_KBYTES(rdev_chunk_based_size(realdev));

            result->capacity_used =
                   (uint64_t)(realdev->chunks.total_chunks_count - realdev->chunks.free_chunks_count)
                        * SECTORS_2_KBYTES(realdev->chunks.chunk_size);

	    result->status = group->layout->rdev_get_compound_status(
                                            group->layout_data, realdev);
        }

        vrt_group_unref(group);
    }
}

static int vrt_info_rdev_rebuild_info(struct vrt_realdev_rebuild_info *result,
                                      const exa_uuid_t *group_uuid,
                                      const exa_uuid_t *disk_uuid)
{
    struct vrt_group *group;
    struct vrt_realdev *realdev;
    uint64_t logical_rebuilt_size = 0;
    uint64_t logical_size_to_rebuild = 0;

    group = vrt_get_group_from_uuid(group_uuid);
    if (group == NULL)
        return -VRT_ERR_UNKNOWN_GROUP_UUID;

    if (group->layout->rdev_get_reintegrate_info == NULL)
    {
        vrt_group_unref(group);
        return -VRT_ERR_LAYOUT_UNKNOWN_OPERATION;
    }

    realdev = storage_get_rdev(group->storage, disk_uuid);
    if (realdev == NULL)
    {
        vrt_group_unref(group);
        return -VRT_ERR_UNKNOWN_DISK_UUID;
    }

    if (!rdev_is_local(realdev))
    {
        vrt_group_unref(group);
        return -VRT_ERR_DISK_NOT_LOCAL;
    }

    /* ask the layout for the reintegrate informations */
    group->layout->rdev_get_rebuild_info(group->layout_data,
                                         realdev,
                                         &logical_rebuilt_size,
                                         &logical_size_to_rebuild);

    result->rebuilt_size = SECTORS_2_KBYTES(logical_rebuilt_size);
    result->size_to_rebuild = SECTORS_2_KBYTES(logical_size_to_rebuild);

    vrt_group_unref(group);

    return EXA_SUCCESS;
}

static int vrt_info_rdev_reintegrated_info(struct vrt_realdev_reintegrate_info *result,
                                           const exa_uuid_t *group_uuid,
                                           const exa_uuid_t *disk_uuid)
{
    struct vrt_group *group;
    struct vrt_realdev *realdev;

    group = vrt_get_group_from_uuid(group_uuid);
    if (group == NULL)
        return -VRT_ERR_UNKNOWN_GROUP_UUID;

    realdev = storage_get_rdev(group->storage, disk_uuid);
    if (realdev == NULL)
    {
        vrt_group_unref(group);
        return -VRT_ERR_UNKNOWN_DISK_UUID;
    }

    /* ask the layout for the reintegrate informations */
    if (group->layout->rdev_get_reintegrate_info != NULL)
        group->layout->rdev_get_reintegrate_info(group->layout_data,
                                                 realdev,
                                                 &result->reintegrate_needed);
    else
        result->reintegrate_needed = false;

    vrt_group_unref(group);

    return EXA_SUCCESS;
}

static void vrt_info_volume_info (struct vrt_volume_info *result,
				  const exa_uuid_t *group_uuid,
				  const exa_uuid_t *volume_uuid)
{
    struct vrt_group *group;
    struct vrt_volume *volume;

    group = vrt_get_group_from_uuid (group_uuid);

    if (group) /* FIXME: we should invert the test and log an error */
    {
	volume = vrt_group_find_volume(group, volume_uuid);
	if (volume)
	{
            uuid_copy (& result->group_uuid, group_uuid);
	    uuid_copy (& result->uuid, volume_uuid);

	    strlcpy(result->name, volume->name, EXA_MAXSIZE_VOLUMENAME + 1);
	    result->size   = SECTORS_2_KBYTES(volume->size);
	    result->status = volume->status;
	}

	vrt_group_unref(group);
    }
}

void vrt_info_handle_message(const struct VrtAskInfo *msg,
                             vrt_reply_t *reply)
{
    /* vrt_info_* functions really should just fill in their part
       completely, but I did not actually verify that they do not rely
       on this memset, so I leave it there in the meantime. */
    memset(reply, 0, sizeof(*reply));

    switch(msg->type)
    {
    case GROUP_INFO:
	reply->retval = vrt_info_group_info(&reply->group_info, &msg->group_uuid);
	break;
    case VOLUME_INFO:
        vrt_info_volume_info(&reply->volume_info, &msg->group_uuid,
                             &msg->volume_uuid);
	reply->retval = EXA_SUCCESS;
	break;
    case RDEV_INFO:
	vrt_info_rdev_info(&reply->rdev_info, &msg->group_uuid,
                           &msg->disk_uuid);
        reply->retval = EXA_SUCCESS;
        break;
    case RDEV_REBUILD_INFO:
	reply->retval = vrt_info_rdev_rebuild_info(&reply->rdev_rebuild_info,
                                                  &msg->group_uuid,
                                                  &msg->disk_uuid);
        break;
    case RDEV_REINTEGRATE_INFO:
	reply->retval = vrt_info_rdev_reintegrated_info(&reply->rdev_reintegrate_info,
                                                        &msg->group_uuid,
                                                        &msg->disk_uuid);
	break;
    }
}


