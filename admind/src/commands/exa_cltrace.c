/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "admind/src/adm_cluster.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/rpc.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"

__export(EXA_ADM_CLTRACE) struct cltrace_params
  {
    uint32_t component;
    uint32_t level;
  };


/* --- cluster_exa_cltrace ------------------------------------------- */

/** \brief Clusterized cltrace command
 *
 * \param[in] ctx	Worker thread id.
 */

static void
cluster_cltrace(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct cltrace_params *params = data;

  exalog_info("received cltrace --level %d --component %d from %s",
	      params->level, params->component, adm_cli_ip());

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  if (!EXAMSG_ID_VALID((int32_t)params->component)
      && params->component != EXAMSG_ALL_COMPONENTS)
    {
      set_error(err_desc, EXA_ERR_DEFAULT, "Invalid component id %d",
	        params->component);
      return;
    }

  /* FIXME there is no check for loglevel value validity */
  /* We have all the data, call the configure */
  if (exalog_configure(params->component, params->level) != EXA_SUCCESS)
    set_error(err_desc, EXA_ERR_DEFAULT, NULL);
  else
    set_success(err_desc);

  exalog_debug("cltrace clustered command complete");
}


/**
 * Definition of the cltrace command.
 */
const AdmCommand exa_cltrace = {
  .code            = EXA_ADM_CLTRACE,
  .msg             = "cltrace",
  .accepted_status = ADMIND_ANY,
  .match_cl_uuid   = true,
  .allowed_in_recovery = true,
  .cluster_command = cluster_cltrace,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};
