/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __TYPE_GFS__H
#define __TYPE_GFS__H

#include "admind/services/fs/generic_fs.h"

extern const fs_definition_t gfs_definition;

void gfs_init();

bool gfs_using_gulm(void);
bool gfs_using_dlm(void);
bool gfs_is_gulm_lockserver(exa_nodeid_t nodeid);
exa_nodeid_t gfs_get_designated_gulm_master(void);

int gfs_shutdown(admwrk_ctx_t *ctx);
int gfs_global_recover(admwrk_ctx_t *ctx,
		       exa_nodeset_t* nodes_up_in_progress,
		       exa_nodeset_t* nodes_down_in_progress,
		       exa_nodeset_t* committed_up);
int gfs_manage_node_stop(const exa_nodeset_t *nodes_to_stop, exa_nodeset_t* nodes_up);
void gfs_nodedel(admwrk_ctx_t *ctx, struct adm_node *node, exa_nodeset_t* nodes_up);

#endif
