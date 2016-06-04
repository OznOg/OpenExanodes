/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_CLINFO_VOLUME_H
#define __EXA_CLINFO_VOLUME_H

#include <libxml/tree.h>

void local_clinfo_volume(admwrk_ctx_t *ctx, void *msg);
int cluster_clinfo_volumes(admwrk_ctx_t *ctx, xmlNodePtr group_node,
			   struct adm_group *group,
			   bool get_fs_info, bool get_fs_size);

#endif
