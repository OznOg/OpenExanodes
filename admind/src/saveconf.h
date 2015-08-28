/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SAVECONF_THREAD_H
#define __SAVECONF_THREAD_H

#include "os/include/os_file.h"

#define ADMIND_CONF_EXANODES_FILE   "exanodes.conf"

/* called from CLI commands when a change is done in the configfile */
int conf_save_synchronous(void);

/* called from clshutdown (from clstop or clnodestop) or admind
   recovery */
int conf_save_synchronous_without_inc_version(void);

/* called from CLI exa_cldelete */
void conf_delete(void);

/* Memory cleanup */
void conf_cleanup(void);

#endif
