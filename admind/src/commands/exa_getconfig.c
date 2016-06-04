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
#include "admind/src/adm_node.h"
#include "admind/src/adm_serialize.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/rpc.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "admind/src/commands/command_api.h"

__export(EXA_ADM_GETCONFIG) __no_param;
__export(EXA_ADM_GETCONFIGCLUSTER) __no_param;

/*------------------------------------------------------------------------------*/
/** \brief Implements the get_config cluster command
 *
 * Implementation of the exa_clinfo command. Basically, it builds a
 * copy of the configuration XML tree and send it back to the user
 * interface.
 *
 * \param [in] ctx: thread id.
 */
/*------------------------------------------------------------------------------*/
static void
cluster_get_config(admwrk_ctx_t *ctx, void *dummy, cl_error_desc_t *err_desc)
{
  char *buffer = NULL;
  int size;
  int ret;

  exalog_debug("getconfig");

  /* Compute buffer size */
  adm_cluster_lock();
  size = adm_serialize_to_null(true /* create */);
  adm_cluster_unlock();

  /* Alloc a buffer */
  buffer = os_malloc(size + 1);
  if (buffer == NULL)
  {
    set_error(err_desc, -ENOMEM, "Unable to allocate memory to perform command.");
    return;
  }

  /* Serialize the config */
  adm_cluster_lock();
  ret = adm_serialize_to_memory(buffer, size + 1, true /* create */);
  adm_cluster_unlock();
  if (ret < EXA_SUCCESS)
  {
    set_error(err_desc, ret, NULL);
    return;
  }

  send_payload_str(buffer);

  os_free(buffer);

  set_success(err_desc);
}

/**
 * Definition of the getconfig command.
 */
const AdmCommand exa_cmd_get_config = {
  .code            = EXA_ADM_GETCONFIG,
  .msg             = "getconfig",
  .accepted_status = ~ADMIND_NOCONFIG,
  .match_cl_uuid   = false,
  .allowed_in_recovery = true,
  .cluster_command = cluster_get_config,
  .local_commands  = {
   { RPC_COMMAND_NULL, NULL }
  }
};

/**
 * Definition of the getconfigcluster command, a clusterized version
 * of the same.
 */
const AdmCommand exa_cmd_get_config_cluster = {
  .code            = EXA_ADM_GETCONFIGCLUSTER,
  .msg             = "getconfigcluster",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .allowed_in_recovery = true,
  .cluster_command = cluster_get_config,
  .local_commands  = {
   { RPC_COMMAND_NULL, NULL }
  }
};

