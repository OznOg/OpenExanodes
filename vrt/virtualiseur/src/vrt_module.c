/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/** @file vrt_module.c
 *
 * @brief Initialization and removal functions of the virtualizer
 * module.
 */

#include <string.h>

#include "common/include/exa_names.h"
#include "common/include/exa_error.h"

#include "os/include/os_atomic.h"

#include "log/include/log.h"

#include "blockdevice/include/blockdevice.h"

#include "vrt/virtualiseur/include/vrt_cmd.h"
#include "vrt/virtualiseur/include/vrt_cmd_threads.h"
#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_nodes.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/constantes.h"

#include "vrt/virtualiseur/src/vrt_module.h"
#include "vrt/virtualiseur/include/vrt_init.h"
#include "vrt/virtualiseur/include/vrt_perf.h"

#include "vrt/layout/rain1/include/rain1.h"
#include "vrt/layout/sstriping/include/sstriping.h"

#include "os/include/os_error.h"

static LIST_HEAD(vrt_groups_list);              /**< List of all started groups */
static unsigned int vrt_groups_count = 0;       /**< Number of started groups */
static os_thread_mutex_t vrt_groups_list_lock;  /**< Group list lock */

static int node_id;
static int max_requests;
static exa_bool_t io_barriers = TRUE;

/** Caller must hold the vrt_groups_list_lock lock. */
#define for_each_group(group) \
    list_for_each_entry(group, &vrt_groups_list, list, struct vrt_group)

/**
 * Look for a group of the given uuid and return a pointer to its
 * vrt_group structure.
 *
 * The caller must call vrt_group_unref() when it's done with the group.
 *
 * @param[in] uuid UUID of the group to look for.
 *
 * @return The vrt_group structure corresponding to the group or
 * NULL if the group doesn't exist.
 */
static struct vrt_group *
vrt_get_group_from_uuid_unsafe(const exa_uuid_t *uuid)
{
    struct vrt_group *group;

    for_each_group (group)
	if (uuid_is_equal(&group->uuid, uuid))
	{
            if (!vrt_group_ref(group))
                return NULL;

	    return group;
	}

    return NULL;
}

/**
 * Look for a group of the given uuid and return a pointer to its
 * vrt_group structure.
 *
 * The caller must call vrt_put_group() when it's done with the group.
 *
 * @param[in] uuid UUID of the group to look for.
 *
 * @return The vrt_group structure corresponding to the group or
 * NULL if the group doesn't exist.
 */
struct vrt_group *
vrt_get_group_from_uuid(const exa_uuid_t *uuid)
{
    struct vrt_group *group;

    os_thread_mutex_lock(&vrt_groups_list_lock);
    group = vrt_get_group_from_uuid_unsafe (uuid);
    os_thread_mutex_unlock(&vrt_groups_list_lock);

    return group;
}


/**
 * Look for a group of the given name and return a pointer to its
 * vrt_group structure.
 *
 * The caller must call vrt_group_unref() when it's done with the group.
 *
 * @param[in] name Name of the group to look for.
 *
 * @return The vrt_group structure corresponding to the group or
 * NULL if the group doesn't exist.
 */
static struct vrt_group *
vrt_get_group_from_name_unsafe(const char *name)
{
    struct vrt_group *group;

    for_each_group(group)
	if (strncmp(group->name, name, EXA_MAXSIZE_GROUPNAME) == 0)
	{
            if (!vrt_group_ref(group))
                return NULL;

            return group;
	}

    return NULL;
}

/**
 * Look for a group of the given name and return a pointer to its
 * vrt_group structure.
 *
 * The caller must call vrt_put_group() when it's done with the group.
 *
 * @param[in] name Name of the group to look for.
 *
 * @return The vrt_group structure corresponding to the group or
 * NULL if the group doesn't exist.
 */
struct vrt_group *
vrt_get_group_from_name(const char *name)
{
    struct vrt_group *group;

    os_thread_mutex_lock (& vrt_groups_list_lock);
    group = vrt_get_group_from_name_unsafe (name);
    os_thread_mutex_unlock (& vrt_groups_list_lock);

    return group;
}

struct vrt_volume *vrt_get_volume_from_uuid(const exa_uuid_t *uuid)
{
    struct vrt_group *group;
    struct vrt_volume *volume = NULL;

    os_thread_mutex_lock(&vrt_groups_list_lock);

    for_each_group (group)
    {
        /* locking 'vrt_groups_list_lock' insures that group exists and is
         * valid (even if it is about to stop) */
        /* If group wants to stop, just do "as is" is was stopped */
        volume = vrt_group_find_volume(group, uuid);

        if (volume != NULL)
            break;
    }

    os_thread_mutex_unlock(&vrt_groups_list_lock);

    return volume;
}

blockdevice_t *vrt_open_volume(const exa_uuid_t *vol_uuid,
                               blockdevice_access_t access)
{
    struct vrt_volume *volume = vrt_get_volume_from_uuid(vol_uuid);

    if (volume == NULL || volume->status != EXA_VOLUME_STARTED)
        return NULL;

    return vrt_volume_create_block_device(volume, access);
}

int vrt_close_volume(blockdevice_t *blockdevice)
{
    EXA_ASSERT(blockdevice != NULL);

    return blockdevice_close(blockdevice);
}

/**
 * Allows to make a group publicly available by registering it in the
 * virtualizer's global group list and by initializing its /proc
 * attributes.
 *
 * The opposite function is vrt_groups_list_del().
 *
 * @param[in] group The group to make publicly available
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int
vrt_groups_list_add(struct vrt_group *group)
{
    struct vrt_group *g;

    os_thread_mutex_lock (& vrt_groups_list_lock);
    g = vrt_get_group_from_name_unsafe(group->name);
    if (g)
    {
	exalog_error("a group named '%s' is already started\n", group->name);
	vrt_group_unref(g);
	os_thread_mutex_unlock (& vrt_groups_list_lock);
	return -VRT_ERR_GROUPNAME_USED;
    }

    list_add(&group->list, &vrt_groups_list);
    vrt_groups_count++;

    os_thread_mutex_unlock (& vrt_groups_list_lock);

    return EXA_SUCCESS;
}

/**
 * Allows to make a group not publicly visible by removing its /proc
 * attributes and unregistering it from the virtualizer's global group
 * list.
 *
 * The opposite function is vrt_groups_list_add().
 *
 * @param[in] group The group to make publicly invisible
 *
 * @return Nothing.
 */
void
vrt_groups_list_del(struct vrt_group *group)
{
    os_thread_mutex_lock (& vrt_groups_list_lock);
    list_del(&group->list);
    vrt_groups_count--;
    os_thread_mutex_unlock (& vrt_groups_list_lock);
}

/**
 * Returns the maximum number of requests handled by the VRT
 * simultaneously. This function is used by the layouts that want to
 * make various assertions.
 */
unsigned int vrt_get_max_requests(void)
{
    return max_requests;
}

/**
 * Predicate - Is support for file systems barriers enabled?
 */
exa_bool_t vrt_barriers_enabled(void)
{
    return io_barriers;
}

static int vrt_module_init(int param_node_id,
                           int param_max_requests,
                           exa_bool_t param_io_barriers)
{
    int retval;

    node_id = param_node_id;
    max_requests = param_max_requests;
    io_barriers = param_io_barriers;

    os_thread_mutex_init(&vrt_groups_list_lock);

    vrt_cmd_handle_message_init();

    INIT_LIST_HEAD(&vrt_groups_list);

    if (node_id < 0 || max_requests < 0)
	return -EINVAL;

    vrt_nodes_init(node_id);

    exalog_as(EXAMSG_VRT_ID);

    retval = vrt_engine_init(max_requests);
    if (retval < 0)
	goto error_vrt_engine_init;

    retval = vrt_msg_subsystem_init();
    if (retval < 0)
	goto error_vrt_msg_subsystem_init;

    retval = vrt_cmd_threads_init();
    if (retval < 0)
	goto error_vrt_cmd_threads_init;

    VRT_PERF_INIT();

    return EXA_SUCCESS;

error_vrt_cmd_threads_init:
    vrt_msg_subsystem_cleanup();
error_vrt_msg_subsystem_init:
    vrt_engine_cleanup();
error_vrt_engine_init:
    vrt_cmd_handle_message_clean();
    os_thread_mutex_destroy(&vrt_groups_list_lock);

    return retval;
}

static void vrt_module_exit(void)
{
    EXA_ASSERT(vrt_groups_count == 0 && list_empty(&vrt_groups_list));

    vrt_cmd_threads_cleanup();

    /* vrt_msg_subsystem_cleanup must be called after all threads have been
     * cleaned up since they use the messaging handles owned by the
     * messaging subsystem. */
    vrt_msg_subsystem_cleanup();
    vrt_engine_cleanup();

    vrt_cmd_handle_message_clean();

    os_thread_mutex_destroy(&vrt_groups_list_lock);

    exalog_end();
}

void vrt_init(int adm_my_id, int max_requests, exa_bool_t io_barriers,
              int rebuilding_slowdown_ms,
              int degraded_rebuilding_slowdown_ms)
{
    sstriping_init();
    rain1_init(rebuilding_slowdown_ms, degraded_rebuilding_slowdown_ms);

    vrt_module_init(adm_my_id, max_requests, io_barriers);
}

void vrt_exit(void)
{
    vrt_module_exit();

    rain1_cleanup();
    sstriping_cleanup();
}






