/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <string.h>

#include "admind/src/adm_cache.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_hostname.h"
#include "admind/src/admindstate.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_license.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"

__export(EXA_ADM_CLDELETE) struct cldelete_params
  {
    __optional bool recursive __default(false);
  };


/** \brief Special local command that executes cldelete.
 *
 * \param[in] ctx: thread id
 * \return void
 *
 */
static void
local_exec_cldelete(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct cldelete_params *params = data;
  char err_msg [EXA_MAXSIZE_LINE + 1];
  int error_val;
  ExamsgAny msg;
  int ack;

  /* Log this command */
  exalog_info("received cldelete%s from %s",
	      params->recursive ? " --recursive" : "", adm_cli_ip());

  err_msg[0] = '\0';		/* Empty error message */

  if (!params->recursive)
    {
      struct adm_group *group;

      /* We allow non recursive delete only if there are no disk groups created */
      adm_group_for_each_group(group)
	{
	  error_val = -ADMIND_ERR_CLUSTER_NOT_EMPTY;
          strlcpy(err_msg, exa_error_msg(error_val), sizeof(err_msg));
	  goto local_exec_cldelete_done;
	}
    }
  else
    {
      struct adm_group *group;

      /* We remove the groups sb_version files */
      adm_group_for_each_group(group)
      {
          sb_version_delete(group->sb_version);
          group->sb_version = NULL;
      }
    }

  /* send event */
  msg.type = EXAMSG_ADM_CLDELETE;

  error_val = examsgSendWithAck(adm_wt_get_localmb(), EXAMSG_ADMIND_EVMGR_ID,
			EXAMSG_LOCALHOST, (Examsg*)&msg, sizeof(msg), &ack);
  error_val = error_val < 0 ? error_val : ack;

  adm_hostname_reset();

  /* Cleanup cache directory */
  adm_cache_cleanup();

local_exec_cldelete_done:
  exalog_debug("local_exec_cldelete_done is_leader=%d error=%d",
	       adm_is_leader(), error_val);

  set_error(err_desc, error_val, err_msg);
}


/**
 * Definition of the cldelete command.
 */
const AdmCommand exa_cldelete = {
  .code            = EXA_ADM_CLDELETE,
  .msg             = "cldelete",
  .accepted_status = ADMIND_STOPPED,
  .match_cl_uuid   = true,
  .cluster_command = local_exec_cldelete,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};
