/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/*!\file exa_names.c
 * \brief This file should contain material concerning deamons and modules
 * naming
 */
#include <stdlib.h>
#include "common/include/exa_names.h"

static const char *module_names[] = {
    [EXA_MODULE_RDEV]         = EXA_RDEV_MODULE_NAME,
#ifdef WITH_BDEV
    [EXA_MODULE_NBD]          = NBD_MODULE_NAME,
#endif
};

const char *exa_module_name(exa_module_id_t id)
{
  if (!exa_module_check(id))
    return NULL;
  return module_names[id];
}

static const char *daemon_names[] = {
    [EXA_DAEMON_ADMIND]     = "exa_admind",
    [EXA_DAEMON_SERVERD]    = "exa_serverd",
    [EXA_DAEMON_CLIENTD]    = "exa_clientd",
    [EXA_DAEMON_FSD]        = "exa_fsd",
    [EXA_DAEMON_CSUPD]      = "exa_csupd",
    [EXA_DAEMON_MSGD]       = "exa_msgd",
    [EXA_DAEMON_LOCK_GULMD] = "lock_gulmd_core",
    [EXA_DAEMON_MONITORD]   = "exa_md",
    [EXA_DAEMON_AGENTX]     = "exa_agentx",
};

const char *exa_daemon_name(exa_daemon_id_t id)
{
  if (!exa_daemon_check(id))
    return NULL;
  return daemon_names[id];
}



