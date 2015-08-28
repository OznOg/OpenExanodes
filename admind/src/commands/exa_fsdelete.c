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
#include "os/include/strlcpy.h"

#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "admind/src/commands/exa_fscommon.h"

#include "admind/include/service_fs_commands.h"
#include "admind/include/service_vrt.h"

__export(EXA_ADM_FSDELETE) struct fsdelete_params
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  __optional bool metadata_recovery __default(false);
};


/** \brief Implements the fsdelete command
 *
 * Only call appropriate VRT call to volume.
 * VRT code will automatically delete associated FS info.
 */
static void
cluster_fsdelete(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct fsdelete_params *params = data;
  fs_data_t fs;
  int error_val;

  exalog_info("received fsdelete '%s:%s' from '%s'",
	      params->group_name, params->volume_name, adm_cli_ip());

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  /* Get the FS */
  error_val = fscommands_params_get_fs(params->group_name, params->volume_name,
				       &fs, NULL, false);
  if (error_val == EXA_SUCCESS)
      error_val = vrt_master_volume_delete(thr_nb, fs_get_volume(&fs), false);

  if (error_val != EXA_SUCCESS)
    {
      exalog_error("fsdelete '%s:%s' failed: %s (%d)",
                params->group_name, params->volume_name,
		exa_error_msg(error_val), error_val);
      set_error(err_desc, error_val, NULL);
      return;
    }

  set_success(err_desc);
}


/**
 * Definition of the exa_fsdelete command.
 */
const AdmCommand exa_fsdelete = {
  .code            = EXA_ADM_FSDELETE,
  .msg             = "fsdelete",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_fsdelete,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};
