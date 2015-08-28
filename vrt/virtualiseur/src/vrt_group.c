/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>

#include "common/include/daemon_request_queue.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"
#include "common/include/exa_names.h"
#include "common/include/exa_nodeset.h"
#include "common/include/exa_constants.h"

#include "log/include/log.h"

#include "os/include/os_mem.h"
#include "os/include/os_atomic.h"
#include "os/include/os_error.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"

#include "vrt/common/include/waitqueue.h"
#include "vrt/virtualiseur/include/vrt_cmd_threads.h"
#include "vrt/virtualiseur/include/vrt_metadata.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_rebuild.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_request.h"

#include "vrt/virtualiseur/src/vrt_module.h"

void vrt_group_wait_initialized_requests(struct vrt_group *group)
{
    wait_event(group->recover_wq,
	       os_atomic_read(&group->initialized_request_count) == 0);
}

/**
 * Remove a real device from a group.
 *
 * @param rdev: the real device to remove
 */
static void vrt_group_del_rdev(struct vrt_group *group, struct vrt_realdev *rdev)
{
    EXA_ASSERT(storage_del_rdev(group->storage, rdev->spof_id, rdev) == 0);

    vrt_rdev_free(rdev);
}

/**
 * Remove all real devices from a group.
 */
static void vrt_group_del_rdevs(struct vrt_group *group)
{
    int i;

    if (group->storage == NULL)
        return;

    /* FIXME This should be done in storage_, but each vrt_group_del_rdev
     * calls group->layout->rdev_del. */
    for (i = 0; i < group->storage->num_spof_groups; i++)
    {
        spof_group_t *spof_group = &group->storage->spof_groups[i];

        while (spof_group->nb_realdevs > 0)
            vrt_group_del_rdev(group, spof_group->realdevs[0]);
    }
}

static void vrt_group_free_all_volumes(struct vrt_group *group)
{
    unsigned int i;

    for (i = 0; i < NBMAX_VOLUMES_PER_GROUP; i++)
    {
        if (group->volumes[i] == NULL)
            continue;

        vrt_volume_free(group->volumes[i]);
        group->nb_volumes--;
    }
}

/**
 * Insert a volume in a group.
 *
 * The volume *must not* be already part of (i.e., inserted in) a group.
 *
 * @param group   Group to insert into
 * @param volume  Volume to insert
 *
 * @return EXA_SUCCESS if inserted succesfully, -VRT_ERR_NB_VOLUMES_CREATED
 *         otherwise
 */
static int __insert_volume(struct vrt_group *group, struct vrt_volume *volume)
{
    int index;

    EXA_ASSERT(volume->group == NULL);

    for (index = 0; index < NBMAX_VOLUMES_PER_GROUP; index++)
	if (group->volumes[index] == NULL)
        {
            volume->group = group;

            group->volumes[index] = volume;
            group->nb_volumes++;

            return EXA_SUCCESS;
        }

    return -VRT_ERR_NB_VOLUMES_CREATED;
}

/**
 * Remove a volume from a group.
 *
 * The volume *must* be part of the group.
 *
 * @param group   Group to remove from
 * @param volume  Volume to remove
 */
static void __remove_volume(struct vrt_group *group, struct vrt_volume *volume)
{
    int index;

    EXA_ASSERT(volume->group == group);

    for (index = 0; index < NBMAX_VOLUMES_PER_GROUP; index++)
	if (group->volumes[index] == volume)
        {
            volume->group = NULL;

            group->volumes[index] = NULL;
            group->nb_volumes--;

            return;
        }

    EXA_ASSERT_VERBOSE(false, "No volume '%s' in group '%s'", volume->name,
                       group->name);
}

struct vrt_volume *vrt_group_find_volume(const struct vrt_group *group,
                                         const exa_uuid_t *uuid)
{
    int i;

    if (group == NULL)
	return NULL;

    for (i = 0; i < NBMAX_VOLUMES_PER_GROUP; i++)
	if (group->volumes[i] != NULL
            && uuid_is_equal(&group->volumes[i]->uuid, uuid))
	    return group->volumes[i];

    return NULL;
}

/**
 * Find a volume in a group using its name.
 *
 * @param[in] group  Group in which the volume is to be searched
 * @param[in] name   Name of the volume to look for
 *
 * @return volume if found, NULL otherwise
 */
static struct vrt_volume *vrt_group_find_volume_by_name(struct vrt_group *group,
                                                        const char *volume_name)
{
    int i;

    if (group == NULL)
	return NULL;

    for (i = 0; i < NBMAX_VOLUMES_PER_GROUP; i++)
    {
        struct vrt_volume *vol = group->volumes[i];
	if (vol != NULL && strncmp(vol->name, volume_name, EXA_MAXSIZE_VOLUMENAME) == 0)
	    return vol;
    }

    return NULL;
}

int vrt_group_wipe_volume(vrt_group_t *group, vrt_volume_t *volume)
{
    int ret;

    ret = vrt_volume_wipe(volume);

    if (ret == -EIO && group->status == EXA_GROUP_OFFLINE)
        ret = EXA_SUCCESS;

    return ret;
}

int vrt_group_create_volume(vrt_group_t *group, vrt_volume_t **volume,
                            const exa_uuid_t *uuid, const char *name,
                            uint64_t size)
{
    assembly_volume_t *av;
    int ret;

    *volume = NULL;

    EXA_ASSERT(group);

    if (group->status == EXA_GROUP_OFFLINE)
        return -VRT_ERR_GROUP_OFFLINE;

    /* FIXME Now that the group is the instigator of the volume's creation,
             it could check that there is enough space in the group to create
             a volume of the requested size.
             Although this check should also consider the license's maximum
             capacity, so it's not completely stupid that this is done by
             admind */
    /* The caller must check that size is != 0, and that there's enough
       space in the group. */
    EXA_ASSERT(size != 0);

    if (group->nb_volumes >= NBMAX_VOLUMES_PER_GROUP)
    {
	exalog_debug("maximum number of volumes (%d) reached for group '%s'",
                     NBMAX_VOLUMES_PER_GROUP, group->name);
	return -VRT_ERR_NB_VOLUMES_CREATED;
    }

    if (vrt_group_find_volume_by_name(group, name) != NULL)
    {
        exalog_debug("Volume '%s' already exists in group '%s' ("UUID_FMT")", name,
                     group->name, UUID_VAL(&group->uuid));
        return -VRT_ERR_VOLUMENAME_USED;
    }

    /* We use the volume's UUID for the subspace too, so that it is identical
       on all nodes and thus easier to debug. (There is no hard constraint:
       the subspace UUID could be different from the volume's and also different
       across nodes.) */
    ret = group->layout->create_subspace(group->layout_data, uuid, size, &av,
                                         group->storage);
    if (ret != 0)
        return ret;

    *volume = vrt_volume_alloc(uuid, name, size);
    if (*volume == NULL)
    {
	exalog_error("Failed allocating new volume");
        group->layout->delete_subspace(group->layout_data, &av,
                                       group->storage);
	return -ENOMEM;
    }

    /* XXX Not nice. Better way to set it? */
    (*volume)->assembly_volume = av;

    __insert_volume(group, *volume);

    return EXA_SUCCESS;
}

/**
 * Delete a volume from a group.
 *
 * @param group   Group to delete from
 * @param volume  Volume to delete
 *
 * @return EXA_SUCCESS if the volume was successfully deleted, a negative
 *         error code otherwise
 */
int vrt_group_delete_volume(struct vrt_group *group, struct vrt_volume *volume)
{
    assembly_volume_t *av;

    EXA_ASSERT(volume->group == group);

    if (volume->status != EXA_VOLUME_STOPPED)
	return -VRT_ERR_VOLUME_ALREADY_STARTED;

    __remove_volume(group, volume);

    av = volume->assembly_volume;

    vrt_volume_free(volume);

    group->layout->delete_subspace(group->layout_data, &av,
                                   group->storage);

    return EXA_SUCCESS;
}

/**
 * Alloc and initialize the group group_name in the virtualizer.
 *
 * @param[in]  group_name   Name of the group
 * @param[in]  group_uuid   UUID of the group
 * @param[in]  layout       Layout to use
 *
 * @return the new group, or NULL on error
 */
vrt_group_t *vrt_group_alloc(const char *group_name, const exa_uuid_t *group_uuid,
                             const struct vrt_layout *layout)
{
    struct vrt_group *group;

    EXA_ASSERT(group_name != NULL);
    EXA_ASSERT(group_uuid != NULL);
    EXA_ASSERT(layout != NULL);

    group = os_malloc(sizeof(struct vrt_group));
    if (!group)
	return NULL;

    /* FIXME Initialize group members in the order they are defined
       and get rid of the memset() */

    memset(group, 0, sizeof(struct vrt_group));

    init_waitqueue_head(&group->suspended_req_wq);
    init_waitqueue_head(&group->recover_wq);

    init_waitqueue_head(&group->metadata_thread.wq);
    os_atomic_set(&group->metadata_thread.running, 0);

    init_waitqueue_head(&group->rebuild_thread.wq);
    os_atomic_set(&group->rebuild_thread.running, 0);

    group->layout = layout;
    uuid_copy(&group->uuid, group_uuid);
    os_strlcpy(group->name, group_name, EXA_MAXSIZE_GROUPNAME + 1);
    group->status = EXA_GROUP_OFFLINE;
    group->suspended = TRUE;
    os_atomic_set(&group->count, 0);
    os_atomic_set(&group->initialized_request_count, 0);

    os_thread_rwlock_init(&group->status_lock);
    os_thread_rwlock_init(&group->suspend_lock);

    group->sb_version = 0;

    return group;
}


/**
 * Free a group.
 */
void __vrt_group_free(struct vrt_group *group)
{
    if (group == NULL)
        return;

    group->layout->group_cleanup(&group->layout_data, group->storage);

    vrt_group_free_all_volumes(group);
    vrt_group_del_rdevs(group);

    storage_free(group->storage);

    os_thread_rwlock_destroy(&group->status_lock);
    os_thread_rwlock_destroy(&group->suspend_lock);

    os_free(group);
}

bool vrt_group_ref(struct vrt_group *group)
{
    /* This logic has been imported from its original location, in
       vrt_get_group_from_name_unsafe() */

    os_atomic_inc(&group->count);

    return true;
}

void vrt_group_unref(struct vrt_group *group)
{
    EXA_ASSERT(group != NULL);
    os_atomic_dec(&group->count);
}

/**
 * Synchronize the superblocks on all local disks.
 *
 * @return EXA_SUCCESS or a negative error code
 */
int vrt_group_sync_sb(struct vrt_group *group, uint64_t old_sb_version,
                      uint64_t new_sb_version)
{
    storage_rdev_iter_t iter;
    vrt_realdev_t *rdev;
    int err = 0;

    group->sb_version = new_sb_version;

    storage_rdev_iterator_begin(&iter, group->storage);

    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
        superblock_write_op_t op;

        if (!rdev_is_local(rdev) || !rdev_is_ok(rdev))
            continue;

        err = vrt_rdev_begin_superblock_write(rdev, old_sb_version, new_sb_version, &op);
        if (err != 0)
            break;

        err = vrt_group_serialize(group, op.stream);
        if (err != 0)
            break;

        err = vrt_rdev_end_superblock_write(rdev, &op);
        if (err != 0)
            break;
    }

    storage_rdev_iterator_end(&iter);

    if (err == -ENOSPC)
        exalog_error("Superblock too small to store serialized group '%s'",
                     group->name);

    return err;
}

void vrt_group_info_init(vrt_group_info_t *info)
{
    vrt_group_layout_info_t *layout_info;

    info->name[0]                   = '\0';
    info->layout_name[0]            = '\0';
    uuid_zero(&info->uuid);
    info->sb_version                = 0;
    info->nb_rdevs                  = 0;

    layout_info = &info->layout_info;

    layout_info->is_set             = false;
    layout_info->slot_width         = 0;
    layout_info->chunk_size         = 0;
    layout_info->su_size            = 0;
    layout_info->dirty_zone_size    = 0;
    layout_info->blended_stripes    = false;
    layout_info->nb_spares          = 0;
}

/**
 * Used to free rdevs added to a storage on error path from
 * storage_build_from_description().
 *
 * @param[in]   storage     The storage
 *
 * @attention Do not call this if the rdevs are someone else's
 * property.
 */
static void __storage_free_rdevs(storage_t *storage)
{
    const struct vrt_realdev *rdev;
    storage_rdev_iter_t iter;

    storage_rdev_iterator_begin(&iter, storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
        vrt_realdev_t *my_rdev = (vrt_realdev_t *)rdev;
        vrt_rdev_free(my_rdev);
    }
    storage_rdev_iterator_end(&iter);
}

/* FIXME Should be in storage.c, if only it didn't take in a whole plate
 * of rdev spaghetti with it.
 */
/**
 * build the storage's SPOF group topology from the rdevs descriptions
 *
 * @param[out]  storage     The newly-allocated storage
 * @param[in]   nb_rdevs    The number of realdevs
 * @param[in]   rdevs       The rdevs
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int storage_build_from_description(storage_t **storage, int nb_rdevs,
                                          const vrt_rdev_info_t *rdevs)
{
    vrt_realdev_t *rdev;
    int i;

    if (nb_rdevs > NBMAX_DISKS_PER_GROUP)
        return -VRT_ERR_NB_RDEVS_IN_GROUP;

    if (nb_rdevs == 0)
        return -VRT_ERR_NO_RDEV_IN_GROUP;

    /* Allocate storage */
    (*storage) = storage_alloc();
    if ((*storage) == NULL)
        return -ENOMEM;

    /* Add rdevs to the storage */
    for (i = 0; i < nb_rdevs; i++)
    {
        const vrt_rdev_info_t *rdev_info = &rdevs[i];
        int err;

        rdev = vrt_rdev_new(rdev_info->node_id, rdev_info->spof_id,
                            &rdev_info->uuid, &rdev_info->nbd_uuid,
                            i, rdev_info->local, rdev_info->up);
        if (rdev == NULL)
        {
            __storage_free_rdevs((*storage));
            storage_free((*storage));
            return -ENOMEM;
        }

        err = vrt_rdev_open(rdev);
        if (err != 0)
        {
            vrt_rdev_free(rdev);
            __storage_free_rdevs((*storage));
            storage_free((*storage));
            return err;
        }

        err = storage_add_rdev((*storage), rdev->spof_id, rdev);
        if (err != 0)
        {
            vrt_rdev_free(rdev);
            __storage_free_rdevs((*storage));
            storage_free((*storage));
            return err;
        }
    }

    return 0;
}

int vrt_group_build_from_description(vrt_group_t **group,
                                     const vrt_group_info_t *group_desc,
                                     char *error_msg)
{
    const struct vrt_layout *layout;
    const vrt_group_layout_info_t *layout_info;
    struct vrt_realdev *rdev;
    storage_t *storage;
    storage_rdev_iter_t iter;
    int err;

    EXA_ASSERT(group_desc->sb_version > 0);

    layout_info = &group_desc->layout_info;
    EXA_ASSERT(layout_info->is_set);

    error_msg[0] = '\0';

    layout = vrt_get_layout(group_desc->layout_name);
    if (layout == NULL)
        return -VRT_ERR_UNKNOWN_LAYOUT;

    if (layout_info->chunk_size < KBYTES_2_SECTORS(VRT_MIN_CHUNK_SIZE))
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "The chunk size must be greater than or equal to %u KiB",
                    VRT_MIN_CHUNK_SIZE);
	return -EINVAL;
    }

    err = storage_build_from_description(&storage, group_desc->nb_rdevs,
                                         group_desc->rdevs);
    if (err != 0)
        return err;

    *group = vrt_group_alloc(group_desc->name, &group_desc->uuid, layout);
    if (*group == NULL)
    {
        storage_free(storage);
	return -ENOMEM;
    }

    (*group)->sb_version = group_desc->sb_version;
    (*group)->storage = storage;

    /* Verify that all the disks of the group are UP, and mark them uncorrupted */
    storage_rdev_iterator_begin(&iter, (*group)->storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
	if (!rdev_is_up(rdev))
        {
            storage_rdev_iterator_end(&iter);
            os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1, "A disk is down.");
	    err = -VRT_ERR_RDEV_DOWN;
            goto failed;
        }

        rdev->corrupted = FALSE;
    }
    storage_rdev_iterator_end(&iter);

    err = storage_cut_in_chunks(storage, SECTORS_2_KBYTES(layout_info->chunk_size));
    if (err == -VRT_ERR_RDEV_TOO_SMALL)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "The chunk size is too big (%" PRIu32 " KiB)\n",
		 SECTORS_2_KBYTES(layout_info->chunk_size));
	goto failed;
    }
    else if (err == -VRT_ERR_TOO_MANY_CHUNKS)
    {
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1,
		 "The disk group contains too many chunks. "
		 "You should increase the chunk size (current value: %" PRIu32 " KiB).\n",
		 SECTORS_2_KBYTES(layout_info->chunk_size));
        goto failed;
    }
    EXA_ASSERT(err == EXA_SUCCESS);

    exalog_debug("Creating group %s with layout %s", (*group)->name,
                 (*group)->layout->name);
    err = (*group)->layout->group_create((*group)->storage,
                                         &(*group)->layout_data,
                                         layout_info->slot_width,
                                         layout_info->chunk_size,
                                         layout_info->su_size,
                                         layout_info->dirty_zone_size,
                                         layout_info->blended_stripes ? 1 : 0, /* FIXME */
                                         layout_info->nb_spares,
                                         error_msg);
    if (err != EXA_SUCCESS)
        goto failed;

    return 0;

failed:
    /* FIXME The rdev superblocks written should be erased  */

    /* The group data isn't kept in memory: it was necessary just for writing
       the group info in the disks' superblocks. The in-memory group data
       will be recreated when the group is started. */
    vrt_group_free(*group);

    return err;
}

/**
 * Create a new group.
 *
 * All sizes are in sectors
 *
 * @param group the group to create
 *
 * @return       error code
 */
int vrt_group_create(const vrt_group_info_t *group_description, char *error_msg)
{
    vrt_group_t *group = NULL;
    storage_rdev_iter_t iter;
    struct vrt_realdev *rdev;
    int err;

    err = vrt_group_build_from_description(&group, group_description, error_msg);
    if (err != 0)
        return err;

    /* Create superblocks on all devices */
    /* FIXME We should set error_msg here */
    storage_rdev_iterator_begin(&iter, group->storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
        if (rdev->local)
        {
            err = vrt_rdev_create_superblocks(rdev);
            if (err != 0)
            {
                storage_rdev_iterator_end(&iter);
                goto cleanup;
            }
        }
    storage_rdev_iterator_end(&iter);

    /* There's no old version at this point so we use zero, which is the
       "none" version number.  */
    err = vrt_group_sync_sb(group, 0, group->sb_version);

cleanup:
    /* FIXME The rdev superblocks written should be erased? */

    /* The group data isn't kept in memory: it was necessary just for writing
       the group info in the disks' superblocks. The in-memory group data
       will be recreated when the group is started. */
    vrt_group_free(group);

    return err;
}

static int vrt_group_read_sb(vrt_group_t **group, storage_t *storage,
                             const exa_uuid_t *group_uuid, uint64_t sb_version)
{
    vrt_realdev_t *rdev;
    storage_rdev_iter_t iter;
    int err = -VRT_ERR_NO_RDEV_TO_READ;

    storage_rdev_iterator_begin(&iter, storage);

    while (err != 0 && (rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
        superblock_read_op_t op;

        /* We don't want to try and read a superblock from a dead realdev */
        if (!rdev_is_ok(rdev))
            continue;

        err = vrt_rdev_begin_superblock_read(rdev, sb_version, &op);
        if (err == 0)
            err = vrt_group_deserialize(group, storage, op.stream, group_uuid);

        if (err != 0)
            exalog_warning("Failed deserializing group on rdev "UUID_FMT":"
                           " %s (%d)", UUID_VAL(&rdev->uuid), exa_error_msg(err),
                           err);

        if (err == 0)
            err = vrt_rdev_end_superblock_read(rdev, &op);

        if (err == -VRT_ERR_SB_UUID_MISMATCH)
            rdev->corrupted = TRUE;
    }

    storage_rdev_iterator_end(&iter);

    if (err == VRT_ERR_SB_NOT_FOUND)
        exalog_error("Superblock with version %"PRIu64" not found", sb_version);

    if (err != 0)
        exalog_error("Failed to read superblocks: %s (%d)\n",
                     exa_error_msg(err), err);

    return err;
}

/**
 * Start a group.
 */
int vrt_group_start(const vrt_group_info_t *group_description,
                    vrt_group_t **started_group)
{
    vrt_group_t *group;
    vrt_realdev_t *rdev;
    long i;
    storage_rdev_iter_t iter;
    int ret;
    storage_t *storage;

    EXA_ASSERT(!group_description->layout_info.is_set);

    group = NULL;

    ret = storage_build_from_description(&storage,
                                         group_description->nb_rdevs,
                                         group_description->rdevs);
    if (ret != 0)
        return ret;

    ret = vrt_group_read_sb(&group, storage, &group_description->uuid,
                            group_description->sb_version);
    if (ret != 0)
    {
        storage_free(storage);
        return ret;
    }

    exalog_debug("start group '%s' with sb_version = %" PRIu64, group->name,
                 group->sb_version);

    /* Check that the devices match the assembly
     * (it can be done only after loading the assembly)
     *
     * FIXME: How could this be done befor the layout start ???
     */
    storage_rdev_iterator_begin(&iter, group->storage);
    while ((rdev = storage_rdev_iterator_get(&iter)) != NULL)
    {
        uint64_t required_size;

        /* This test is also done at check up (see vrt_rdev_up) */
        required_size = rdev_chunk_based_size(rdev);
	if (vrt_realdev_get_usable_size(rdev) < required_size)
	{
	    exalog_error("Real size of device "UUID_FMT" is too small %"PRIu64" < %"PRIu64")",
                         UUID_VAL(&rdev->uuid), rdev->real_size, required_size);
	    rdev->corrupted = TRUE;
	}
    }
    storage_rdev_iterator_end(&iter);

    /* FIXME : add a check on the capacity for the devices (do they fit the
     * assembly) see 'rdev_up'
     */
    exalog_debug("Starting group %s with layout %s", group->name,
                 group->layout->name);
    ret = group->layout->group_start(group, group->storage);
    if (ret != EXA_SUCCESS)
	goto error_proc;

    ret = vrt_group_rebuild_thread_start(group);
    if (ret != EXA_SUCCESS)
	goto error_start;

    /* Start metadata thread*/
    ret = vrt_group_metadata_thread_start(group);
    if(ret != EXA_SUCCESS)
	goto error_metadata;

    *started_group = group;

    return EXA_SUCCESS;

error_metadata:
    vrt_group_rebuild_thread_cleanup(group);

error_start:
    exalog_debug("stop group '%s'", group->name);
    group->layout->group_stop(group->layout_data);

error_proc:

    /* FIXME: remove this cleanup, the volume structures are allocated before
     * command start */
    /* FIXME No need for explicitely checking and setting to NULL: os_free()
       does the job */
    for (i = 0; i < NBMAX_VOLUMES_PER_GROUP; i++)
	if (group->volumes[i])
	{
	    os_free(group->volumes[i]);
	    group->volumes[i] = NULL;
	}

    vrt_group_free(group);
    *started_group = NULL;

    return ret;
}

int vrt_group_insert_rdev(const vrt_group_info_t *group_description,
                          const exa_uuid_t *uuid, const exa_uuid_t *nbd_uuid,
                          exa_nodeid_t node_id, spof_id_t spof_id, bool local,
                          uint64_t old_sb_version, uint64_t new_sb_version)
{
    vrt_group_t *group;
    int err;
    storage_t *storage;
    vrt_realdev_t *new_rdev = NULL;
    int new_rdev_index;

    EXA_ASSERT(!group_description->layout_info.is_set);

    group = NULL;

    err = storage_build_from_description(&storage,
                                         group_description->nb_rdevs,
                                         group_description->rdevs);
    if (err != 0)
        return err;

    err = vrt_group_read_sb(&group, storage, &group_description->uuid,
                            group_description->sb_version);
    if (err != 0)
    {
        storage_free(storage);
        return err;
    }

    new_rdev_index = storage_get_num_realdevs(storage);
    /* add the newly inserted rdev to the storage */
    new_rdev = vrt_rdev_new(node_id, spof_id, uuid, nbd_uuid, new_rdev_index,
                            local, true);
    if (new_rdev == NULL)
    {
        exalog_error("Can't allocate rdev "UUID_FMT, UUID_VAL(uuid));
        err = -ENOMEM;
        goto cleanup;
    }

    err = vrt_rdev_open(new_rdev);
    if (err != 0)
    {
        exalog_error("Can't open rdev "UUID_FMT": %s (%d)", UUID_VAL(uuid),
                     exa_error_msg(err), err);
        vrt_rdev_free(new_rdev);
        goto cleanup;
    }

    err = storage_add_rdev(storage, new_rdev->spof_id, new_rdev);
    if (err != 0)
    {
        exalog_error("Can't add rdev "UUID_FMT" to storage: %s (%d)",
                     UUID_VAL(uuid), exa_error_msg(err), err);
        vrt_rdev_free(new_rdev);
        goto cleanup;
    }

    if (new_rdev->local)
    {
        err = vrt_rdev_create_superblocks(new_rdev);
        if (err != 0)
        {
            exalog_error("Cannot write initial superblock on rdev "UUID_FMT": %s (%d)",
                         UUID_VAL(uuid), exa_error_msg(err), err);
            vrt_group_del_rdev(group, new_rdev);
            goto cleanup;
        }
    }

    err = storage_cut_rdev_in_chunks(storage, new_rdev);
    if (err != 0)
    {
        exalog_error("Can't get chunks from rdev "UUID_FMT": %s (%d)",
                     UUID_VAL(uuid), exa_error_msg(err), err);
        vrt_group_del_rdev(group, new_rdev);
        goto cleanup;
    }

    if (group->layout->group_insert_rdev != NULL)
    {
        err = group->layout->group_insert_rdev(group->layout_data, new_rdev);
        if (err != 0)
        {
            exalog_error("Layout couldn't insert rdev "UUID_FMT": %s (%d)",
                         UUID_VAL(uuid), exa_error_msg(err), err);
            vrt_group_del_rdev(group, new_rdev);
            goto cleanup;
        }
    }

    if (err == 0)
        err = vrt_group_sync_sb(group, old_sb_version, new_sb_version);

    if (err != 0)
    {
        exalog_error("Can't synchronise superblock: %s (%d)",
                     exa_error_msg(err), err);
        vrt_group_del_rdev(group, new_rdev);
    }

cleanup:
    vrt_group_free(group);
    return err;
}

/**
 * Stop the group with the given name and the given uuid.
 *
 * @param[in] group Group to test
 *
 * @return 0 on success, a negative error code on failure.
 */
int vrt_group_stop(struct vrt_group *group)
{
    int i, err;

    vrt_group_metadata_thread_cleanup(group);
    vrt_group_rebuild_thread_cleanup(group);

    for (i = 0; i < NBMAX_VOLUMES_PER_GROUP; i++)
	EXA_ASSERT(group->volumes[i] == NULL
                   || group->volumes[i]->status == EXA_VOLUME_STOPPED);

    exalog_debug("stop group '%s'", group->name);
    err = group->layout->group_stop(group->layout_data);
    if (err == 0)
        vrt_group_free(group);

    return err;
}


/**
 * Test whether a group is stoppable (i.e if it has no started volume).
 *
 * @param[in] group Group to test
 *
 * @param[in] group_uuid uuid of the group.
 *
 * @return 0 on success, a negative error code on failure.
 */
int
vrt_group_stoppable (struct vrt_group *group, const exa_uuid_t *group_uuid)
{
    int i;

    /*  This Test allows to stop a group is the uuid passed is 0:0:0:0
     *  This is in prevision of a --force command that would
     */
    if (!uuid_is_equal(&group->uuid, group_uuid) &&
	!uuid_is_zero(group_uuid)  )
    {
	return -EXA_ERR_UUID;
    }

    for (i = 0; i < NBMAX_VOLUMES_PER_GROUP; i++)
	if (group->volumes[i])
	    if (group->volumes[i]->status != EXA_VOLUME_STOPPED)
		return -VRT_ERR_VOL_NOT_STOPPED_IN_GROUP;

    return EXA_SUCCESS;
}


/**
 * Test whether a group will go offline if we stop the specified
 * nodes
 *
 * @param[in] group           Group to test
 *
 * @param[in] stop_nodes      The nodes that may stop
 *
 * @return -VRT_ERR_GROUP_OFFLINE if the group will go offline, or
 *         EXA_SUCCESS if the group will no go offline, or
 *         a negative error code on failure
 */
int
vrt_group_going_offline (struct vrt_group *group, const exa_nodeset_t *stop_nodes)
{
    if (group->layout->group_going_offline)
        return group->layout->group_going_offline(group, stop_nodes);
    else
        return -EOPNOTSUPP;
}


/**
 * Turns the group in suspend status: all new requests on this group
 * will be put inside a wait queue, until they are woken up by
 * vrt_group_resume.
 *
 * @param[in] group      The group to suspend
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int
vrt_group_suspend (struct vrt_group *group)
{
    os_thread_rwlock_wrlock(&group->suspend_lock);
    group->suspended = TRUE;
    os_thread_rwlock_unlock(&group->suspend_lock);

    return EXA_SUCCESS;
}

/**
 * Must be executed on all nodes.
 *
 * @param[in] group The group on which the recover applies
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int
vrt_group_compute_status (struct vrt_group *group)
{
    int ret;

    exalog_debug("compute status of group '%s': old status = '%d'",
                 group->name, group->status);

    EXA_ASSERT (group->suspended);

    if (group->layout->group_compute_status)
	ret = group->layout->group_compute_status (group);
    else
	ret = EXA_SUCCESS;

    exalog_debug("compute status of group '%s': new status = '%d' (ret = %d)",
                 group->name, group->status, ret);

    if (ret != EXA_SUCCESS)
	return ret;

    if (group->status == EXA_GROUP_OFFLINE)
    {
	exalog_info("Group %s is  OFFLINE", group->name);
	return -VRT_WARN_GROUP_OFFLINE;
    }

    return EXA_SUCCESS;
}


/**
 * Resynchronize the devices of a group after the crash of a node. It
 * must be called only on the master node.
 *
 * @param[in] group   The group to start the resync on
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int vrt_group_resync(struct vrt_group *group, const exa_nodeset_t *nodes)
{
    int ret = EXA_SUCCESS;

    EXA_ASSERT (group->suspended);

    if (group->layout->group_resync != NULL)
	ret = group->layout->group_resync(group, nodes);

    exalog_debug("finished resync of group '%s' %d", group->name, ret);

    return ret;
}


/**
 * Must be executed on all nodes.
 *
 * @param[in] group The group on which the recover applies
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int vrt_group_post_resync(struct vrt_group *group)
{
    int err;

    EXA_ASSERT (group->suspended);

    if (group->layout->group_post_resync == NULL)
        return EXA_SUCCESS;

    if (group->status != EXA_GROUP_OFFLINE)
	err = group->layout->group_post_resync(group->layout_data);
    else
        err = -VRT_ERR_GROUP_OFFLINE;

    exalog_debug("finished post-resync of group '%s' %d", group->name, err);

    return err;
}


/**
 * Resume a group.
 */
int
vrt_group_resume(struct vrt_group *group)
{
    exalog_debug("resuming group '%s' (offline = %d)",
                 group->name, group->status == EXA_GROUP_OFFLINE);

    /* Silently return with a success when group is already
     * resumed. This is a workaround for a bug in the admin
     * part.
     *
     * FIXME: Does this "bug" still exist and is the workaround (cf. r10321)
     * still necessary ?
     */
    if (!group->suspended)
    {
        exalog_warning("group " UUID_FMT " is resumed while not suspended",
                       UUID_VAL(&group->uuid));
	return EXA_SUCCESS;
    }

    os_thread_rwlock_wrlock(&group->suspend_lock);
    group->suspended = FALSE;
    os_thread_rwlock_unlock(&group->suspend_lock);

    vrt_thread_wakeup();
    wake_up_all(& group->suspended_req_wq);

    /* Wake up the rebuilding thread
     * (it's up to it to decide if there is something to do) */
    if (group->status != EXA_GROUP_OFFLINE)
        vrt_group_rebuild_thread_resume(group);

    vrt_group_metadata_thread_resume(group);

    return EXA_SUCCESS;
}


int vrt_group_reset(struct vrt_group *group)
{
    int ret;

    if (group->layout->group_reset)
        ret = group->layout->group_reset(group->layout_data);
    else
    {
        exalog_error("Layout '%s' does not support group reset",
                     group->layout->name);
        ret = -EINVAL;
    }

    return ret;
}


int vrt_group_check(struct vrt_group *group)
{
    int ret;

    if (group->layout->group_check)
        ret = group->layout->group_check(group->layout_data);
    else
    {
        exalog_error("Layout '%s' does not support group check",
                     group->layout->name);
        ret = -EINVAL;
    }

    return ret;
}

bool vrt_group_supports_device_replacement(const struct vrt_group *group)
{
    EXA_ASSERT(group != NULL);
    EXA_ASSERT(group->layout != NULL);
    /* The group supports the replacement of its devices if it is able
       to reintegrate a device */
    return group->layout->rdev_reset != NULL
        && group->layout->rdev_reintegrate != NULL;
}

int vrt_group_reintegrate_rdev(vrt_group_t *group, struct vrt_realdev *rdev)
{
    exalog_debug("reintegrate rdev " UUID_FMT " in group '%s'",
                 UUID_VAL(&rdev->uuid), group->name);

    if (group->layout->rdev_reintegrate == NULL)
        return EXA_SUCCESS;

    return group->layout->rdev_reintegrate(group, rdev);
}

int vrt_group_post_reintegrate_rdev(vrt_group_t *group,
                                    struct vrt_realdev *rdev)
{
    exalog_debug("post-reintegrate rdev " UUID_FMT " in group '%s'",
                 UUID_VAL(&rdev->uuid), group->name);

    if (group->layout->rdev_post_reintegrate == NULL)
        return EXA_SUCCESS;

    return group->layout->rdev_post_reintegrate(group, rdev);
}

int vrt_group_header_read(group_header_t *header, stream_t *stream)
{
    int r;

    r = stream_read(stream, header, sizeof(group_header_t));
    if (r < 0)
        return r;
    else if (r != sizeof(group_header_t))
        return -EIO;

    return 0;
}

uint64_t vrt_group_serialized_size(const vrt_group_t *group)
{
    uint64_t total_vol_size;
    int i;

    total_vol_size = 0;
    for (i = 0; i < NBMAX_VOLUMES_PER_GROUP; i++)
        if (group->volumes[i] != NULL)
            total_vol_size += vrt_volume_serialized_size(group->volumes[i]);

    return sizeof(group_header_t)
        + storage_serialized_size(group->storage)
        + group->layout->serialized_size(group->layout_data)
        + total_vol_size;
}

#define serialization_debug(entity, uuid, sub_entity, sub_uuid, err)            \
    do {                                                                        \
        exalog_debug("Serialization of " entity " uuid " UUID_FMT " failed "    \
                     "on " sub_entity " " UUID_FMT ": %s (%d)", UUID_VAL(uuid), \
                     UUID_VAL(sub_uuid), exa_error_msg(err), err);              \
    } while (false)

int vrt_group_serialize(const vrt_group_t *group, stream_t *stream)
{
    group_header_t header;
    int w;
    int err;
    int i;

    header.magic = GROUP_HEADER_MAGIC;
    header.format = GROUP_HEADER_FORMAT;
    header.reserved = 0;
    uuid_copy(&header.uuid, &group->uuid);
    os_strlcpy(header.name, group->name, sizeof(header.name));
    header.nb_volumes = group->nb_volumes;
    os_strlcpy(header.layout_name, group->layout->name, sizeof(header.layout_name));
    header.data_size = vrt_group_serialized_size(group) - sizeof(group_header_t);

    w = stream_write(stream, &header, sizeof(header));
    if (w < 0)
        return w;
    else if (w != sizeof(header))
        return -EIO;

    err = storage_serialize(group->storage, stream);
    if (err != 0)
    {
        serialization_debug("group", &group->uuid, "storage", &exa_uuid_zero, err);
        return err;
    }

    err = group->layout->serialize(group->layout_data, stream);
    if (err != 0)
    {
        serialization_debug("group", &group->uuid, "layout", &exa_uuid_zero, err);
        return err;
    }

    for (i = 0; i < NBMAX_VOLUMES_PER_GROUP; i++)
    {
        if (group->volumes[i] == NULL)
            continue;

        err = vrt_volume_serialize(group->volumes[i], stream);
        if (err != 0)
        {
            serialization_debug("group", &group->uuid, "volume",
                                &group->volumes[i]->uuid, err);
            return err;
        }
    }

    return 0;
}

#define deserialization_debug(entity, uuid, sub_entity, err)                \
    do {                                                                    \
    exalog_debug("Deserialization of " entity " uuid " UUID_FMT " failed "  \
                 "on " sub_entity ": %s (%d)", UUID_VAL(uuid),              \
                 exa_error_msg(err), err);                                  \
    } while (false)

int vrt_group_deserialize(vrt_group_t **group, storage_t *storage,
                             stream_t *stream, const exa_uuid_t *group_uuid)
{
    group_header_t header;
    int err;
    int i;
    const struct vrt_layout *layout;
    const assembly_group_t *ag;

    EXA_ASSERT(group != NULL);

    *group = NULL;

    err = vrt_group_header_read(&header, stream);
    if (err != 0)
        return err;

    if (header.magic != GROUP_HEADER_MAGIC)
        return -VRT_ERR_SB_MAGIC;

    if (header.format != GROUP_HEADER_FORMAT)
        return -VRT_ERR_SB_FORMAT;

    if (header.reserved != 0)
        return -VRT_ERR_SB_CORRUPTION;

    if (!uuid_is_equal(group_uuid, &header.uuid))
        return -VRT_ERR_SB_UUID_MISMATCH;

    layout = vrt_get_layout(header.layout_name);
    if (layout == NULL)
        return -VRT_ERR_UNKNOWN_LAYOUT;

    *group = vrt_group_alloc(header.name, &header.uuid, layout);
    if (*group == NULL)
    {
        exalog_debug("Couldn't allocate group");
        return -ENOMEM;
    }

    (*group)->storage = NULL;

    err = storage_deserialize(storage, stream);
    if (err != 0)
    {
        deserialization_debug("group", &(*group)->uuid, "storage", err);
        vrt_group_free(*group);
        return err;
    }

    err = (*group)->layout->deserialize(&(*group)->layout_data, storage, stream);
    if (err != 0)
    {
        deserialization_debug("group", &(*group)->uuid, "layout", err);
        vrt_group_free(*group);
        return err;
    }

    ag = (*group)->layout->get_assembly_group((*group)->layout_data);
    EXA_ASSERT(ag != NULL);

    /* *group->nb_volumes *must* be set to zero since it is
       incremented as volumes are inserted */
    (*group)->nb_volumes = 0;

    for (i = 0; i < header.nb_volumes; i++)
    {
        vrt_volume_t *volume;

        err = vrt_volume_deserialize(&volume, ag, stream);
        if (err != 0)
        {
            deserialization_debug("group", &(*group)->uuid, "volume", err);
            vrt_group_free(*group);
            return err;
        }
        __insert_volume(*group, volume);
    }

    if ((*group)->nb_volumes != header.nb_volumes)
    {
        exalog_debug("Wrong number of volumes %"PRIu32", expected %"PRIu32,
                     (*group)->nb_volumes, header.nb_volumes);
        vrt_group_free(*group);
        return -VRT_ERR_SB_CORRUPTION;
    }

    (*group)->storage = storage;
    return 0;
}

bool vrt_group_equals(const vrt_group_t *group1, const vrt_group_t *group2)
{
    int i;

    if (!uuid_is_equal(&group1->uuid, &group2->uuid))
        return false;
    if (strcmp(group1->name, group2->name) != 0)
        return false;
    if (group1->nb_volumes != group2->nb_volumes)
        return false;
    if (group1->layout != group2->layout)
        return false;
    if (!storage_equals(group1->storage, group2->storage))
        return false;
    if (!group1->layout->layout_data_equals(group1->layout_data,
                                            group2->layout_data))
        return false;

    for (i = 0; i < NBMAX_VOLUMES_PER_GROUP; i++)
    {
        const vrt_volume_t *vol1;
        const vrt_volume_t *vol2;

        vol1 = group1->volumes[i];
        if (vol1 == NULL)
            continue;
        vol2 = vrt_group_find_volume(group2, &vol1->uuid);
        if (vol2 == NULL)
            return false;

        if (!vrt_volume_equals(vol1, vol2))
            return false;
    }
    return true;
}

/**
 * Ask the exclusion of a real device. This function is used in the "device
 * down" procedure. It places the given real device in the 'down' state and
 * asks the layout if it can still process requests without this device. If
 * not, group is put in the ERROR state, and an error is returned.
 *
 * @param[in] rdev    The real device to put in the DOWN state
 *
 * @return    Exanodes error code
 */
int
vrt_group_rdev_down(vrt_group_t *group, vrt_realdev_t *rdev)
{
    EXA_ASSERT (group->suspended);
    EXA_ASSERT (group->layout != NULL);

    exalog_debug("rdev " UUID_FMT " from group '%s' is DOWN",
                 UUID_VAL(&rdev->uuid), group->name);

    vrt_rdev_down(rdev);

    if (group->layout->group_rdev_down == NULL)
        return EXA_SUCCESS;

    return group->layout->group_rdev_down(group);
}

/**
 * Tells that a unusable device is now available again. We'll put it
 * in the EXA_REALDEV_UPDATING state, which doesn't mean we can safely
 * use it, but that a rebuilding process must take place before
 * changing the status to EXA_REALDEV_OK.
 *
 * @param[in] rdev       The real device which is now available again
 *
 * @return    always EXA_SUCCESS
 */
int
vrt_group_rdev_up(vrt_group_t *group, vrt_realdev_t *rdev)
{
    int err;

    /* Admind can send an up message even if the device is not down */
    if (rdev_is_ok(rdev))
	return EXA_SUCCESS;

    exalog_debug("rdev " UUID_FMT " from group '%s' is UP: status = %d",
                 UUID_VAL(&rdev->uuid), group->name, rdev_get_compound_status(rdev));
    EXA_ASSERT (group->suspended);

    err = vrt_rdev_up(rdev);
    if (err != EXA_SUCCESS)
        return err;

    /* signal to the layout that a new device is ready to be treated */
    if (group->layout->group_rdev_up == NULL)
        return EXA_SUCCESS;

    return group->layout->group_rdev_up(group);
}

int vrt_group_rdev_replace(vrt_group_t *group, vrt_realdev_t *rdev, const exa_uuid_t *new_rdev_uuid)
{
    int err;

    if (group->status == EXA_GROUP_OFFLINE)
    {
        /* FIXME Talking about rebuild here is bad */
	exalog_error("Rebuild is not possible on an offline group");
	return -VRT_ERR_CANT_REBUILD;
    }

    if (rdev_is_ok(rdev))
    {
	exalog_error("Bad rdev status %d", rdev_get_compound_status(rdev));
	return -VRT_ERR_CANT_DGDISKRECOVER;
    }

    exalog_debug("replacing disk for rdev " UUID_FMT " in group '%s': "
                 UUID_FMT " -> " UUID_FMT,
                 UUID_VAL(&rdev->uuid), group->name,
                 UUID_VAL(&rdev->nbd_uuid), UUID_VAL(new_rdev_uuid));

    err = vrt_rdev_replace(rdev, new_rdev_uuid);
    if (err != EXA_SUCCESS)
        return err;

    if (rdev->local)
    {
        /* Put the rdev up temporarily to allow writing the superblocks.
         * Admind ensures that the corresponding NBD device is accessible.
         *
         * FIXME: It is a workaround that lets us clear the superblocks while the
         *        rdev is not yet up. It is almost the same trick that is used
         *        at group create when we create pre-up devices (see
         *        vrt_rdev_new "up" argument).
         */
        err = vrt_rdev_up(rdev);
        if (err != EXA_SUCCESS)
            return err;

        err = vrt_rdev_create_superblocks(rdev);

        vrt_rdev_down(rdev);

        if (err != 0)
        {
            exalog_error("Cannot initialize superblock on new rdev "UUID_FMT": %s (%d)",
                         UUID_VAL(&rdev->uuid), exa_error_msg(err), err);
            return err;
        }
    }

    EXA_ASSERT(group->layout->rdev_reset != NULL);
    group->layout->rdev_reset(group->layout_data, rdev);

    return EXA_SUCCESS;
}

uint64_t vrt_group_total_capacity(const vrt_group_t *group)
{
    return group->layout->get_group_total_capacity(group->layout_data,
                                                   group->storage);
}

uint64_t vrt_group_used_capacity(const vrt_group_t *group)
{
    return group->layout->get_group_used_capacity(group->layout_data);
}
