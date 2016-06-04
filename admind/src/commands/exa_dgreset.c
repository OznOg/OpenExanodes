/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/adm_service.h" /* for adm_service_admin */
#include "admind/src/adm_command.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/rpc.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "log/include/log.h"
#include "vrt/virtualiseur/include/vrt_client.h"


__export(EXA_ADM_DGRESET) struct dgreset_params
{
    char groupname[EXA_MAXSIZE_GROUPNAME + 1];
};


static void cluster_dgreset(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
    const struct dgreset_params *params = data;
    admwrk_request_t handle;
    struct adm_group *group;
    int error_val, reply_ret;

    exalog_info("received dgreset '%s' from %s", params->groupname,
        adm_cli_ip());

    /* Check the license status to send warnings/errors */
    cmd_check_license_status();

    group = adm_group_get_group_by_name(params->groupname);
    if (group == NULL)
    {
        set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME,
                  "Group '%s' not found", params->groupname);
        return;
    }

    admwrk_run_command(ctx, &adm_service_admin, &handle, RPC_ADM_DGRESET,
                       &group->uuid, sizeof(group->uuid));

    error_val = EXA_SUCCESS;
    while (admwrk_get_ack(&handle, NULL, &reply_ret))
    {
        if (reply_ret != EXA_SUCCESS)
            error_val = reply_ret;
    }

    if (error_val != EXA_SUCCESS)
        set_error(err_desc, error_val, "%s", exa_error_msg(error_val));
    else
        set_success(err_desc);
}

static void local_exa_dgreset(admwrk_ctx_t *ctx, void *msg)
{
    int error_val;
    exa_uuid_t *group_uuid = msg;

    error_val = vrt_client_group_reset(adm_wt_get_localmb(), group_uuid);

    admwrk_ack(ctx, error_val);
}

/**
 * Definition of the dgreset command.
 */
const AdmCommand exa_dgreset = {
    .code            = EXA_ADM_DGRESET,
    .msg             = "dgreset",
    .accepted_status = ADMIND_STARTED,
    .match_cl_uuid   = true,
    .cluster_command = cluster_dgreset,
    .local_commands  =  {
        { RPC_ADM_DGRESET, local_exa_dgreset },
        { RPC_COMMAND_NULL, NULL }
    }
};

