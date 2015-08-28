/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RDEV__H
#define __RDEV__H

#include "common/include/uuid.h"

struct adm_disk;
struct adm_node;

int rdev_initialize_sb(const char *path, const exa_uuid_t *disk_uuid);

int rdev_remove_broken_disks_file(void);

#endif
