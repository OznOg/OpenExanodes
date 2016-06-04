/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "os/include/os_file.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_license.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_file_ops.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/adm_workthread.h"
#include "common/include/exa_env.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/os_mem.h"
#include "log/include/log.h"

__export(EXA_ADM_GETLICENSE) __no_param;

static void
cluster_get_license(admwrk_ctx_t *ctx, void *dummy, cl_error_desc_t *err_desc)
{
  char *buffer;
  char path[OS_PATH_MAX];

  exalog_debug("getlicense");

  exa_env_make_path(path, sizeof(path), exa_env_cachedir(), ADM_LICENSE_FILE);

  buffer = adm_file_read_to_str(path, err_desc);
  if (buffer == NULL)
      return;

  send_payload_str(buffer);

  os_free(buffer);

  set_success(err_desc);
}


/**
 * Definition of the getlicense command.
 */
const AdmCommand exa_cmd_get_license = {
  .code            = EXA_ADM_GETLICENSE,
  .msg             = "getlicense",
  .accepted_status = ~ADMIND_NOCONFIG,
  .match_cl_uuid   = false,
  .allowed_in_recovery = true,
  .cluster_command = cluster_get_license,
  .local_commands  = {
   { RPC_COMMAND_NULL, NULL }
  }
};
