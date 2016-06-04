/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_CLINFO_COMPONENTS_H
#define __EXA_CLINFO_COMPONENTS_H

#include <libxml/tree.h>

void local_clinfo_components(admwrk_ctx_t *ctx, void *msg);
int cluster_clinfo_components(admwrk_ctx_t *ctx, xmlNodePtr exanodes_node);

#endif
