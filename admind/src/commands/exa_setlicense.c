/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/rpc.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_config.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"


__export(EXA_ADM_SETLICENSE) struct setlicense_params
{
    xmlDocPtr license;
};

/* --- cluster_exa_setlicense ------------------------------------------- */

/** \brief Clusterized setlicense command
 *
 * \param[in] ctx	Worker thread id.
 */

static void
cluster_setlicense(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
    struct setlicense_params *params = (struct setlicense_params*)data;
    xmlDocPtr license_dptr = params->license;
    xmlNodePtr license_nptr;
    const char *license_str;
    adm_license_t *new_license;

    exalog_info("received setlicense from %s", adm_cli_ip());

    license_nptr = xml_conf_xpath_singleton(license_dptr, "/license");
    if (!license_nptr)
    {
	set_error(err_desc, -EINVAL, "Command XML is malformed, cannot find element 'license'");
	xmlFreeDoc(license_dptr);
	return;
    }

    license_str = xml_get_prop(license_nptr, "raw");
    if (license_str == NULL)
    {
	set_error(err_desc, -EINVAL, "Command XML is malformed, cannot find argument 'raw'");
	xmlFreeDoc(license_dptr);
	return;
    }


    new_license = adm_license_install(license_str, strlen(license_str),
                                           err_desc);
    if (new_license == NULL)
    {
	xmlFreeDoc(license_dptr);
	return;
    }

    adm_license_delete(exanodes_license);
    exanodes_license = new_license;

    xmlFreeDoc(license_dptr);
}


/**
 * Definition of the setlicense command.
 */
const AdmCommand exa_cmd_set_license = {
    .code            = EXA_ADM_SETLICENSE,
    .msg             = "setlicense",
    .accepted_status = ADMIND_STOPPED,
    .match_cl_uuid   = true,
    .allowed_in_recovery = false,
    .cluster_command = cluster_setlicense,
    .local_commands  = {
	{ RPC_COMMAND_NULL, NULL }
    }
};
