/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_CLINFO_GROUP_H
#define __EXA_CLINFO_GROUP_H

#include <libxml/tree.h>

void local_clinfo_group_disk(int thr_nb, void *msg);
int cluster_clinfo_group_disks(int thr_nb, xmlNodePtr group_node, struct adm_group *group);
int cluster_clinfo_groups(int thr_nb, xmlNodePtr exanodes_node,
			  bool get_disks_info, bool get_vl_info,
			  bool get_fs_info, bool get_fs_size);

#endif
