/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "log/include/log.h"

#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "os/include/strlcpy.h"

#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"

#include "admind/services/fs/service_fs.h"
#include "admind/src/commands/exa_fscommon.h"

__export(EXA_ADM_FSTUNE) struct fstune_params
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  char option[32];		/* FIXME Ugly this is supposed to */
				/* be linked to EXA_FSTUNE_GFS_LOGS or else ? */
  char value[128];		/* FIXME Ugly */
};


/** \brief Implements the fstune command
 *
 * - get FS, options name and value.
 * - call the tune callback for this type of FS.
 */
static void
cluster_fstune(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct fstune_params *params = data;
  fs_data_t fs;
  fs_definition_t* fs_definition;
  int error_val;

  exalog_info("received fstune '%s:%s' %s=%s from %s",
	      params->group_name, params->volume_name, params->option,
	      params->value, adm_cli_ip());

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  /* Get the FS */
  if ((error_val = fscommands_params_get_fs(params->group_name,
	                                    params->volume_name,
					    &fs,
					    &fs_definition,
					    true)) != EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }

  EXA_ASSERT(fs_definition->tune);

  error_val = fs_definition->tune(ctx, &fs, params->option, params->value);
  if (error_val != EXA_SUCCESS)
    {
      set_error(err_desc, error_val, exa_error_msg(error_val));
      return;
    }

  set_success(err_desc);
}


/**
 * Definition of the exa_fstune command.
 */
const AdmCommand exa_fstune = {
  .code            = EXA_ADM_FSTUNE,
  .msg             = "fstune",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_fstune,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};


