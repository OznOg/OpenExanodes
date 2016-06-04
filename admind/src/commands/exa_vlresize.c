/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "admind/include/service_vrt.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admindstate.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"
#include "common/include/exa_names.h"
#include "os/include/os_file.h"
#include "common/include/exa_nodeset.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "vrt/virtualiseur/include/vrt_client.h"
#include "lum/client/include/lum_client.h"

__export(EXA_ADM_VLRESIZE) struct vlresize_params
{
    char group_name[EXA_MAXSIZE_GROUPNAME + 1];
    char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
    __optional bool no_fs_check __default(false);
    uint64_t size;
};

/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct vlresize_info
{
    char group_name[EXA_MAXSIZE_GROUPNAME + 1];
    char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
    uint64_t size;
    uint32_t force;
    uint32_t pad;
};


/** \brief Implements the vlresize command
 */
static void
cluster_vlresize(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
    const struct vlresize_params *params = data;
    struct adm_group *group;
    struct adm_volume *volume;
    int error_val;

    exalog_info("received vlresize '%s:%s' --size=%" PRIu64 " KB%s from %s",
                params->group_name, params->volume_name, params->size,
                params->no_fs_check ? " --nofscheck" : "",
                adm_cli_ip());

    /* Check the license status to send warnings/errors */
    cmd_check_license_status();

    group = adm_group_get_group_by_name(params->group_name);
    if (group == NULL)
    {
        set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME,
                  "Group '%s' not found", params->group_name);
        return;
    }

    volume = adm_group_get_volume_by_name(group, params->volume_name);
    if (volume == NULL)
    {
        set_error(err_desc, -ADMIND_ERR_UNKNOWN_VOLUMENAME, "Volume '%s' not found",
                  params->volume_name);
        return;
    }


    /* Check if the volume is not part of a file system */
    if (!params->no_fs_check && adm_volume_is_in_fs(volume))
    {
        set_error(err_desc, -ADMIND_ERR_VOLUME_IN_FS,
                  "volume '%s:%s' is managed by the file system layer, "
                  "please use exa_fsresize instead.",
                  volume->group->name, volume->name);
        return;
    }

    /* ask to start volume */
    error_val = vrt_master_volume_resize(ctx, volume, params->size);
    set_error(err_desc, error_val, NULL);
}

int vrt_master_volume_resize(admwrk_ctx_t *ctx, struct adm_volume *volume,
                             uint64_t sizeKB)
{
    int error_val;
    struct vrt_group_info group_info;
    struct vlresize_info info;
    uint64_t free_size, allowed_size;

    EXA_ASSERT(volume != NULL);

    /* Do not continue if the group is not started */
    if (volume->group->goal == ADM_GROUP_GOAL_STOPPED)
        return -VRT_ERR_GROUP_NOT_STARTED;

    memset(&info, 0, sizeof(info));

    /* Create the message for the local command */
    strlcpy(info.group_name, volume->group->name, EXA_MAXSIZE_GROUPNAME + 1);
    strlcpy(info.volume_name, volume->name, EXA_MAXSIZE_VOLUMENAME + 1);

    /* Get group info from the executive */
    error_val = vrt_client_group_info(adm_wt_get_localmb(), &volume->group->uuid,
                                      &group_info);
    if (error_val != EXA_SUCCESS)
    {
	exalog_error("Cannot get information on group "UUID_FMT": %s (%d)",
                     UUID_VAL(&volume->group->uuid),
                     exa_error_msg(error_val), error_val);
        return error_val;
    }

    if (group_info.status == EXA_GROUP_OFFLINE)
        return -VRT_ERR_GROUP_OFFLINE;

    EXA_ASSERT(group_info.usable_capacity >= group_info.used_capacity);
    EXA_ASSERT(adm_license_get_max_size(exanodes_license)
        >= adm_group_get_total_size());

    free_size = group_info.usable_capacity - group_info.used_capacity;
    allowed_size = adm_license_get_max_size(exanodes_license)
        - adm_group_get_total_size();

    if (sizeKB == 0) /* ie. size = max */
        info.size = volume->size + MIN(free_size, allowed_size);
    else
        info.size = sizeKB;

    /* Check the free size on the group */
    if (info.size > volume->size + free_size)
    {
	exalog_error("Not enough free space on group "UUID_FMT
                     " (%"PRIu64" > %"PRIu64")",
                     UUID_VAL(&volume->group->uuid),
                     info.size, volume->size + free_size);
        return -VRT_ERR_NOT_ENOUGH_FREE_SC;
    }
    else if (info.size > volume->size + allowed_size)
    {
        /* TODO report GB instead of KB */
        exalog_error("Resizing volume '%s:%s' to %"PRIu64" KiB would exceed the"
                     " license's size limitation (maximum size: %"PRIu64" KiB)",
                     info.group_name, info.volume_name,
                     info.size, volume->size + allowed_size);
        return -ADMIND_ERR_LICENSE;
    }

    error_val = admwrk_exec_command(ctx, &adm_service_admin,
                                    RPC_ADM_VLRESIZE, &info, sizeof(info));
    if(error_val != EXA_SUCCESS)
	exalog_error("Failed to resize volume '%s': %s (%d)",
                     info.volume_name, exa_error_msg(error_val), error_val);

    return error_val;
}

static void local_exa_vlresize (admwrk_ctx_t *ctx, void *msg)
{
    struct adm_group *group;
    struct adm_volume *volume = NULL;
    int ret, barrier_ret;
    struct vlresize_info *info = msg;
    uint64_t old_size;
    bool is_growing = true;

    /*** step 0 ***/
    /* Read the groupname from the message */
    group = adm_group_get_group_by_name(info->group_name);
    if (group == NULL)
    {
        ret = -ADMIND_ERR_UNKNOWN_GROUPNAME;
        goto check_barrier;
    }

    /* Read the volumename from the message */
    volume = adm_group_get_volume_by_name(group, info->volume_name);
    if (volume == NULL)
    {
        ret = -ADMIND_ERR_UNKNOWN_VOLUMENAME;
        goto check_barrier;
    }

    /* If the volume is marked as in-progress, then a previous
     * exa_vlcreate or exa_vldelete command was interrupted by a
     * node failure and we should not try to resize this volume.
     */
    if (!volume->committed)
        ret = -ADMIND_ERR_RESOURCE_IS_INVALID;
    else if (volume->size == info->size)
        ret = -ADMIND_ERR_NOTHINGTODO;
    else
        ret = EXA_SUCCESS;

    old_size = volume->size;
    is_growing = info->size > old_size;

check_barrier:
    barrier_ret = admwrk_barrier(ctx, ret, "Resizing - step 0 : "
                                 "Checking XML configuration");
    if (barrier_ret != EXA_SUCCESS)
        goto local_exa_vlresize_end_no_resume; /* Nothing to undo */

    ret = vrt_group_suspend_threads_barrier(ctx, &group->uuid);
    if (ret != EXA_SUCCESS)
        goto local_exa_vlresize_end;

    /*** Step 1 ***/
    if (is_growing)
    {
        /* Make the volume grows first
         *  -> allocate new slots before using the storage
         *
         * Rq: This the only expectable error (in case of not enough space)
         */
        ret = vrt_client_volume_resize(adm_wt_get_localmb(),
                                       &group->uuid, &volume->uuid,
                                       info->size);
    }
    else if (volume->exported)
    {
        /* Make the export shrinks first
         *  -> stop using the storage before removing it
         */
        ret = lum_client_export_resize(adm_wt_get_localmb(),
                                       &volume->uuid, info->size);

    }

    barrier_ret = admwrk_barrier(ctx, ret, "Resizing - step 1 : "
                                 "export shrinks or volume grows");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
        goto local_exa_vlresize_end;

    /*** Step 1.1 ***/
    if (is_growing)
    {
        /* Synchronize the group SBs */
        ret = adm_vrt_group_sync_sb(ctx, group);

        barrier_ret = admwrk_barrier(ctx, ret, "Resizing - step 1.1 : "
                                     "synchronize the group SBs");
        if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
            goto metadata_corruption;
        else if (barrier_ret != EXA_SUCCESS)
            goto local_exa_vlresize_end;
    }

    /*** Step 2 ***/
    /* Save the configuration file with the new volume size */
    volume->size = info->size;
    ret = conf_save_synchronous();

    barrier_ret = admwrk_barrier(ctx, ret, "Resizing - step 2 : "
                                 "save config file");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
        goto local_exa_vlresize_end;

    /*** Step 3 ***/
    if (is_growing && volume->exported)
    {
        /* Let the export grow */
        ret = lum_client_export_resize(adm_wt_get_localmb(),
                                       &volume->uuid, info->size);
    }
    else
    {
        /* Let the volume shrink */
        ret = vrt_client_volume_resize(adm_wt_get_localmb(),
                                       &group->uuid, &volume->uuid,
                                       info->size);
    }

    barrier_ret = admwrk_barrier(ctx, ret, "Resizing - step 3 : "
                                 "export grows or volume shrinks");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
        goto local_exa_vlresize_end;

    /*** Step 3.1 ***/
    if (!is_growing)
    {
        /* Synchronize the group SBs */
        ret = adm_vrt_group_sync_sb(ctx, group);

        barrier_ret = admwrk_barrier(ctx, ret, "Resizing - step 3.1 : "
                                     "synchronyse the group SBs");
        if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
            goto metadata_corruption;
        else if (barrier_ret != EXA_SUCCESS)
            goto local_exa_vlresize_end;
    }

    /* All done with success */
    ret = EXA_SUCCESS;
    goto local_exa_vlresize_end;

metadata_corruption:
    ret = -ADMIND_ERR_METADATA_CORRUPTION;

local_exa_vlresize_end:
    barrier_ret = vrt_group_resume_threads_barrier(ctx, &group->uuid);
    /* What to do if that fails... I don't know. */
    if (barrier_ret != 0)
        ret = barrier_ret;

local_exa_vlresize_end_no_resume:
    exalog_debug("Local volume resize command is complete");
    admwrk_ack(ctx, ret);
}

/**
 * Definition of the vlresize command.
 */
const AdmCommand exa_vlresize = {
    .code            = EXA_ADM_VLRESIZE,
    .msg             = "vlresize",
    .accepted_status = ADMIND_STARTED,
    .match_cl_uuid   = true,
    .cluster_command = cluster_vlresize,
    .local_commands  = {
        { RPC_ADM_VLRESIZE, local_exa_vlresize },
        { RPC_COMMAND_NULL, NULL }
    }
};


