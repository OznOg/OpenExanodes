/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "admind/include/service_vrt.h"
#include "admind/include/service_lum.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admindstate.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "target/iscsi/include/lun.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/exa_nodeset.h"
#include "common/include/exa_math.h"
#include "common/include/uuid.h"
#include "log/include/log.h"
#include "os/include/os_file.h"
#include "os/include/os_string.h"
#include "os/include/strlcpy.h"
#include "vrt/virtualiseur/include/vrt_client.h"

#ifdef USE_YAOURT
#include <yaourt/yaourt.h>
#endif

__export(EXA_ADM_VLCREATE) struct vlcreate_params
{
    char group_name[EXA_MAXSIZE_GROUPNAME + 1];
    char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
    char export_type[32];
    uint64_t size;
    __optional int32_t readahead __default(-1);
    __optional bool private __default(false);
    __optional int32_t lun __default(-1);
};

/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct vlcreate_info
{
    char group_name[EXA_MAXSIZE_GROUPNAME + 1];
    char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
    export_type_t export_type;
    exa_uuid_t volume_uuid;
    uint64_t size;        /* in KB */
    uint32_t is_private;
    uint32_t readahead;   /* in KB */
    uint32_t force;
    lun_t  lun;
};

static void __vrt_master_volume_create (int thr_nb, struct adm_group *group,
                                        const char *volume_name,
                                        export_type_t export_type,
                                        uint64_t sizeKB, int isprivate,
                                        int32_t readaheadKB,
                                        lun_t lun, cl_error_desc_t *err_desc);

/** \brief Implements the vlcreate command
 *
 * \param thr_nb: the thread number
 */
static void cluster_vlcreate(int thr_nb, void *data,
                             cl_error_desc_t *err_desc)
{
    const struct vlcreate_params *params = data;
    struct adm_group *group;
    lun_t lun;
    export_type_t export_type;

    exalog_info("received vlcreate '%s:%s' --export-method=%s --size=%" PRIu64
                "KB --access=%s"
                " --lun=%" PRId32 "%s"
                " --readahead=%" PRId32 "KB%s"
                " from %s",
                params->group_name, params->volume_name, params->export_type,
                params->size, params->private ? "private" : "shared",
                params->lun, params->lun == -1 ? " (auto)" : "",
                params->readahead, params->readahead == -1 ? " (default)" : "",
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

    lun = params->lun == -1 ? LUN_NONE : params->lun;

    if (!os_strcasecmp(params->export_type, "bdev"))
        export_type = EXPORT_BDEV;
    else if (!os_strcasecmp(params->export_type, "iscsi"))
        export_type = EXPORT_ISCSI;
    else
    {
        set_error(err_desc, -EXA_ERR_EXPORT_WRONG_METHOD,
                  "Invalid export type: '%s'", params->export_type);
        return;
    }

    __vrt_master_volume_create(thr_nb, group, params->volume_name, export_type,
                               params->size, params->private, params->readahead, lun,
                               err_desc);

    exalog_debug("vlcreate clustered command is complete %d", err_desc->code);
}

/**
 * @param size the size of the new volume in kilobytes
 */

static void __vrt_master_volume_create (int thr_nb, struct adm_group *group,
                                        const char *volume_name,
                                        export_type_t export_type,
                                        uint64_t sizeKB, int isprivate,
                                        int32_t readaheadKB,
                                        lun_t lun, cl_error_desc_t *err_desc)
{
    int ret;
    int reply_ret;
    admwrk_request_t handle;
    struct vlcreate_info info;
    struct vrt_group_info group_info;
    uint64_t free_size, allowed_size;

    EXA_ASSERT(group != NULL);

    /* Do not continue if the group is not started */
    if (group->goal == ADM_GROUP_GOAL_STOPPED)
    {
        set_error(err_desc, -VRT_ERR_GROUP_NOT_STARTED, NULL);
        return;
    }

    EXA_ASSERT(EXPORT_TYPE_IS_VALID(export_type));

    if (export_type == EXPORT_ISCSI)
    {
        /* If no lun was provided, pick up a new one */
        if (lun == LUN_NONE)
        {
            ret = lum_get_new_lun(&lun);
            if (ret != EXA_SUCCESS)
            {
                set_error(err_desc, ret, NULL);
                return;
            }
        }
        else
        { /* Check lun provided by user is not already used by another volume */

            /* Check lun is available when provided by user */
            if (!LUN_IS_VALID(lun))
            {
                set_error(err_desc, -LUN_ERR_INVALID_VALUE, NULL);
                return;
            }

            if (!lum_lun_is_available(lun))
            {
                set_error(err_desc, -LUN_ERR_ALREADY_ASSIGNED, NULL);
                return;
            }
        }

        /* When here, we are supposed to have a valid lun. */
        EXA_ASSERT(LUN_IS_VALID(lun));
    }
    else if (export_type == EXPORT_BDEV)
    {
        /* Catch the -1 value which means that the user wants the default value
         * to be used
         */
        if (readaheadKB == -1)
            readaheadKB = adm_cluster_get_param_int("default_readahead");

        if (readaheadKB < 0)
        {
            set_error(err_desc, -EINVAL,
                      "The readahead value must be a positive integer.");
            return;
        }
    }

    memset(&info, 0, sizeof(info));

    /* Create the message for the local command */
    strlcpy(info.group_name,  group->name,  sizeof(info.group_name));
    strlcpy(info.volume_name, volume_name, sizeof(info.volume_name));
    uuid_generate(&info.volume_uuid);
    info.export_type = export_type;
    info.size = sizeKB;
    info.is_private = isprivate;
    info.readahead = readaheadKB;
    info.lun = lun;

    /* Get group info from the executive */
    ret = vrt_client_group_info(adm_wt_get_localmb(), &group->uuid,
                                      &group_info);
    if (ret != EXA_SUCCESS)
    {
	exalog_error("Cannot get information on group "UUID_FMT": %s (%d)",
                     UUID_VAL(&group->uuid), exa_error_msg(ret), ret);
        set_error(err_desc, ret, NULL);
        return;
    }

    EXA_ASSERT(group_info.usable_capacity >= group_info.used_capacity);
    EXA_ASSERT(adm_license_get_max_size(exanodes_license)
        >= adm_group_get_total_size());

    free_size = group_info.usable_capacity - group_info.used_capacity;
    allowed_size = adm_license_get_max_size(exanodes_license) - adm_group_get_total_size();

    if (sizeKB == 0) /* ie. size = max */
        info.size = MIN(free_size, allowed_size);
    else
        info.size = sizeKB;

    /* Check the free size on the group */
    if (free_size == 0 || info.size > free_size)
    {
	exalog_error("Not enough free space on group "UUID_FMT
                     " (%"PRIu64" > %"PRIu64")",
                     UUID_VAL(&group->uuid), info.size, free_size);
        set_error(err_desc, -VRT_ERR_NOT_ENOUGH_FREE_SC, NULL);
        return;
    }
    else if (allowed_size == 0 || info.size > allowed_size)
    {
        char msg[256];

        os_snprintf(msg, sizeof(msg), "Exceeding the license's size limitation"
                    " (size remaining: %"PRIu64" KiB)", allowed_size);
        exalog_error("%s", msg);
        set_error(err_desc, -ADMIND_ERR_LICENSE, msg);
        return;
    }

    admwrk_run_command(thr_nb, &adm_service_admin, &handle, RPC_ADM_VLCREATE,
                       &info, sizeof(info));
    /* Examine replies in order to filter return values.
     * The priority of return values is the following (in descending order):
     * o ADMIND_ERR_METADATA_CORRUPTION
     * o ADMIND_ERR_NODE_DOWN
     * o other errors
     */
    ret = EXA_SUCCESS;
    while (admwrk_get_ack(&handle, NULL, &reply_ret))
    {
        if (reply_ret == -ADMIND_ERR_METADATA_CORRUPTION)
            ret = -ADMIND_ERR_METADATA_CORRUPTION;
        else if (reply_ret == -ADMIND_ERR_NODE_DOWN &&
                 ret != -ADMIND_ERR_METADATA_CORRUPTION)
            ret = -ADMIND_ERR_NODE_DOWN;
        else if (reply_ret != EXA_SUCCESS &&
                 reply_ret != -ADMIND_ERR_NOTHINGTODO &&
                 ret != -ADMIND_ERR_METADATA_CORRUPTION &&
                 ret != -ADMIND_ERR_NODE_DOWN)
            ret = reply_ret;
    }

    set_error(err_desc, ret, NULL);
}

/* This function is kept as a wrapper for compatibility purpose with FS.
 * But FS doesn not work on GA, anyway.... */
int vrt_master_volume_create (int thr_nb, struct adm_group *group,
                              const char *volume_name, export_type_t export_type,
                              uint64_t sizeKB, int isprivate, uint32_t readaheadKB)
{
    cl_error_desc_t err_desc;
    __vrt_master_volume_create(thr_nb, group, volume_name, export_type, sizeKB,
	                       isprivate, readaheadKB, LUN_NONE, &err_desc);
    return err_desc.code;
}

static void
local_exa_vlcreate (int thr_nb, void *msg)
{
    struct adm_group *group;
    struct adm_volume *volume;
    int ret;               /* used for local function calls */
    int barrier_ret;       /* used for barriers return values */
    int rollback_ret;      /* used for local function calls during rollback */
    bool force_undo; /* used to force steps during rollback */
    struct vlcreate_info *info = msg;
    char path[EXA_MAXSIZE_LINE + 1];


    EXA_ASSERT(EXPORT_TYPE_IS_VALID(info->export_type));

    ret = EXA_SUCCESS;
    force_undo = false;

    group = adm_group_get_group_by_name(info->group_name);
    if (group == NULL)
        ret = -ADMIND_ERR_UNKNOWN_GROUPNAME;

    /*** Barrier: "cmd_params_get", getting parameters ***/
    barrier_ret = admwrk_barrier(thr_nb, ret, "Getting parameters");
    if (barrier_ret != EXA_SUCCESS)
        goto local_exa_vlcreate_end_no_resume; /* Nothing to undo */

    ret = vrt_group_suspend_threads_barrier(thr_nb, &group->uuid);
    if (ret != EXA_SUCCESS)
        goto local_exa_vlcreate_end;

    /*** Action: create the volume (transaction set to "INPROGRESS") ***/
    volume = adm_volume_alloc();
    if (volume == NULL)
    {
        ret = -ENOMEM;
        goto update_barrier;
    }

    strlcpy(volume->name, info->volume_name, EXA_MAXSIZE_VOLUMENAME + 1);
    uuid_copy(&volume->uuid, &info->volume_uuid);
    volume->size = info->size;
    volume->shared = !info->is_private;
    adm_nodeset_set_all(&volume->goal_stopped);
    exa_nodeset_reset(&volume->goal_started);
    exa_nodeset_reset(&volume->goal_readonly);
    volume->committed = false;
    volume->readahead = info->readahead;

    ret = adm_group_insert_volume(group, volume);

#ifdef USE_YAOURT
    yaourt_event_wait(examsgOwner(adm_wt_get_inboxmb(thr_nb)),
                      "local_exa_vlcreate_tr_inprogress");
#endif

update_barrier:
    /*** Barrier: "xml_update", update XML configuration ***/
    barrier_ret = admwrk_barrier(thr_nb, ret, "Updating XML configuration");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
        goto undo_xml_update;

    /*** Action: save configuration file ***/
    ret = conf_save_synchronous();

    /*** Barrier: "xml_save", save configuration file ***/
    barrier_ret = admwrk_barrier(thr_nb, ret, "Saving configuration file");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
        goto undo_xml_save;

    /*** Action: create the volume (in memory) through the virtualiser API ***/
    ret = vrt_client_volume_create(adm_wt_get_localmb(), &group->uuid,
                                   volume->name, &volume->uuid,
                                   volume->size);

    /*** Barrier: "vrt_volume_create", volume creation ***/
    barrier_ret = admwrk_barrier(thr_nb, ret, "Creating logical volume");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
        goto undo_vrt_volume_create;

    /*** Action: group sync SB ***/
    ret = adm_vrt_group_sync_sb(thr_nb, group);

    /*** Barrier: "adm_vrt_group_sync_sb", Syncing metadata on disk ***/
    barrier_ret = admwrk_barrier(thr_nb, ret,
                                 "Syncing metadata on disk");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
        goto undo_vrt_sync;

#ifdef USE_YAOURT
    yaourt_event_wait(examsgOwner(adm_wt_get_inboxmb(thr_nb)),
                      "local_exa_vlcreate_tr_committed");
#endif

    /*** Action: mark the progress parameter as COMMITTED in the XML config file ***/
    /* This is an in-memory operation, and we assume it won't fail */
    volume->committed = true;

    /* Force a config file save */
    ret = conf_save_synchronous();
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "%s", exa_error_msg(ret));

    /* Creation of export structures */

    if (info->export_type == EXPORT_ISCSI)
        lum_create_export_iscsi(&info->volume_uuid, info->lun);
    else
    {
        adm_volume_path(path, sizeof(path), info->group_name, info->volume_name);
        lum_create_export_bdev(&info->volume_uuid, path);
    }

    /* Everything went OK */
    goto local_exa_vlcreate_end;

    /*** Rollback: undo labels reached when something bad occurred. ***/
    /* sync_sb_commit:
     * ---------------
     * We assume that the in memory increment performed by the sync SB
     * commit won't fail.
     *
     * sync_sb:
     * --------
     * sync sb is performed by the master, so if it failed, there is
     * nothing to undo => no rollback needed for this action and no barrier.
     */
undo_vrt_sync:
    /* Nothing to undo, just force next action */
    force_undo = true;

undo_vrt_volume_create:
    if ((ret == EXA_SUCCESS) || force_undo)
        rollback_ret = vrt_client_volume_delete(adm_wt_get_localmb(),
                                                &group->uuid, &volume->uuid);
    else
        rollback_ret = -ADMIND_ERR_NOTHINGTODO;
    barrier_ret = admwrk_barrier(thr_nb, rollback_ret, "undo_vrt_volume_create");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;
    force_undo = true;

undo_xml_save:
    /* Nothing to undo, just force next action */
    force_undo = true;

undo_xml_update:
    if ((ret == EXA_SUCCESS) || force_undo)
    {
        adm_group_remove_volume(volume);
        adm_volume_free(volume);
        rollback_ret = EXA_SUCCESS;
    }
    else if (ret == -ADMIND_ERR_VOLUME_ALREADY_CREATED || ret == -ENOSPC)
    {
        /* The insertion of the volume in the group failed,
         * simply free the volume.
         */
        adm_volume_free(volume);
        rollback_ret = EXA_SUCCESS;
    }
    else
        rollback_ret = -ADMIND_ERR_NOTHINGTODO;
    barrier_ret = admwrk_barrier(thr_nb, rollback_ret, "undo_xml_update");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;
    force_undo = true;

    /* Always save the configuration file after rollback */
    rollback_ret = conf_save_synchronous();
    barrier_ret = admwrk_barrier(thr_nb, rollback_ret, "conf_save_synchronous");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
        goto metadata_corruption;

    goto local_exa_vlcreate_end;

    /*** Metadata-recovery ***/
metadata_corruption:
#ifdef USE_YAOURT
    yaourt_event_wait(examsgOwner(adm_wt_get_inboxmb(thr_nb)), "local_exa_vlcreate_mrecovery");
#endif
    ret = -ADMIND_ERR_METADATA_CORRUPTION;

local_exa_vlcreate_end:
    barrier_ret = vrt_group_resume_threads_barrier(thr_nb, &group->uuid);
    /* What to do if that fails... I don't know. */
    if (barrier_ret != 0)
        ret = barrier_ret;

local_exa_vlcreate_end_no_resume:

    exalog_debug("local_exa_vlcreate() = %s", exa_error_msg(ret));
    admwrk_ack(thr_nb, ret);
}

/**
 * Definition of the vlcreate command.
 */
const AdmCommand exa_vlcreate = {
    .code            = EXA_ADM_VLCREATE,
    .msg             = "vlcreate",
    .accepted_status = ADMIND_STARTED,
    .match_cl_uuid   = true,
    .cluster_command = cluster_vlcreate,
    .local_commands  =
    {
        { RPC_ADM_VLCREATE, local_exa_vlcreate },
        { RPC_COMMAND_NULL, NULL },
    },
};


