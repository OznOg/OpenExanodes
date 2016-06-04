/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_CLINFO_NODE_H
#define __EXA_CLINFO_NODE_H

#include <libxml/tree.h>

struct disk_info_query
{
  exa_uuid_t uuid;
};


struct disk_info_reply
{
  bool has_disk;
  exa_uuid_t uuid;
  uint64_t size;
  char status[EXA_MAXSIZE_ADMIND_PROP + 1];
  char path[EXA_MAXSIZE_DEVPATH + 1];
};



int cluster_clinfo_nodes(admwrk_ctx_t *ctx, xmlNodePtr cluster_node);
void local_clinfo_node_disks(admwrk_ctx_t *ctx, void *msg);
void local_clinfo_disk_info(admwrk_ctx_t *ctx, void *msg);

#endif
