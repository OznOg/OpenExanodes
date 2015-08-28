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

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_service.h"
#include "admind/src/admindstate.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"


/* --- local data ---------------------------------------------------- */

__export(EXA_ADM_CLTUNE) struct AdmTune
{
  char param[EXA_MAXSIZE_PARAM_NAME+1];
  char value[EXA_MAXSIZE_PARAM_VALUE+1];
};

/** set param=value in the config
 *
 * This works in cluster mode or not.
 */
static exa_error_code
tune(int thr_nb, const struct AdmTune *tune_info)
{
  exalog_info("received cltune %s=%s from %s",
	      tune_info->param, tune_info->value, adm_cli_ip());

  if (tune_info->value[0] == '\0')
    return adm_cluster_set_param_to_default(tune_info->param);
  else
    return adm_cluster_set_param(tune_info->param, tune_info->value);
}


/** \brief Clusterized cltune command
 *
 * \param[in] thr_nb	Worker thread id.
 */

static void
cluster_cltune(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  struct AdmTune *tune_info = (struct AdmTune *)data;
  int error_val, save_ret;

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  set_success(err_desc);

  error_val = tune(thr_nb, tune_info);
  if (error_val)
    goto error;

  exalog_debug("cltune clustered command complete");


  /* We test the goal because we want to make sure the cluster is not started
   * or starting.
   */
  switch(adm_cluster.goal)
  {
      case ADM_CLUSTER_GOAL_UNDEFINED:
      case ADM_CLUSTER_GOAL_STOPPED:
          save_ret = conf_save_synchronous();
          EXA_ASSERT_VERBOSE(save_ret == EXA_SUCCESS, "%s", exa_error_msg(save_ret));
          break;
      case ADM_CLUSTER_GOAL_STARTED:
          EXA_ASSERT_VERBOSE(false, "Inconsistent goal.");
          break;
  }

error:
  set_error(err_desc, error_val, NULL);
}

/**
 * Definition of the cltune command.
 */
const AdmCommand exa_cltune = {
  .code            = EXA_ADM_CLTUNE,
  .msg             = "cltune",
  .accepted_status = ADMIND_STOPPED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_cltune,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};
