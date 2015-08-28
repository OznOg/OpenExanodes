/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */



#ifndef __SERVICE_FS_COMMANDS_H__
#define __SERVICE_FS_COMMANDS_H__

#include "admind/src/admind.h"
#include "common/include/exa_nodeset.h"
#include "os/include/os_inttypes.h"
#include "fs/include/fs_data.h"

struct adm_group;
struct adm_fs;

int fs_start_all_fs(int thr_nb, const struct adm_group *group);

int fs_stop_all_fs(int thr_nb, struct adm_group *group,
		   const exa_nodeset_t *nodelist,
		   int force, adm_goal_change_t goal_change);

#endif /* __SERVICE_FS_COMMANDS_H__ */
