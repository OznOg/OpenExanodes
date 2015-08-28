/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __COMMANDS_FS_COMMON__H
#define __COMMANDS_FS_COMMON__H

#include "fs/include/fs_data.h"
#include "admind/services/fs/service_fs.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_workthread.h"

void adm_warning_node_down(const exa_nodeset_t *list_to_check,
		           const char *message);

int fscommands_params_get_fs(const char *group_name,
                             const char *volume_name,
			     fs_data_t* fs,
			     fs_definition_t **fs_definition,
			     bool check_ok);

#endif
