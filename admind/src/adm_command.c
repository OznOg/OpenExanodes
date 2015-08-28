/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/**
 * AdmCommand infrastructure.
 */
#include <stdio.h>
#include <string.h>

#include "admind/src/adm_command.h"

/* The AdmCommand structures are defined in each command source file */
extern const AdmCommand exa_clcreate;
extern const AdmCommand exa_cldelete;
extern const AdmCommand exa_clinfo;
extern const AdmCommand exa_clinit;
extern const AdmCommand exa_cldiskadd;
extern const AdmCommand exa_cldiskdel;
extern const AdmCommand exa_clnodeadd;
extern const AdmCommand exa_clnodedel;
extern const AdmCommand exa_clshutdown;
extern const AdmCommand exa_clstats;
extern const AdmCommand exa_clnodestop;
extern const AdmCommand exa_cltrace;
extern const AdmCommand exa_cltune;
#ifdef WITH_MONITORING
extern const AdmCommand exa_clmonitorstart;
extern const AdmCommand exa_clmonitorstop;
#endif

extern const AdmCommand exa_dgcreate;
extern const AdmCommand exa_dgdelete;
extern const AdmCommand exa_dgdiskrecover;
extern const AdmCommand exa_dgdiskadd;
extern const AdmCommand exa_dgstart;
extern const AdmCommand exa_dgstop;
extern const AdmCommand exa_dgreset;
extern const AdmCommand exa_dgcheck;

extern const AdmCommand exa_vlcreate;
extern const AdmCommand exa_vldelete;
extern const AdmCommand exa_vlresize;
extern const AdmCommand exa_vlstart;
extern const AdmCommand exa_vlstop;
extern const AdmCommand exa_vltune;
extern const AdmCommand exa_vlgettune;

#ifdef WITH_FS
extern const AdmCommand exa_fscheck;
extern const AdmCommand exa_fscreate;
extern const AdmCommand exa_fsdelete;
extern const AdmCommand exa_fsresize;
extern const AdmCommand exa_fsstart;
extern const AdmCommand exa_fsstop;
extern const AdmCommand exa_fstune;
extern const AdmCommand exa_fsgettune;
#endif

extern const AdmCommand exa_cmd_get_cluster_name;
extern const AdmCommand exa_cmd_get_config;
extern const AdmCommand exa_cmd_get_config_cluster;
extern const AdmCommand exa_cmd_get_param;
extern const AdmCommand exa_cmd_get_nodedisks;
extern const AdmCommand exa_cmd_get_license;

extern const AdmCommand exa_cmd_set_license;

extern const AdmCommand run_shutdown;
extern const AdmCommand run_recovery;

/**
 * Array of all commands available in the XML protocol from the GUI or
 * CLI.
 */
static const AdmCommand *adm_commands[] =
  {
    & exa_clcreate,
    & exa_cldelete,
    & exa_clinfo,
    & exa_clinit,
    & exa_cldiskadd,
    & exa_cldiskdel,
    & exa_clnodeadd,
    & exa_clnodedel,
    & exa_clshutdown,
    & exa_clstats,
    & exa_clnodestop,
    & exa_cltrace,
    & exa_cltune,
#ifdef WITH_MONITORING
    & exa_clmonitorstart,
    & exa_clmonitorstop,
#endif
    & exa_cmd_get_cluster_name,
    & exa_cmd_get_config,
    & exa_cmd_get_config_cluster,
    & exa_cmd_get_param,
    & exa_cmd_get_nodedisks,
    & exa_cmd_get_license,
    & exa_cmd_set_license,
    & exa_dgcreate,
    & exa_dgdelete,
    & exa_dgdiskrecover,
    & exa_dgdiskadd,
    & exa_dgstart,
    & exa_dgstop,
    & exa_dgreset,
    & exa_dgcheck,
#ifdef WITH_FS
    & exa_fscheck,
    & exa_fscreate,
    & exa_fsdelete,
    & exa_fsresize,
    & exa_fsstart,
    & exa_fsstop,
    & exa_fstune,
    & exa_fsgettune,
#endif
    & exa_vlcreate,
    & exa_vldelete,
    & exa_vlresize,
    & exa_vlstart,
    & exa_vlstop,
    & exa_vltune,
    & exa_vlgettune,
    &run_shutdown,
    &run_recovery
  };

/**
 * Find a XML command using its code
 *
 * @param[in] code Code of the XML command
 *
 * @return Pointer to the AdmCommand structure if the command exists,
 *         NULL otherwise
 */
const AdmCommand *
adm_command_find(adm_command_code_t code)
{
  int i;

  for (i = 0; i < (sizeof(adm_commands) / sizeof(AdmCommand*)); i++)
  {
    if (adm_commands[i]->code == code)
      return adm_commands[i];
  }

  return NULL;
}

const char *
adm_command_name(adm_command_code_t code)
{
  const AdmCommand *cmd = adm_command_find(code);

  return cmd ? cmd->msg : NULL;
}

/**
 * Register the local commands in the global array
 * of local commands
 */
void
adm_command_init_processing(void)
{
  int i, j;

  for (i = 0; i < (sizeof(adm_commands) / sizeof(AdmCommand*)); i++)
  {
    const AdmCommand *cmd = adm_commands[i];

    /* Register local commands */
    for (j = 0; cmd->local_commands[j].fct != NULL; j++)
      rpc_command_set(cmd->local_commands[j].id, cmd->local_commands[j].fct);
  }
}


