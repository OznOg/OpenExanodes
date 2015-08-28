/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "log/include/log.h"

#include "common/include/exa_error.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_names.h"
#include "os/include/strlcpy.h"

#include "admind/src/commands/exa_fscommon.h"

__export(EXA_ADM_FSRESIZE) struct fsresize_params
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  int64_t sizeKB;
};


/** \brief Implements the fsresize command
 *
 * - Check the file system transaction parameter. It should be "COMMITTED".
 * - Resize it on the specified nodes, using FS specific function.
 */
static void
cluster_fsresize(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct fsresize_params *params = data;
  fs_data_t fs;
  fs_definition_t *fs_definition;
  int error_val;

  exalog_info("received fsresize '%s:%s' --size=%" PRIu64 "KB from '%s'",
	      params->group_name, params->volume_name,
	      params->sizeKB, adm_cli_ip());

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

  EXA_ASSERT_VERBOSE(fs_definition, "Unknown file system type found.");

  /* Now check that the file system transaction is COMMITTED */
  if (!fs.transaction)
    {
      set_error(err_desc, -FS_ERR_INVALID_FILESYSTEM, NULL);
      return;
    }

  /* Check it can be resized. Then resize it. */
  EXA_ASSERT(fs_definition->resize_fs);
  error_val = fs_definition->resize_fs(thr_nb, &fs, params->sizeKB);
  if (error_val != EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }

  /* Need an update of the "size" value */
  error_val = fs_update_tree(thr_nb, &fs);
  if (error_val != EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }

  set_success(err_desc);
}


/**
 * Definition of the exa_fsresize command.
 */
const AdmCommand exa_fsresize = {
  .code            = EXA_ADM_FSRESIZE,
  .msg             = "fsresize",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_fsresize,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};


