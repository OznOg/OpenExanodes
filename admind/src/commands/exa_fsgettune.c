/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <errno.h>

#include "log/include/log.h"

#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "os/include/strlcpy.h"

#include "admind/services/fs/service_fs.h"
#include "admind/services/fs/generic_fs.h"
#include "admind/src/commands/exa_fscommon.h"
#include "admind/src/commands/tunelist.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"

__export(EXA_ADM_FSGETTUNE) struct fsgettune_params
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
};


/** \brief Implements the fsgettune command
 *
 * - get FS
 * - call the gettune callback for this type of FS, as many times as required.
 */
static void
cluster_fsgettune(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct fsgettune_params *params = data;
  fs_data_t fs;
  fs_definition_t* fs_definition;
  tune_t *tune_value;
  int error_val;
  tunelist_t* tunelist;
  const char* result;

  exalog_info("received fsgettune '%s:%s' from %s",
	      params->group_name, params->volume_name, adm_cli_ip());

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

  EXA_ASSERT(fs_definition->gettune);

  tune_value = tune_create(1);
  if (tune_value == NULL)
  {
      error_val = -ENOMEM;
      goto error_tune_create;
  }

  error_val = tunelist_create(&tunelist);
  if (error_val)
    goto error_tunelist_create;

  /* get first */
  while (fs_definition->gettune(thr_nb, &fs, tune_value, &error_val))
    {
      error_val = tunelist_add_tune(tunelist,
				    tune_value);
      if (error_val)
	goto error_tune;
    }
  if (error_val) /* in case gettune returned an error */
    goto error_tune;

  result = tunelist_get_result(tunelist);
  if (result == NULL)
    goto error_tune;

  send_payload_str(result);

  tune_delete(tune_value);
  tunelist_delete(tunelist);
  set_success(err_desc);
  return;

 error_tune:
  tunelist_delete(tunelist);
 error_tunelist_create:
  tune_delete(tune_value);
 error_tune_create:
  set_error(err_desc, error_val, NULL);
}


/**
 * Definition of the exa_fsgettune command.
 */
const AdmCommand exa_fsgettune = {
  .code            = EXA_ADM_FSGETTUNE,
  .msg             = "fsgettune",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_fsgettune,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};


