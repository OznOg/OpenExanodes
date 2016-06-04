/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/commands/command_api.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/adm_command.h"
#include "admind/services/rdev/include/rdev_config.h"

#include "os/include/os_mem.h"

__export(EXA_ADM_GET_NODEDISKS) __no_param;

/** \brief Implements the get_nodedisks command
 *
 * return availavble disks for cluster
 */
static void cluster_get_nodedisks(admwrk_ctx_t *ctx, void *dummy, cl_error_desc_t *err_desc)
{
    char *buffer = NULL;

    buffer = rdev_get_path_list(err_desc);

    if (err_desc->code != EXA_SUCCESS)
	return;

    send_payload_str(buffer);

    os_free(buffer);

    set_success(err_desc);
}

const AdmCommand exa_cmd_get_nodedisks = {
    .code            = EXA_ADM_GET_NODEDISKS,
    .msg             = "get_nodedisks",
    .accepted_status = ADMIND_ANY,
    .match_cl_uuid   = false,
    .allowed_in_recovery = false,
    .cluster_command = cluster_get_nodedisks,
    .local_commands  = {
	{ RPC_COMMAND_NULL, NULL }
    }
};

