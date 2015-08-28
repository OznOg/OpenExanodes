/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SUP_HELPERS_H__
#define __SUP_HELPERS_H__

#include "csupd/src/sup_cluster.h"

#include "common/include/exa_nodeset.h"

#include <stdio.h>

int read_mship_set(FILE *file, unsigned *num_nodes, exa_nodeset_t **mship_tab,
		   exa_nodeset_t **expected_tab);

int read_cluster(FILE *file, sup_cluster_t *cluster, exa_nodeset_t **expected);

int open_cluster(const char *filename, sup_cluster_t *cluster,
		 exa_nodeset_t **expected);

#endif
