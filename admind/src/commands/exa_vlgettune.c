/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <errno.h>

#include "admind/include/service_vrt.h"
#include "admind/include/service_lum.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admindstate.h"
#include "admind/src/deviceblocks.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/tunelist.h"
#include "admind/src/commands/exa_vlgettune.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/exa_nodeset.h"
#include "log/include/log.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "vrt/virtualiseur/include/vrt_client.h"

__export(EXA_ADM_VLGETTUNE) struct vlgettune_params
{
    char group_name[EXA_MAXSIZE_GROUPNAME + 1];
    char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
};


/* FIXME Pass in a lun instead of a volume */
int tunelist_add_lun(tunelist_t *tunelist, const struct adm_volume *volume)
{
    tune_t *tune_value;
    int ret;
    lun_t export_lun;

    tune_value = tune_create(1);
    if (tune_value == NULL)
        return -ENOMEM;

    export_lun = lum_exports_iscsi_get_lun_by_uuid(&volume->uuid);
    if (export_lun == LUN_NONE)
        return -EXA_ERR_EXPORT_NOT_FOUND;

    tune_set_name(tune_value, VLTUNE_PARAM_LUN);
    tune_set_default_value(tune_value, "The lowest available LUN");
    tune_set_description(tune_value,
            "The volume's LUN. The valid range is 0..255");
    tune_set_nth_value(tune_value, 0, "%" PRIlun, export_lun);

    ret = tunelist_add_tune(tunelist, tune_value);

    tune_delete(tune_value);
    return ret;
}

static int tunelist_add_iqn_auth(tunelist_t *tunelist,
                                 const struct adm_volume *volume,
                                 iqn_filter_policy_t policy)
{
    tune_t *tune_value;
    int ret, i, num_rules;
    int all_rules = lum_exports_iscsi_get_iqn_filters_number_by_uuid(&volume->uuid);

    /* First count the IQN filters matching the requested policy, so that we
     * can correctly size the tune_t list.
     */
    num_rules = 0;
    for (i = 0; i < all_rules; i++)
        if (lum_exports_iscsi_get_nth_iqn_filter_policy_by_uuid(&volume->uuid, i)
            == policy)
            num_rules++;

    tune_value = tune_create(num_rules);
    if (tune_value == NULL)
	return -ENOMEM;

    if (policy == IQN_FILTER_ACCEPT)
    {
        tune_set_name(tune_value, VLTUNE_PARAM_IQN_AUTH_ACCEPT);
        tune_set_default_value(tune_value, TUNE_EMPTY_LIST);
        tune_set_description(tune_value,
            "List of IQNs that are allowed access to the volume's LUN.");
    }
    else
    {
        tune_set_name(tune_value, VLTUNE_PARAM_IQN_AUTH_REJECT);
        tune_set_default_value(tune_value, TUNE_EMPTY_LIST);
        tune_set_description(tune_value,
            "List of IQNs that are denied access to the volume's LUN.");
    }

    /* Now fill the list */
    num_rules = 0;
    for (i = 0; i < all_rules; i++)
    {
        if (lum_exports_iscsi_get_nth_iqn_filter_policy_by_uuid(&volume->uuid, i)
            == policy)
        {
            const iqn_t *iqn = lum_exports_iscsi_get_nth_iqn_filter_by_uuid(&volume->uuid, i);
            tune_set_nth_value(tune_value, num_rules, IQN_FMT, IQN_VAL(iqn));
            num_rules++;
        }
    }

    ret = tunelist_add_tune(tunelist, tune_value);

    tune_delete(tune_value);
    return ret;
}

int tunelist_add_iqn_auth_accept(tunelist_t *tunelist, const struct adm_volume *volume)
{
    return tunelist_add_iqn_auth(tunelist, volume, IQN_FILTER_ACCEPT);
}

int tunelist_add_iqn_auth_reject(tunelist_t *tunelist, const struct adm_volume *volume)
{
    return tunelist_add_iqn_auth(tunelist, volume, IQN_FILTER_REJECT);
}


int tunelist_add_iqn_auth_mode(tunelist_t *tunelist, const struct adm_volume *volume)
{
    tune_t *tune_value;
    int ret;

    tune_value = tune_create(1);
    if (tune_value == NULL)
	return -ENOMEM;
    iqn_filter_policy_to_str(IQN_FILTER_DEFAULT_POLICY);
    tune_set_name(tune_value, VLTUNE_PARAM_IQN_AUTH_MODE);
    tune_set_default_value(tune_value, "%s",
            iqn_filter_policy_to_str(IQN_FILTER_DEFAULT_POLICY));
    tune_set_description(tune_value,
            "Mode of the IQN identification (accept|reject).");
    tune_set_nth_value(tune_value, 0, "%s", iqn_filter_policy_to_str(
            lum_exports_iscsi_get_filter_policy_by_uuid(&volume->uuid)));

    ret = tunelist_add_tune(tunelist, tune_value);

    tune_delete(tune_value);
    return ret;
}

int tunelist_add_readahead(tunelist_t *tunelist, const struct adm_volume *volume)
{
    int ret;
    tune_t *tune_value;
    char buffer[EXA_MAXSIZE_TUNE_VALUE];

    tune_value = tune_create(1);
    if (tune_value == NULL)
	return -ENOMEM;

    tune_set_name(tune_value, VLTUNE_PARAM_READAHEAD);

    exa_get_human_size(buffer, sizeof(buffer),
            adm_cluster_get_param_int("default_readahead"));
    tune_set_default_value(tune_value, "%s", buffer);

    exa_get_human_size(buffer, sizeof(buffer), volume->readahead);
    tune_set_nth_value(tune_value, 0, "%s", buffer);

    tune_set_description(tune_value,
            "The readahead size of the volume followed by a SI prefix "
            "(eg. '2 M' means two megabytes). Must be a multiple of 4 K. "
            "A big readahead size may increase the performance of large "
            "sequential reads.");

    ret =  tunelist_add_tune(tunelist, tune_value);

    tune_delete(tune_value);
    return ret;
}

static void cluster_vlgettune(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
    const struct vlgettune_params *params = data;
    struct adm_group *group;
    struct adm_volume *volume;
    tunelist_t *tunelist;
    int error_val;
    const char *result;


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

    /* If the volume is part of a file system, the user cannot get the
     * readahead parameter */
    if (adm_volume_is_in_fs(volume))
    {
	set_error(err_desc, -ADMIND_ERR_VOLUME_IN_FS,
		  "volume '%s' is managed by the file system layer.",
		  volume->name);
	return;
    }

    error_val = tunelist_create(&tunelist);
    if (error_val)
        goto error_tune_create;

    switch (lum_exports_get_type_by_uuid(&volume->uuid))
    {
    case EXPORT_ISCSI:
        error_val = tunelist_add_lun(tunelist, volume);
        if (error_val)
            goto error_tune;

        error_val = tunelist_add_iqn_auth_accept(tunelist, volume);
        if (error_val != EXA_SUCCESS)
            goto error_tune;

        error_val = tunelist_add_iqn_auth_reject(tunelist, volume);
        if (error_val != EXA_SUCCESS)
            goto error_tune;

        error_val = tunelist_add_iqn_auth_mode(tunelist, volume);
        if (error_val != EXA_SUCCESS)
            goto error_tune;

        break;

    case EXPORT_BDEV:
        error_val = tunelist_add_readahead(tunelist, volume);
        if (error_val)
            goto error_tune;
    }

    result = tunelist_get_result(tunelist);
    if (result == NULL)
        goto error_tune;

    send_payload_str(result);

    tunelist_delete(tunelist);
    set_success(err_desc);
    return;

error_tune:
    tunelist_delete(tunelist);

error_tune_create:
    set_error(err_desc, error_val, NULL);

    return;
}


const AdmCommand exa_vlgettune = {
    .code            = EXA_ADM_VLGETTUNE,
    .msg             = "vlgettune",
    .accepted_status = ADMIND_STARTED,
    .match_cl_uuid   = true,
    .cluster_command = cluster_vlgettune,
    .local_commands  = {
        { RPC_COMMAND_NULL, NULL }
    }
};





