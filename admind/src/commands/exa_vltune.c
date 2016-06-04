/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "admind/include/service_lum.h"
#include "admind/include/service_vrt.h"
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
#include "admind/src/commands/exa_vlgettune.h"
#include "admind/src/commands/command_api.h"
#include "lum/client/include/lum_client.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/exa_nodeset.h"
#include "common/include/exa_conversion.h"
#include "log/include/log.h"
#include "os/include/os_file.h"
#include "os/include/os_stdio.h"
#include "vrt/virtualiseur/include/vrt_client.h"

#include "target/iscsi/include/lun.h"
#include "admind/include/service_lum.h"


__export(EXA_ADM_VLTUNE) struct vltune_params
  {
    char group_name[EXA_MAXSIZE_GROUPNAME + 1];
    char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
    __optional bool no_fs_check __default(false);
    char operation[EXA_MAXSIZE_PARAM_OPERATION+1];
    char param_name[EXA_MAXSIZE_PARAM_NAME+1];
    char param_value[EXA_MAXSIZE_PARAM_VALUE+1];
  };


typedef enum
{
    VLTUNE_OPERATION_SET, 	/* Set value of a parameter. */
    VLTUNE_OPERATION_GET, 	/* Get value of a parameter. */
    VLTUNE_OPERATION_ADD, 	/* Add a value to a list parameter. */
    VLTUNE_OPERATION_REMOVE,    /* Remove a value from a list parameter. */
    VLTUNE_OPERATION_RESET 	/* Reset value of a parameter. */
} vltune_operation_t;


/*******************************************************************************/
/* LUN tuning                                                                  */

typedef struct
{
    exa_uuid_t volume_uuid;
    lun_t lun;
} vltune_lun_cmd_t;

/**
 * @brief Globally tune the lun parameter
 */
static void
cluster_tune_lun(admwrk_ctx_t *ctx, const struct adm_volume *volume, vltune_operation_t operation,
                 const char *lun_str, cl_error_desc_t *err_desc)
{
    int ret = EXA_SUCCESS;
    lun_t lun;
    exa_nodeset_t all_nodes;
    vltune_lun_cmd_t cmd;
    const char *result;
    tunelist_t *tunelist;
    lun_t export_lun;

    if (lum_exports_get_type_by_uuid(&volume->uuid) != EXPORT_ISCSI)
    {
        set_error(err_desc, -EXA_ERR_EXPORT_WRONG_METHOD, NULL);
        return;
    }

    export_lun = lum_exports_iscsi_get_lun_by_uuid(&volume->uuid);

    if (export_lun == LUN_NONE)
    {
        set_error(err_desc, -EXA_ERR_EXPORT_NOT_FOUND, NULL);
        return;
    }

    if (operation == VLTUNE_OPERATION_GET)
        ; /* Nothing to do for now */
    else if (operation == VLTUNE_OPERATION_SET || operation == VLTUNE_OPERATION_RESET)
    {
        adm_nodeset_set_all(&all_nodes);

        /* Checks that the given volume is stopped on all nodes */
        if (! exa_nodeset_equals(&volume->goal_stopped, &all_nodes))
        {
            set_error(err_desc, -VRT_ERR_VOLUME_NOT_STOPPED, NULL);
            return;
        }

        /* Reset and set with no arguments are equivalent */
        if (operation == VLTUNE_OPERATION_RESET)
        {
            ret = lum_get_new_lun(&lun);
            if (ret != EXA_SUCCESS)
            {
                set_error(err_desc, ret, NULL);
                return;
            }
        }
        else
        {
            lun = lun_from_str(lun_str);
            if (!LUN_IS_VALID(lun))
            {
                set_error(err_desc, -LUN_ERR_INVALID_VALUE, NULL);
                return;
            }

            /* FIXME Use lun_t in adm_volume */
            if (export_lun == lun)
                goto send_reply;

            if (!lum_lun_is_available(lun))
            {
                set_error(err_desc, -LUN_ERR_ALREADY_ASSIGNED, NULL);
                return;
            }
        }

        /* Send the command to the nodes */
        memset(&cmd, 0, sizeof(cmd));
        cmd.lun = lun;
        uuid_copy(&cmd.volume_uuid, &volume->uuid);

        ret =  admwrk_exec_command(ctx, &adm_service_admin,
                                   RPC_ADM_VLTUNE_LUN, &cmd, sizeof(cmd));
        if (ret != EXA_SUCCESS)
            goto error;
    }
    else
    {
        set_error(err_desc, -EXA_ERR_INVALID_VALUE,
		  "Operation not implemented for this parameter");
        return;
    }

send_reply:
    /* Forge the reply */
    ret = tunelist_create(&tunelist);
    if (ret != EXA_SUCCESS)
        goto error;

    ret = tunelist_add_lun(tunelist, volume);

    if (ret != EXA_SUCCESS)
        goto error_free_tunelist;

    result = tunelist_get_result(tunelist);
    if (result == NULL)
        goto error_free_tunelist;

    /* Send the reply to the CLI */
    send_payload_str(result);

    tunelist_delete(tunelist);
    set_success(err_desc);
    return;

error_free_tunelist:
    tunelist_delete(tunelist);
error:
    set_error(err_desc, ret, NULL);
    return;
}


/**
 * @brief Locally tune the lun parameter.
 *
 * @param msg  Buffer containing the LUN tuning command
 *
 * @return EXA_SUCCESS upon success. Otherwise an error code
 */
void local_vltune_lun(admwrk_ctx_t *ctx, void *msg)
{
    vltune_lun_cmd_t *cmd = msg;
    struct adm_volume *volume;
    int err;

    EXA_ASSERT(LUN_IS_VALID(cmd->lun));

    volume = adm_cluster_get_volume_by_uuid(&cmd->volume_uuid);
    if (volume == NULL)
    {
        admwrk_ack(ctx, -ADMIND_ERR_UNKNOWN_VOLUMENAME);
        return;
    }

    if (lum_exports_get_type_by_uuid(&cmd->volume_uuid) != EXPORT_ISCSI)
    {
        admwrk_ack(ctx, -EXA_ERR_EXPORT_WRONG_METHOD);
        return;
    }

    EXA_ASSERT(!volume->started);

    err = lum_exports_iscsi_set_lun_by_uuid(&volume->uuid, cmd->lun);

    admwrk_ack(ctx, err);
}

/*******************************************************************************/
/* IQN authentification tuning                                                 */

typedef enum
{
#define VLTUNE_IQN__FIRST VLTUNE_IQN_ACCEPT
    VLTUNE_IQN_ACCEPT,
    VLTUNE_IQN_REJECT,
    VLTUNE_IQN_MODE,
#define VLTUNE_IQN__LAST VLTUNE_IQN_MODE
} vltune_iqnauth_param_t;

#define VLTUNE_IQN_PARAM_IS_VALID(param) \
    (param >= VLTUNE_IQN__FIRST && param <= VLTUNE_IQN__LAST)

typedef struct
{
    vltune_operation_t operation;
    vltune_iqnauth_param_t param;
    exa_uuid_t volume_uuid;
    char param_value[EXA_MAXSIZE_PARAM_VALUE+1];
} vltune_iqnauth_cmd_t;

/**
 * @brief Globally tune the allowed IQN authorization parameters
 */
static void
cluster_tune_iqnauth(admwrk_ctx_t *ctx, const struct adm_volume *volume,
		     vltune_operation_t operation, vltune_iqnauth_param_t param,
		     const char *param_value, cl_error_desc_t *err_desc)
{
    int ret = EXA_SUCCESS;
    vltune_iqnauth_cmd_t cmd;
    tunelist_t *tunelist;
    const char *result = NULL;

    /* Forge the command */
    memset(&cmd, 0, sizeof(cmd));
    cmd.operation = operation;
    cmd.param = param;
    uuid_copy(&cmd.volume_uuid, &volume->uuid);

    if (lum_exports_get_type_by_uuid(&volume->uuid) != EXPORT_ISCSI)
    {
        set_error(err_desc, -EXA_ERR_EXPORT_WRONG_METHOD, NULL);
        return;
    }

    if (param == VLTUNE_IQN_MODE &&
        (operation == VLTUNE_OPERATION_ADD ||
         operation == VLTUNE_OPERATION_REMOVE))
    {
        set_error(err_desc, -EXA_ERR_INVALID_VALUE,
                  "Operation not relevant for this parameter");
        return;
    }

    switch (operation)
    {
    case VLTUNE_OPERATION_ADD:
    case VLTUNE_OPERATION_REMOVE:
    case VLTUNE_OPERATION_SET:
        strlcpy(cmd.param_value, param_value, sizeof(cmd.param_value));
        break;
    case VLTUNE_OPERATION_RESET:
    case VLTUNE_OPERATION_GET:
        break;
    default:
        EXA_ASSERT_VERBOSE(false, "Operation not implemented:"
                           " operation=%i param=%i value='%s'",
                           operation, param, param_value);
        return;
    }

    /* As information is maintained synchronous between all nodes, whenever
     * asking for such information, no need to ask all nodes for a data
     * that is obviously up to date locally. That's why VLTUNE_OPERATION_GET
     * is not clusterised. */
    if (operation != VLTUNE_OPERATION_GET)
    {
        /* Send the command to the nodes */
        ret = admwrk_exec_command(ctx, &adm_service_admin,
			      RPC_ADM_VLTUNE_IQNAUTH, &cmd, sizeof(cmd));

        if (ret != EXA_SUCCESS)
           goto error;
    }

    /* Forge the reply */
    ret = tunelist_create(&tunelist);
    if (ret != EXA_SUCCESS)
        goto error;

    if (param == VLTUNE_IQN_ACCEPT)
        ret = tunelist_add_iqn_auth_accept(tunelist, volume);
    else if(param == VLTUNE_IQN_REJECT)
        ret = tunelist_add_iqn_auth_reject(tunelist, volume);
    else if(param == VLTUNE_IQN_MODE)
        ret = tunelist_add_iqn_auth_mode(tunelist, volume);

    if (ret != EXA_SUCCESS)
        goto error_free_tunelist;

    result = tunelist_get_result(tunelist);
    if (result == NULL)
        goto error_free_tunelist;

    /* Send the reply to the CLI */
    send_payload_str(result);

    tunelist_delete(tunelist);
    set_success(err_desc);
    return;

error_free_tunelist:
    tunelist_delete(tunelist);
error:
    set_error(err_desc, ret, NULL);
    return;
}


/**
 * @brief Locally tune the allowed IQN authorization parameters
 */
void local_tune_iqnauth(admwrk_ctx_t *ctx, void *msg)
{
    vltune_iqnauth_cmd_t *cmd = msg;
    const exa_uuid_t *volume_uuid = &cmd->volume_uuid;
    int ret = EXA_SUCCESS;

    if (adm_cluster_get_volume_by_uuid(volume_uuid) == NULL)
    {
	admwrk_ack(ctx, -ADMIND_ERR_UNKNOWN_VOLUMENAME);
	return;
    }

    if (lum_exports_get_type_by_uuid(&cmd->volume_uuid) != EXPORT_ISCSI)
    {
        admwrk_ack(ctx, -EXA_ERR_EXPORT_WRONG_METHOD);
        return;
    }

    ret = admwrk_barrier(ctx, ret, "Setting IQN authorization parameters");

    switch(cmd->param)
    {
        case VLTUNE_IQN_MODE:
        {
            iqn_filter_policy_t policy = IQN_FILTER_NONE;

            if (cmd->operation == VLTUNE_OPERATION_SET)
                policy = iqn_filter_policy_from_str(cmd->param_value);
            else if(cmd->operation == VLTUNE_OPERATION_RESET)
                policy = IQN_FILTER_DEFAULT_POLICY;

            if (policy == IQN_FILTER_NONE)
                ret = -EXA_ERR_INVALID_VALUE;
            else
                ret = lum_exports_iscsi_set_filter_policy_by_uuid(volume_uuid,
                                                                  policy);

            break;
        }
        case VLTUNE_IQN_ACCEPT:
        case VLTUNE_IQN_REJECT:
            switch(cmd->operation)
            {
                case VLTUNE_OPERATION_RESET:
                    ret = lum_exports_iscsi_clear_iqn_filters_policy_by_uuid(
                            volume_uuid,
                            cmd->param == VLTUNE_IQN_ACCEPT ?
                                          IQN_FILTER_ACCEPT : IQN_FILTER_REJECT);
                    break;
                case VLTUNE_OPERATION_ADD:
                    ret = lum_exports_iscsi_add_iqn_filter_by_uuid(
                            volume_uuid, cmd->param_value,
                            cmd->param == VLTUNE_IQN_ACCEPT ?
                                          IQN_FILTER_ACCEPT : IQN_FILTER_REJECT);
                    break;
                case VLTUNE_OPERATION_REMOVE:
                    ret = lum_exports_iscsi_remove_iqn_filter_by_uuid(
                            volume_uuid, cmd->param_value);
                    break;
                case VLTUNE_OPERATION_SET:
                case VLTUNE_OPERATION_GET:
                    ret = -ADMIND_ERR_VLTUNE_NON_APPLICABLE;
                    exalog_info("Command not applicable for this parameter.");
                    break;
            }
            break;
    }

    /* Send the update to the target only if the volume is exported */
    if (ret == EXA_SUCCESS && adm_volume_is_exported(volume_uuid))
    {
        size_t buf_size = export_serialized_size();
        char buf[buf_size];

        ret = lum_exports_serialize_export_by_uuid(volume_uuid, buf, buf_size);
        if (ret == EXA_SUCCESS)
            ret = lum_client_export_update_iqn_filters(adm_wt_get_localmb(),
                                                       buf, buf_size);
    }

    admwrk_ack(ctx, ret);
}

/*******************************************************************************/
/* Readahead tuning                                                            */

typedef struct
{
    exa_uuid_t volume_uuid;
    uint32_t readahead;
} vltune_readahead_cmd_t;


/**
 * @brief Set the read-ahead, global command
 *
 * @param[in] volume       to tune
 * @param[in] read_ahead   new value
 *
 * @return error code
 */
int vrt_master_volume_tune_readahead(admwrk_ctx_t *ctx, const struct adm_volume *volume,
                                     uint32_t read_ahead)
{
    vltune_readahead_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    cmd.readahead = read_ahead;
    uuid_copy(&cmd.volume_uuid, &volume->uuid);

    return admwrk_exec_command(ctx, &adm_service_admin,
                               RPC_ADM_VLTUNE_READAHEAD, &cmd, sizeof(cmd));
}


/**
 * @brief Globally tune the readahead parameter
 */
static void
cluster_tune_readahead(admwrk_ctx_t *ctx, const struct adm_volume *volume, vltune_operation_t operation,
                       const char *readahead_str, cl_error_desc_t *err_desc)
{
    int ret = EXA_SUCCESS;
    uint64_t readahead;
    const char *result;
    tunelist_t *tunelist;

    if (lum_exports_get_type_by_uuid(&volume->uuid) != EXPORT_BDEV)
    {
        set_error(err_desc, -EXA_ERR_EXPORT_WRONG_METHOD, NULL);
        return;
    }

    switch (operation)
    {
    case VLTUNE_OPERATION_GET:
	goto send_reply;

    case VLTUNE_OPERATION_RESET:
        readahead = adm_cluster_get_param_int("default_readahead");
        break;

    case VLTUNE_OPERATION_SET:
        ret = exa_get_size_kb(readahead_str, &readahead);

        if (ret != EXA_SUCCESS)
        {
            set_error(err_desc, -EXA_ERR_CMD_PARSING,
                      "Failed to parse the provided value '%s'. "
                      "Did you add a SI prefix (eg '512K' or '2M')?",
                      readahead_str);
            return;
        }

        /* The Linux kernel rounds to a multiple of PAGE_SIZE. */
        if (readahead % 4 != 0)
        {
            set_error(err_desc, -EXA_ERR_CMD_PARSING, "The readahead value"
                      " '%s' is not a multiple of 4 K.", readahead_str);
            return;
        }

        /* Prevent an integer overflow. The readahead expressed in sectors must
           fit in an unsigned int. */
        if (readahead > UINT32_MAX / 2)
        {
            set_error(err_desc, -EXA_ERR_CMD_PARSING, "The readahead value"
                    " '%s' is too big.", readahead_str);
            return;
        }
        break;

    default:
        set_error(err_desc, -EXA_ERR_INVALID_VALUE,
		  "Operation not implemented for this parameter");
        return;
    }

    ret = vrt_master_volume_tune_readahead(ctx, volume, (uint32_t)readahead);
    if (ret != EXA_SUCCESS)
        goto error;

send_reply:
    /* Forge the reply */
    ret = tunelist_create(&tunelist);
    if (ret != EXA_SUCCESS)
        goto error;

    ret = tunelist_add_readahead(tunelist, volume);

    if (ret != EXA_SUCCESS)
        goto error_free_tunelist;

    result = tunelist_get_result(tunelist);
    if (result == NULL)
        goto error_free_tunelist;

    /* Send the reply to the CLI */
    send_payload_str(result);

    tunelist_delete(tunelist);
    set_success(err_desc);
    return;

error_free_tunelist:
    tunelist_delete(tunelist);
error:
    set_error(err_desc, ret, NULL);
    return;
}


/**
 * @brief Locally tune the readahead parameter.
 *
 * @param volume       The volume to tune
 * @param readahead    The readahead value to set
 *
 * @return EXA_SUCCESS upon success. Otherwise an error code
 */
void local_vltune_readahead(admwrk_ctx_t *ctx, void *msg)
{
    vltune_readahead_cmd_t *cmd = msg;
    uint32_t readahead = cmd->readahead;
    struct adm_volume *volume;
    int ret = EXA_SUCCESS;

    volume = adm_cluster_get_volume_by_uuid(&cmd->volume_uuid);
    if (volume == NULL)
    {
	admwrk_ack(ctx, -ADMIND_ERR_UNKNOWN_VOLUMENAME);
	return;
    }

    if (lum_exports_get_type_by_uuid(&cmd->volume_uuid) != EXPORT_BDEV)
    {
        admwrk_ack(ctx, -EXA_ERR_EXPORT_WRONG_METHOD);
        return;
    }

    if (volume->started)
    {
        /* FIXME Should pass export->uuid, *not* volume->uuid */
        ret = lum_client_set_readahead(adm_wt_get_localmb(),
                                      &cmd->volume_uuid, readahead);
        if (ret != EXA_SUCCESS)
            exalog_warning("Failed setting readahead on %s:%s",
                           volume->group->name, volume->name);
    }

    ret = admwrk_barrier(ctx, ret, "Setting readahead");
    if (ret == EXA_SUCCESS)
        volume->readahead = readahead;

    /* FIXME In case of error on the barrier, which means not all nodes were
             able to set the readahead, it seems the old code attempted to
             revert the readahead to its previous value. Should we do this?
             But then, what if that fails? */

    admwrk_ack(ctx, ret);
}


/**
 * Returns the list of volume tunable parameters
 */
static void
cluster_vltune(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
    const struct vltune_params *params = data;
    struct adm_group *group;
    struct adm_volume *volume;
    vltune_operation_t operation;

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
        set_error(err_desc, -ADMIND_ERR_UNKNOWN_VOLUMENAME,
                  "Volume '%s' not found", params->volume_name);
        return;
    }

    /* If the volume is part of a file system, the user cannot get or
     * set the readahead parameter unless he provided the --nofscheck
     * option.
     */
    if (!params->no_fs_check && adm_volume_is_in_fs(volume))
    {
        set_error(err_desc, -ADMIND_ERR_VOLUME_IN_FS,
                  "volume '%s' is managed by the file system layer.",
                  volume->name);
        return;
    }

    /* Do not continue if the group is not started */
    if (volume->group->goal == ADM_GROUP_GOAL_STOPPED)
    {
        set_error(err_desc, -VRT_ERR_GROUP_NOT_STARTED, NULL);
        return;
    }

    if (strncmp("set", params->operation, EXA_MAXSIZE_PARAM_OPERATION) == 0)
	operation = VLTUNE_OPERATION_SET;
    else if (strncmp("get", params->operation, EXA_MAXSIZE_PARAM_OPERATION) == 0)
	operation = VLTUNE_OPERATION_GET;
    else if (strncmp("add", params->operation, EXA_MAXSIZE_PARAM_OPERATION) == 0)
	operation = VLTUNE_OPERATION_ADD;
    else if (strncmp("remove", params->operation, EXA_MAXSIZE_PARAM_OPERATION) == 0)
	operation = VLTUNE_OPERATION_REMOVE;
    else if (strncmp("reset", params->operation, EXA_MAXSIZE_PARAM_OPERATION) == 0)
	operation = VLTUNE_OPERATION_RESET;
    else
    {
        set_error(err_desc, -EXA_ERR_CMD_PARSING,
		  "Parameter operation '%s' is not implemented", params->operation);
        return;
    }


    exalog_info("received vltune from %s on '%s:%s'-> operation:'%s' param:'%s' value:'%s' %s",
                adm_cli_ip(), params->group_name, params->volume_name,
                params->operation, params->param_name, params->param_value,
                params->no_fs_check ? "(nofscheck)" : "");

    /* Determine the parameter to tune */
    if (strncmp(VLTUNE_PARAM_LUN, params->param_name,
                EXA_MAXSIZE_PARAM_NAME) == 0)
        cluster_tune_lun(ctx, volume, operation, params->param_value,
                         err_desc);
    else if (strncmp(VLTUNE_PARAM_IQN_AUTH_ACCEPT, params->param_name,
                     EXA_MAXSIZE_PARAM_NAME) == 0)
        cluster_tune_iqnauth(ctx, volume, operation, VLTUNE_IQN_ACCEPT,
			                 params->param_value, err_desc);
    else if (strncmp(VLTUNE_PARAM_IQN_AUTH_REJECT, params->param_name,
                     EXA_MAXSIZE_PARAM_NAME) == 0)
        cluster_tune_iqnauth(ctx, volume, operation, VLTUNE_IQN_REJECT,
			                 params->param_value, err_desc);
    else if (strncmp(VLTUNE_PARAM_IQN_AUTH_MODE, params->param_name,
                     EXA_MAXSIZE_PARAM_NAME) == 0)
        cluster_tune_iqnauth(ctx, volume, operation, VLTUNE_IQN_MODE,
			                 params->param_value, err_desc);
    else if (strncmp(VLTUNE_PARAM_READAHEAD, params->param_name,
                EXA_MAXSIZE_PARAM_NAME) == 0)
        cluster_tune_readahead(ctx, volume, operation,
                               params->param_value, err_desc);
    else
        set_error(err_desc, -EXA_ERR_CMD_PARSING,
                  "Unknown tuning parameter '%s'.", params->param_name);
}




/**
 * Definition of the vltune message.
 */
const AdmCommand exa_vltune = {
  .code            = EXA_ADM_VLTUNE,
  .msg             = "vltune",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_vltune,
  .local_commands  = {
    { RPC_ADM_VLTUNE_LUN, local_vltune_lun },
    { RPC_ADM_VLTUNE_IQNAUTH, local_tune_iqnauth },
    { RPC_ADM_VLTUNE_READAHEAD, local_vltune_readahead },
    { RPC_COMMAND_NULL, NULL }
  }
};


