/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "admind/src/admindstate.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr.h"
#include "log/include/log.h"
#include "admind/src/commands/command_api.h"

__export(EXA_ADM_GET_CLUSTER_NAME) __no_param;

/** \brief Implements the get_cluster_name command
 *
 * Just return the current cluster name
 */
static void
cluster_get_cluster_name(int thr_nb, void *dummy, cl_error_desc_t *err_desc)
{
  if (!adm_cluster.created)
    {
      set_error(err_desc, -EXA_ERR_ADMIND_NOCONFIG,
                exa_error_msg(-EXA_ERR_ADMIND_NOCONFIG));
      return;
    }

  send_payload_str(adm_cluster.name);

  set_success(err_desc);
}

const AdmCommand exa_cmd_get_cluster_name = {
  .code            = EXA_ADM_GET_CLUSTER_NAME,
  .msg             = "get_cluster_name",
  .accepted_status = ADMIND_ANY,
  .match_cl_uuid   = false,
  .allowed_in_recovery = true,
  .cluster_command = cluster_get_cluster_name,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};

