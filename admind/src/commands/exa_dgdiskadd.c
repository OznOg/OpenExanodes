/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/include/service_vrt.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "admind/src/instance.h"

#include "vrt/virtualiseur/include/vrt_client.h"

#include "os/include/os_disk.h"

__export(EXA_ADM_DGDISKADD) struct dgdiskadd_params
{
    char node_name[EXA_MAXSIZE_NODENAME + 1];
    char disk_path[EXA_MAXSIZE_DEVPATH + 1];
    char group_name[EXA_MAXSIZE_GROUPNAME + 1];
};

/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct dgdiskadd_info
{
    exa_uuid_t group_uuid;
    exa_uuid_t disk_uuid;
    exa_uuid_t vrt_uuid;
};

/**
 * dgdiskadd cluster command.
 *
 * The dgdiskadd cluster commands basically reads the arguments from
 * the XML description of the command, and builds a struct
 * dgdiskadd_info message used to pass the arguments to the local
 * commands.
 */
static void cluster_dgdiskadd(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
    const struct dgdiskadd_params *params = data;
    char normalized_path[EXA_MAXSIZE_DEVPATH + 1];
    struct adm_node *node;
    struct adm_group *group;
    struct adm_disk *new_disk;
    int err;
    struct dgdiskadd_info info;
    exa_nodeset_t nodes_up;

    /* Check the license status to send warnings/errors */
    cmd_check_license_status();

    if (adm_cluster.goal != ADM_CLUSTER_GOAL_STARTED)
    {
        set_error(err_desc, -EXA_ERR_ADMIND_STOPPED, NULL);
        return;
    }

    exalog_info("received dgdiskadd '%s' --disk '%s:%s' from %s",
                params->group_name, params->node_name, params->disk_path,
                adm_cli_ip());


    node = adm_cluster_get_node_by_name(params->node_name);
    if (node == NULL)
    {
        set_error(err_desc, -ADMIND_ERR_UNKNOWN_NODENAME,
                  "Node '%s' is not part of the cluster.", params->node_name);
        return;
    }

    group = adm_group_get_group_by_name(params->group_name);
    if (group == NULL)
    {
        set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME,
                  "Group '%s' not found", params->group_name);
        return;
    }

    /* Do not continue if the group is not stopped */
    if (group->goal == ADM_GROUP_GOAL_STARTED)
    {
        set_error(err_desc, -VRT_ERR_GROUP_NOT_STOPPED,
                  "Group '%s' is not stopped", group->name);
        return;
    }

    err = os_disk_normalize_path(params->disk_path, normalized_path,
                                 sizeof(normalized_path));
    if (err != 0)
    {
        set_error(err_desc, err, "Cannot normalize path '%s'.",
                  params->disk_path);
        return;
    }

    new_disk = adm_cluster_get_disk_by_path(params->node_name, normalized_path);
    if (new_disk == NULL)
    {
        set_error(err_desc, -ADMIND_ERR_UNKNOWN_DISK, NULL);
        return;
    }

    if (!uuid_is_zero(&new_disk->group_uuid))
    {
        set_error(err_desc, -ADMIND_ERR_DISK_ALREADY_ASSIGNED,
                  "Disk %s:%s is already in group '%s'",
                  params->node_name, params->disk_path, group->name);
        return;
    }

    inst_get_nodes_up(&adm_service_rdev, &nodes_up);
    if (!exa_nodeset_contains(&nodes_up, new_disk->node_id))
    {
        set_error(err_desc, -VRT_ERR_RDEV_DOWN, "Cannot add disk: its node"
                  " is down");
        return;
    }

    uuid_copy(&info.group_uuid, &group->uuid);
    uuid_copy(&info.disk_uuid, &new_disk->uuid);
    uuid_generate(&info.vrt_uuid);

    err = admwrk_exec_command(thr_nb, &adm_service_rdev, RPC_ADM_DGDISKADD,
                              &info, sizeof(info));

    set_error(err_desc, err, NULL);
}

/**
 * The dgdiskadd local command, executed on all nodes.
 */
static void local_dgdiskadd(int thr_nb, void *msg)
{
    int barrier_ret;
    struct dgdiskadd_info *info = msg;
    struct adm_group *group;
    struct adm_disk *new_disk;
    struct adm_node *node;
    uint64_t old_sb_version, new_sb_version;
    int err;

    group = adm_group_get_group_by_uuid(&info->group_uuid);
    EXA_ASSERT(group);

    new_disk = adm_cluster_get_disk_by_uuid(&info->disk_uuid);
    EXA_ASSERT(new_disk);
    EXA_ASSERT(uuid_is_zero(&new_disk->group_uuid));
    uuid_copy(&new_disk->vrt_uuid, &info->vrt_uuid);

    node = adm_cluster_get_node_by_id(new_disk->node_id);
    EXA_ASSERT(node);

    if (new_disk->imported == 0)
    {
        barrier_ret = admwrk_barrier(thr_nb, -VRT_ERR_RDEV_DOWN, "Disk down");
        goto local_exa_dgdiskadd_end;
    }

    err = service_vrt_prepare_group(group);

    barrier_ret = admwrk_barrier(thr_nb, err, "Preparing the group");
    if (barrier_ret != EXA_SUCCESS)
    {
        exalog_error("Failed to prepare group " UUID_FMT ": %s (%d)",
	           UUID_VAL(&group->uuid), exa_error_msg(barrier_ret),
                   barrier_ret);
        goto local_exa_dgdiskadd_end;
    }

    old_sb_version = sb_version_get_version(group->sb_version);
    new_sb_version = sb_version_new_version_prepare(group->sb_version);

    err = vrt_client_group_insert_rdev(adm_wt_get_localmb(), &group->uuid,
				       new_disk->node_id, node->spof_id,
                                       &new_disk->vrt_uuid, &new_disk->uuid,
				       adm_disk_is_local(new_disk),
                                       old_sb_version, new_sb_version);

    barrier_ret = admwrk_barrier(thr_nb, err, "Sending new rdev information");
    if (barrier_ret != EXA_SUCCESS)
    {
        exalog_error("Failed to insert disk " UUID_FMT " in group: %s (%d)",
	           UUID_VAL(&new_disk->uuid), exa_error_msg(barrier_ret),
                   barrier_ret);
        goto local_exa_dgdiskadd_end;
    }

    sb_version_new_version_done(group->sb_version);

    barrier_ret = admwrk_barrier(thr_nb, EXA_SUCCESS, "Writing superblocks version");
    /* Commit anyway, If we are here, we are sure that other nodes have done the
    * job too even if they crashed meanwhile */
    sb_version_new_version_commit(group->sb_version);

    err = adm_group_insert_disk(group, new_disk);

    /* Assert is not really nice, but rollback may not be possible...
    * this would need some transactional behaviour */
    EXA_ASSERT(err == EXA_SUCCESS);

    err = conf_save_synchronous();
    EXA_ASSERT(err == EXA_SUCCESS);

local_exa_dgdiskadd_end:
    admwrk_ack(thr_nb, barrier_ret);
}


/**
 * Definition of the dgdiskadd command.
 */
const AdmCommand exa_dgdiskadd = {
    .code            = EXA_ADM_DGDISKADD,
    .msg             = "dgdiskadd",
    .accepted_status = ADMIND_STARTED,
    .match_cl_uuid   = true,
    .cluster_command = cluster_dgdiskadd,
    .local_commands  = {
        { RPC_ADM_DGDISKADD, local_dgdiskadd },
        { RPC_COMMAND_NULL,  NULL }
    }
};
