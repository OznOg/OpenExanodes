/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>
#include <errno.h>

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"

#include "vrt/virtualiseur/include/vrt_msg.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_nodes.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_metadata.h"
#include "vrt/virtualiseur/include/vrt_rebuild.h"
#include "vrt/virtualiseur/include/vrt_request.h"
#include "vrt/common/include/spof.h"

#include "vrt/virtualiseur/src/vrt_module.h"

#include "log/include/log.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_string.h"
#include "os/include/os_stdio.h"

/** The static singleton used to build the description from successive examsgs. */
static vrt_group_info_t *pending_group = NULL;

/**
 * A mutex protecting the global variable pending_group against
 * concurrent accesses
 */
static os_thread_mutex_t pending_group_lock;

/**
 * Clean up the pending group. Used in exa_clstop. Needed when a group
 * creation failed and just after that, the user want to stop the
 * cluster.
 *
 * FIXME I really doubt this cleans the layout correctly.
 *
 * @return EXA_SUCCESS, period.
 */
static int
vrt_cmd_pending_group_cleanup(void)
{
    os_thread_mutex_lock(&pending_group_lock);
    os_free(pending_group);
    os_thread_mutex_unlock(&pending_group_lock);

    return EXA_SUCCESS;
}

/**
 * Begin group creation/starting command. Such a command must be
 * followed by several group_add_rdev() commands to add the real
 * devices in the group, and finally by a group_create() or
 * group_start() command.
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - Name of the group to create or start
 *  - Its UUID
 *  - Name of the layout to use
 *
 * @return EXA_SUCCESS on success, negative error code on failure
 */
static int
vrt_cmd_group_begin(const struct VrtGroupBegin *cmd)
{
    vrt_group_t *group;

    exalog_debug("begin group '%s': UUID='" UUID_FMT "' layout='%s'",
                 cmd->group_name, UUID_VAL(& cmd->group_uuid), cmd->layout);

    /* Check if the group is already started */
    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if(group)
    {
	EXA_ASSERT(strcmp(cmd->group_name, group->name)==0 );
	vrt_group_unref(group);
	return -VRT_INFO_GROUP_ALREADY_STARTED;
    }

    /* check if the group name is already used (this should not happen if
     * admind XML parsing is correct...
     */
    group = vrt_get_group_from_name(cmd->group_name);
    if (group != NULL)
    {
	vrt_group_unref(group);
	return -VRT_ERR_GROUPNAME_USED;
    }

    os_thread_mutex_lock(&pending_group_lock);
    if (pending_group)
        os_free(pending_group);

    pending_group = os_malloc(sizeof(vrt_group_info_t));
    vrt_group_info_init(pending_group);
    os_strlcpy(pending_group->name, cmd->group_name,
            sizeof(pending_group->name));
    os_strlcpy(pending_group->layout_name, cmd->layout,
            sizeof(pending_group->layout_name));
    uuid_copy(&pending_group->uuid, &cmd->group_uuid);
    pending_group->sb_version = cmd->sb_version;

    os_thread_mutex_unlock(&pending_group_lock);

    return EXA_SUCCESS;
}


/**
 * Command to register a real device in a group.
 * It must be called after group_begin() and before
 * group_create() or group_start().
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group in which the real device has to be added
 *  - UUID of the real device in the VRT
 *  - UUID of the real device in the NBD
 *  - Whether the disk is local or not
 *  - Whether the disk is UP or not
 *  - Whether the device properties must be loaded from disk
 *
 * @return EXA_SUCCESS on success, negative error code on failure.
 */
static int
vrt_cmd_group_add_rdev(const struct VrtGroupAddRdev *cmd)
{
    vrt_rdev_info_t *rdev_info;

    os_thread_mutex_lock(&pending_group_lock);
    if (!pending_group)
    {
	os_thread_mutex_unlock(&pending_group_lock);
	return -EPERM;
    }

    if (!uuid_is_equal(&cmd->group_uuid, &pending_group->uuid))
    {
	exalog_error("Failed to edit group " UUID_FMT
                     ", group " UUID_FMT " is already being edited.",
                     UUID_VAL(&cmd->group_uuid), UUID_VAL(&pending_group->uuid));
	os_thread_mutex_unlock(&pending_group_lock);
	return -EAGAIN;
    }

    if (pending_group->nb_rdevs == NBMAX_DISKS_PER_GROUP)
    {
	os_thread_mutex_unlock(&pending_group_lock);
	return -EAGAIN;
    }

    rdev_info = &pending_group->rdevs[pending_group->nb_rdevs];

    uuid_copy(&rdev_info->uuid,     &cmd->uuid);
    uuid_copy(&rdev_info->nbd_uuid, &cmd->nbd_uuid);

    rdev_info->node_id = cmd->node_id;
    rdev_info->spof_id = cmd->spof_id;
    rdev_info->local   = cmd->local;
    rdev_info->up      = cmd->up;

    pending_group->nb_rdevs++;

    os_thread_mutex_unlock(&pending_group_lock);

    return EXA_SUCCESS;
}

/**
 * Finalize group creation. This command must be called after all
 * group_add_rdev() commands in order to finalize the creation
 * of the group.
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group to create
 *  - Group properties: slot width, chunk size, SU size, etc.
 *
 * @return EXA_SUCCESS on success, negative error code on failure
 */
static int vrt_cmd_group_create(const struct VrtGroupCreate *cmd,
                                struct vrt_group_create *reply)
{
    vrt_group_layout_info_t *layout_info;
    int ret;

    /* FIXME The newlines (\n) in the error messages have *NOTHING* to do
             here: leave the formatting to upper layers. */

    os_thread_mutex_lock(&pending_group_lock);
    if (!pending_group)
    {
        os_thread_mutex_unlock(&pending_group_lock);
	os_snprintf(reply->error_msg, EXA_MAXSIZE_LINE + 1,
                    "Pending group not available\n");

	return -EPERM;
    }

    if (!uuid_is_equal(&cmd->group_uuid, &pending_group->uuid))
    {
        os_thread_mutex_unlock(&pending_group_lock);
	os_snprintf(reply->error_msg, EXA_MAXSIZE_LINE + 1,
		    "You are trying to create, start or delete two groups at"
                    " the same time.\n");

        return -EAGAIN;
    }

    os_snprintf(reply->error_msg, EXA_MAXSIZE_LINE + 1, "OK\n");

    layout_info                  = &pending_group->layout_info;
    layout_info->is_set          = true;
    layout_info->slot_width      = cmd->slot_width;
    layout_info->chunk_size      = KBYTES_2_SECTORS(cmd->chunk_size);
    layout_info->su_size         = KBYTES_2_SECTORS(cmd->su_size);
    layout_info->dirty_zone_size = KBYTES_2_SECTORS(cmd->dirty_zone_size);
    layout_info->blended_stripes = cmd->blended_stripes != 0;
    layout_info->nb_spares       = cmd->nb_spare;

    /* All sizes in 'cmd' are in KB. VRT internal functions want sizes in
     * sectors. */
    ret = vrt_group_create(pending_group, reply->error_msg);

    os_free(pending_group);

    os_thread_mutex_unlock(&pending_group_lock);

    return ret;
}


/**
 * Test whether a group is stoppable (i.e if it has no started volume).
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group to test
 *
 * @return 0 on success, a negative error code on failure.
 */
static int
vrt_cmd_group_stoppable(const struct VrtGroupStoppable *cmd)
{
    struct vrt_group *group;
    int ret;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (group == NULL)
	return -VRT_ERR_GROUP_NOT_STARTED;

    ret = vrt_group_stoppable(group, &cmd->group_uuid);

    vrt_group_unref(group);

    return ret;
}


/**
 * Test whether a group will go offline if we stop the specified
 * nodes
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group to test
 *  - The nodeset of nodes that may stop
 *
 * @return 0 on success, a negative error code on failure.
 */
static int
vrt_cmd_group_going_offline(const struct VrtGroupGoingOffline *cmd)
{
    struct vrt_group *group;
    int ret;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (group == NULL)
	return -VRT_ERR_GROUP_NOT_STARTED;

    /* If the group is already OFFLINE, it does not go OFFLINE. */
    if (group->status == EXA_GROUP_OFFLINE)
	ret = EXA_SUCCESS;
    else
	ret = vrt_group_going_offline(group, &cmd->stop_nodes);

    vrt_group_unref(group);

    return ret;
}


/**
 * Stop an active group
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group to stop
 *
 * @return 0 on success, a negative error code on failure
 */
static int
vrt_cmd_group_stop(const struct VrtGroupStop *cmd)
{
    struct vrt_group *group;
    int ret;

    exalog_debug("stop group " UUID_FMT, UUID_VAL(&cmd->group_uuid));

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (!group)
    {
	exalog_debug("unknown group " UUID_FMT, UUID_VAL(&cmd->group_uuid));
	return -VRT_ERR_GROUP_NOT_STARTED;
    }

    /* removing group from group list prevent other user to be able to get
     * new references on this group */
    vrt_groups_list_del(group);

    ret = vrt_group_stop(group);
    if (ret != EXA_SUCCESS)
    {
	vrt_group_unref(group);
	return ret;
    }

    /* No need to call (and can't anyway) vrt_group_unref() because the
       group has been freed */

    return EXA_SUCCESS;
}


/**
 * Finalize the start of a group.
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group to start
 *
 * @return 0 on success, a negative error code on failure
 */
static int
vrt_cmd_group_start(const struct VrtGroupStart *cmd)
{
    vrt_group_t *group;
    int ret;

    exalog_debug("start group " UUID_FMT, UUID_VAL(&cmd->group_uuid));

    os_thread_mutex_lock(&pending_group_lock);
    if (!pending_group)
    {
	os_thread_mutex_unlock(&pending_group_lock);
	return -EPERM;
    }

    if (!uuid_is_equal(&cmd->group_uuid, &pending_group->uuid))
    {
	exalog_error("You are trying to create or start two groups at the same time.");
	os_thread_mutex_unlock(&pending_group_lock);
	return -EAGAIN;
    }

    ret = vrt_group_start(pending_group, &group);

    if (ret == EXA_SUCCESS)
    {
        ret = vrt_groups_list_add(group);
        if (ret != EXA_SUCCESS)
            vrt_group_stop(group);
    }

    os_free(pending_group);
    os_thread_mutex_unlock(&pending_group_lock);

    return ret;
}

/**
 * Finalize the insertion of a new rdev in a group.
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group to insert the new rdev in
 *
 * @return 0 on success, a negative error code on failure
 */
static int vrt_cmd_group_insert_rdev(const struct VrtGroupInsertRdev *cmd)
{
    int ret;

    exalog_debug("finish adding rdev in group " UUID_FMT,
            UUID_VAL(&cmd->group_uuid));

    os_thread_mutex_lock(&pending_group_lock);
    if (!pending_group)
    {
	os_thread_mutex_unlock(&pending_group_lock);
	return -EPERM;
    }

    if (!uuid_is_equal(&cmd->group_uuid, &pending_group->uuid))
    {
	exalog_error("You are trying to insert an rdev into a group "
                     "while a dgcreate, dgstart or dgdiskadd is running.");
	os_thread_mutex_unlock(&pending_group_lock);
	return -EAGAIN;
    }

    ret = vrt_group_insert_rdev(pending_group, &cmd->uuid, &cmd->nbd_uuid,
                                cmd->node_id, cmd->spof_id, cmd->local,
                                cmd->old_sb_version, cmd->new_sb_version);

    os_free(pending_group);
    os_thread_mutex_unlock(&pending_group_lock);

    return ret;
}


/**
 * Create a volume in a given group
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group in which the volume has to be created
 *  - Name of the volume to create
 *  - UUID of the volume to create
 *  - Size of the volume to create (in KB)
 *
 * @return 0 on success, a negative error code on failure
 */
static int
vrt_cmd_volume_create(const struct VrtVolumeCreate *cmd)
{
    vrt_group_t *group;
    vrt_volume_t *volume;
    int ret;

    EXA_ASSERT(cmd->volume_size > 0);

    exalog_debug("create volume '%s': size %" PRIu64 " KB",
                 cmd->volume_name, cmd->volume_size);

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (group == NULL)
    {
	exalog_debug("Unknown group " UUID_FMT, UUID_VAL(&cmd->group_uuid));
	return -VRT_ERR_UNKNOWN_GROUP_UUID;
    }


    /* !!! All sizes in 'cmd' are in KB and VRT internal functions want sizes in
     * sectors.
     */
    ret = vrt_group_create_volume(group, &volume, &cmd->volume_uuid, cmd->volume_name,
                                  KBYTES_2_SECTORS(cmd->volume_size));

    if (ret != EXA_SUCCESS)
    {
        exalog_error("Can't create volume '%s' in group '%s': %s(%d)",
                     cmd->volume_name, group->name, exa_error_msg(ret), ret);
	vrt_group_unref(group);
	return ret;
    }

    EXA_ASSERT(volume != NULL);

    /* wipe the newly created volume
     *
     * FIXME: This code is called from all the clients while it should be done
     * only once. To do so we should add a new RPC and trigger the wipping from
     * admind.
     */
    /* Let only one node (the first one) do the wipe */
    if (vrt_node_get_upnode_id() == 0)
    {
        ret = vrt_group_wipe_volume(group, volume);
        if (ret != EXA_SUCCESS)
        {
            exalog_error("Can't wipe volume '%s' in group '%s': %s(%d)",
                         volume->name, group->name, exa_error_msg(ret), ret);
            /* Rollback volume creation */
            vrt_group_delete_volume(group, volume);
            vrt_group_unref(group);
            return ret;
        }
    }

    vrt_group_unref(group);
    return EXA_SUCCESS;
}


/**
 * Sync metadata on disk.
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group
 *
 * @return 0 on success, a negative error code on failure
 */
static int vrt_cmd_group_sync_sb(const struct VrtGroupSyncSb *cmd)
{
    struct vrt_group *group;
    int ret;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if(!group)
    {
	exalog_error("Unknown group " UUID_FMT, UUID_VAL(&cmd->group_uuid));
	return -VRT_ERR_UNKNOWN_GROUP_UUID;
    }

    ret = vrt_group_sync_sb(group, cmd->old_sb_version, cmd->new_sb_version);
    vrt_group_unref(group);

    return ret;
}


/**
 * Freeze a group: block incoming IO and wait current IO to be finished
 *
 * @param[in] cmd       The msg containing parameters for freezing the group
 *
 * @return 0 on success, a negative error code on failure
 */
static int
vrt_cmd_group_freeze(const struct VrtGroupFreeze *cmd)
{
    struct vrt_group *group;
    int i;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if(!group)
    {
	exalog_debug("Unknown group " UUID_FMT, UUID_VAL(&cmd->group_uuid));
	return -VRT_ERR_UNKNOWN_GROUP_UUID;
    }

    for (i = 0 ; i < NBMAX_VOLUMES_PER_GROUP ; i++)
    {
	struct vrt_volume *volume = group->volumes[i];

	if (! volume)
	    continue;

	volume->frozen = TRUE;
	wait_event(volume->cmd_wq,
		   os_atomic_read(&volume->inprogress_request_count) == 0);
    }

    vrt_group_unref(group);

    return EXA_SUCCESS;
}


/**
 * Unfreeze a group: resume IO previously blocked by freeze
 *
 * @param[in] cmd       The msg containing parameters for freezing the group
 *
 * @return 0 on success, a negative error code on failure
 */
int
vrt_cmd_group_unfreeze(const struct VrtGroupUnfreeze *cmd)
{
    struct vrt_group *group;
    int i;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if(!group)
    {
	exalog_debug("Unknown group " UUID_FMT, UUID_VAL(&cmd->group_uuid));
	return -VRT_ERR_UNKNOWN_GROUP_UUID;
    }

    for (i = 0 ; i < NBMAX_VOLUMES_PER_GROUP ; i++)
    {
	struct vrt_volume *volume = group->volumes[i];

	if (! volume)
	    continue;

	volume->frozen = FALSE;
	wake_up_all(&volume->frozen_req_wq);
    }

    vrt_group_unref(group);

    return EXA_SUCCESS;
}


/**
 * Start a volume.
 *
 * @param[in] cmd       : A struct VrtVolumeStart
 *
 * @return EXA_SUCCESS or a negative error code
 */
static int vrt_cmd_volume_start(const struct VrtVolumeStart *cmd)
{
    struct vrt_group *group;
    struct vrt_volume *volume;
    int error;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (!group)
	return -VRT_ERR_GROUP_NOT_STARTED;

    volume = vrt_group_find_volume(group, &cmd->volume_uuid);
    if (! volume)
    {
	exalog_debug("volume '" UUID_FMT "' not found in group '" UUID_FMT "'",
                     UUID_VAL(&cmd->volume_uuid), UUID_VAL(&group->uuid));
	vrt_group_unref(group);
	return -VRT_ERR_UNKNOWN_VOLUME_UUID;
    }

    error = vrt_volume_start(volume);

    vrt_group_unref(group);

    return error;
}

/**
 * Stop a volume
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group in which is located the volume to stop
 *  - UUID of the volume to stop
 *
 * @return 0 on success, a negative error code on failure
 */
static int
vrt_cmd_volume_stop(const struct VrtVolumeStop *cmd)
{
    struct vrt_group *g;
    struct vrt_volume *volume;
    int ret;

    g = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (g == NULL)
	return -VRT_ERR_UNKNOWN_GROUP_UUID;

    volume = vrt_group_find_volume(g, &cmd->volume_uuid);
    if (! volume)
    {
	vrt_group_unref(g);
        return -VRT_ERR_UNKNOWN_VOLUME_UUID;
    }

    if (volume->status == EXA_VOLUME_STOPPED)
    {
	vrt_group_unref(g);
	return -VRT_ERR_VOLUME_NOT_STARTED;
    }

    ret = vrt_volume_stop (volume);
    if (ret != EXA_SUCCESS)
    {
	vrt_group_unref(g);
	return ret;
    }

    vrt_group_unref(g);

    return EXA_SUCCESS;
}


/**
 * Resize a volume
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of the group in which is located the volume to resize
 *  - UUID of the volume to resize
 *  - New size of the volume (in KB)
 *
 * @return 0 on success, a negative error code on failure
 */
static int
vrt_cmd_volume_resize(const struct VrtVolumeResize *cmd)
{
    struct vrt_group *group;
    struct vrt_volume *volume;
    int ret;

    EXA_ASSERT(cmd->volume_size > 0);

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (! group)
	return -VRT_ERR_UNKNOWN_GROUP_UUID;

    volume = vrt_group_find_volume(group, &cmd->volume_uuid);
    if (! volume)
    {
	vrt_group_unref(group);
	return -VRT_ERR_UNKNOWN_VOLUME_UUID;
    }

    /* !!! All sizes in 'cmd' are in KB and VRT internal functions want sizes in
     * sectors.
     */
    ret = vrt_volume_resize(volume, KBYTES_2_SECTORS(cmd->volume_size),
                            group->storage);

    if (ret != EXA_SUCCESS)
    {
	vrt_group_unref(group);
	return ret;
    }

    vrt_group_unref(group);
    return ret;
}


/**
 * Delete a volume (volume must be stopped to do so)
 *
 * @param[in] params The parsed command array
 *
 * The real parameters passed in the array are:
 *  - UUID of group in which is located the volume
 *  - UUID of the volume to delete
 *
 * @return 0 on success, a negative error code on failure
 */
static int
vrt_cmd_volume_delete(const struct VrtVolumeDelete *cmd)
{
    struct vrt_group *group;
    struct vrt_volume *volume;
    int ret;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (group == NULL)
	return -VRT_ERR_UNKNOWN_GROUP_UUID;

    volume = vrt_group_find_volume(group, &cmd->volume_uuid);
    if (volume == NULL)
    {
	vrt_group_unref(group);
	return -VRT_ERR_UNKNOWN_VOLUME_UUID;
    }

    ret = vrt_group_delete_volume(group, volume);

    vrt_group_unref(group);

    return ret;
}

static int
vrt_cmd_device_replace(const struct VrtDeviceReplace *cmd)
{
    struct vrt_group *group;
    struct vrt_realdev *rdev;
    int ret;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (group == NULL)
        return -VRT_ERR_UNKNOWN_GROUP_UUID;

    rdev = storage_get_rdev(group->storage, &cmd->vrt_uuid);
    if (rdev == NULL)
    {
        exalog_error("Cannot find vrt UUID " UUID_FMT " in group '%s'",
                     UUID_VAL(&cmd->vrt_uuid), group->name);
        vrt_group_unref(group);
        return -VRT_ERR_NO_SUCH_RDEV_IN_GROUP;
    }

    if (!vrt_group_supports_device_replacement(group))
    {
	exalog_error("Group '%s' (layout '%s') does not support disk replacement",
                     group->name, group->layout->name);
        vrt_group_unref(group);
	return -VRT_ERR_DISK_REPLACEMENT_NOT_SUPPORTED;
    }

    ret = vrt_group_rdev_replace(group, rdev, &cmd->rdev_uuid);

    vrt_group_unref(group);

    return ret;
}

static int
vrt_cmd_get_volume_status(const struct VrtGetVolumeStatus *cmd)
{
    struct vrt_group *group;
    struct vrt_volume *volume;
    int ret;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (!group)
	return -VRT_ERR_UNKNOWN_GROUP_UUID;

    volume = vrt_group_find_volume(group, &cmd->volume_uuid);
    if (! volume)
    {
	vrt_group_unref(group);
	return -VRT_ERR_UNKNOWN_VOLUME_UUID;
    }

    ret = vrt_volume_get_status(volume);

    vrt_group_unref(group);

    return ret;
}


/* parts to handle nodes events */
static int
vrt_cmd_node_set_upnodes(const struct VrtSetNodesStatus *event_msg)
{
    vrt_node_set_upnodes(&event_msg->nodes_up);

    return EXA_SUCCESS;
}

/* parts to handle devices events, cases REBUILD and RECOVER are
   missing */
static int
vrt_cmd_device_event(const struct VrtDeviceEvent *event_msg)
{
    int retval = -EINVAL;
    struct vrt_group *group;
    struct vrt_realdev *rdev;

    group = vrt_get_group_from_uuid(&event_msg->group_uuid);
    if (!group)
    {
	exalog_debug("group " UUID_FMT " not found",
                     UUID_VAL(&event_msg->group_uuid));
	return -VRT_ERR_UNKNOWN_GROUP_UUID;
    }

    rdev = storage_get_rdev(group->storage, &event_msg->rdev_uuid);
    if (!rdev)
    {
	exalog_debug("rdev " UUID_FMT " not found", UUID_VAL(&event_msg->rdev_uuid));
	return -VRT_ERR_OLD_RDEVS_MISSING;
    }

    switch(event_msg->event)
    {
    case VRT_DEVICE_DOWN:
	retval = vrt_group_rdev_down(group, rdev);
	break;

    case VRT_DEVICE_UP:
	retval = vrt_group_rdev_up(group, rdev);
	break;

    case VRT_DEVICE_REINTEGRATE:
	retval = vrt_group_reintegrate_rdev(group, rdev);
	break;

    case VRT_DEVICE_POST_REINTEGRATE:
	retval = vrt_group_post_reintegrate_rdev(group, rdev);
	break;

    default :
	EXA_ASSERT_VERBOSE(0, "struct VrtDeviceEvent: Unknown event type %d\n",
			   event_msg->event);
    }

    vrt_group_unref(group);

    return retval;
}


static int
vrt_cmd_group_event(const struct VrtGroupEvent *event_msg)
{
    int retval = -EINVAL;
    struct vrt_group *group;

    group = vrt_get_group_from_uuid(&event_msg->group_uuid);
    if (!group)
    {
	exalog_debug("group " UUID_FMT " not found",
                     UUID_VAL(&event_msg->group_uuid));
	return -VRT_ERR_UNKNOWN_GROUP_UUID;
    }
    switch(event_msg->event)
    {
    case VRT_GROUP_RESUME:
	retval = vrt_group_resume(group);
	break;

    case VRT_GROUP_SUSPEND_METADATA_AND_REBUILD:
        vrt_group_metadata_thread_suspend(group);
        vrt_group_rebuild_thread_suspend(group);
        retval = 0;
	break;

    case VRT_GROUP_RESUME_METADATA_AND_REBUILD:
        vrt_group_metadata_thread_resume(group);
        vrt_group_rebuild_thread_resume(group);
        retval = 0;
	break;

    case VRT_GROUP_COMPUTESTATUS:
	retval = vrt_group_compute_status(group);
	break;

    case VRT_GROUP_WAIT_INITIALIZED_REQUESTS:
	vrt_group_wait_initialized_requests (group);
	retval = EXA_SUCCESS;
	break;

    case VRT_GROUP_POSTRESYNC:
	retval = vrt_group_post_resync(group);
	break;

    default :
	EXA_ASSERT_VERBOSE(0, "struct VrtGroupEvent: Unknown event type %d\n",
			   event_msg->event);
    }

    vrt_group_unref(group);

    return retval;
}


static int vrt_cmd_group_reset(const struct VrtGroupReset *cmd)
{
    struct vrt_group *group;
    int ret;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (group == NULL)
        return -VRT_ERR_UNKNOWN_GROUP_UUID;

    ret = vrt_group_reset(group);
    if (ret != EXA_SUCCESS)
        exalog_error("Reset failed with %d", ret);

    vrt_group_unref(group);

    return ret;
}



static int vrt_cmd_group_check(const struct VrtGroupCheck *cmd)
{

    struct vrt_group *group;
    int ret;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (group == NULL)
        return -VRT_ERR_UNKNOWN_GROUP_UUID;

    ret = vrt_group_check(group);
    if (ret != EXA_SUCCESS)
        exalog_error("Check failed with %d", ret);

    vrt_group_unref(group);

    return ret;
}

static int vrt_cmd_group_resync(const struct vrt_group_resync_request *cmd)
{

    struct vrt_group *group;
    int err;

    group = vrt_get_group_from_uuid(&cmd->group_uuid);
    if (group == NULL)
        return -VRT_ERR_UNKNOWN_GROUP_UUID;

    err = vrt_group_resync(group, &cmd->nodes);
    if (err != EXA_SUCCESS)
        exalog_error("Resync failed: %s (%d)", exa_error_msg(err), err);

    vrt_group_unref(group);

    return err;
}

void vrt_cmd_handle_message_init(void)
{
    os_thread_mutex_init(&pending_group_lock);
}

void vrt_cmd_handle_message_clean(void)
{
    os_thread_mutex_destroy(&pending_group_lock);
}

void vrt_cmd_handle_message(const vrt_cmd_t *recv, vrt_reply_t *reply)
{

    EXA_ASSERT_VERBOSE(VRTRECV_TYPE_IS_VALID(recv->type),
		       "Data type %d is unknown.", recv->type);
    switch (recv->type)
    {
    case VRTRECV_NODE_SET_UPNODES:
	reply->retval = vrt_cmd_node_set_upnodes(&recv->d.vrt_set_nodes_status);
	break;

    case VRTRECV_DEVICE_EVENT:
	reply->retval = vrt_cmd_device_event(&recv->d.vrt_device_event);
	break;

    case VRTRECV_DEVICE_REPLACE:
        reply->retval = vrt_cmd_device_replace(&recv->d.vrt_device_replace);
        break;

    case VRTRECV_GET_VOLUME_STATUS:
	reply->retval = vrt_cmd_get_volume_status(&recv->d.vrt_get_volume_status);
	break;

    case VRTRECV_GROUP_ADD_RDEV:
	reply->retval = vrt_cmd_group_add_rdev(&recv->d.vrt_group_add_rdev);
	break;

    case VRTRECV_GROUP_BEGIN:
	reply->retval = vrt_cmd_group_begin(&recv->d.vrt_group_begin);
	break;

    case VRTRECV_GROUP_CREATE:
	reply->retval = vrt_cmd_group_create(&recv->d.vrt_group_create, &reply->group_create);
	break;

    case VRTRECV_GROUP_EVENT:
	reply->retval = vrt_cmd_group_event(&recv->d.vrt_group_event);
	break;

    case VRTRECV_GROUP_START:
	reply->retval = vrt_cmd_group_start(&recv->d.vrt_group_start);
	break;

    case VRTRECV_GROUP_STOP:
	reply->retval = vrt_cmd_group_stop(&recv->d.vrt_group_stop);
	break;

    case VRTRECV_GROUP_INSERT_RDEV:
	reply->retval = vrt_cmd_group_insert_rdev(&recv->d.vrt_group_insert_rdev);
	break;

    case VRTRECV_GROUP_STOPPABLE:
	reply->retval = vrt_cmd_group_stoppable(&recv->d.vrt_group_stoppable);
	break;

    case VRTRECV_GROUP_GOING_OFFLINE:
	reply->retval = vrt_cmd_group_going_offline(&recv->d.vrt_group_going_offline);
	break;

    case VRTRECV_GROUP_SYNC_SB:
	reply->retval = vrt_cmd_group_sync_sb(&recv->d.vrt_group_sync_sb);
	break;

    case VRTRECV_GROUP_FREEZE:
	reply->retval = vrt_cmd_group_freeze(&recv->d.vrt_group_freeze);
	break;

    case VRTRECV_GROUP_UNFREEZE:
	/* group unfreeze has been catched by the multiplexer thread */
	EXA_ASSERT(FALSE);
	break;

    case VRTRECV_VOLUME_CREATE:
	reply->retval = vrt_cmd_volume_create(& recv->d.vrt_volume_create);
	break;

    case VRTRECV_VOLUME_DELETE:
	reply->retval = vrt_cmd_volume_delete(& recv->d.vrt_volume_delete);
	break;

    case VRTRECV_VOLUME_RESIZE:
	reply->retval = vrt_cmd_volume_resize(& recv->d.vrt_volume_resize);
	break;

    case VRTRECV_VOLUME_START:
	reply->retval = vrt_cmd_volume_start(& recv->d.vrt_volume_start);
	break;

    case VRTRECV_VOLUME_STOP:
	reply->retval = vrt_cmd_volume_stop(& recv->d.vrt_volume_stop);
	break;

    case VRTRECV_PENDING_GROUP_CLEANUP:
	reply->retval = vrt_cmd_pending_group_cleanup();
	break;

    case VRTRECV_GROUP_RESET:
	reply->retval = vrt_cmd_group_reset(& recv->d.vrt_group_reset);
	break;

    case VRTRECV_GROUP_CHECK:
	reply->retval = vrt_cmd_group_check(& recv->d.vrt_group_check);
	break;

    case VRTRECV_GROUP_RESYNC:
	reply->retval = vrt_cmd_group_resync(&recv->d.vrt_group_resync);
	break;

    case VRTRECV_ASK_INFO:
    case VRTRECV_STATS:
	EXA_ASSERT_VERBOSE(FALSE,
		"Type %s (%d) Should not be handled by this thread\n",
		recv->type == VRTRECV_STATS ?  "stats" : "info", recv->type);
    }
}

