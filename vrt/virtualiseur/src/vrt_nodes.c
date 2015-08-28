/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**@file
 *
 * This file contains data structures and functions needed to keep track of the
 * different nodes of the cluster (their state up or down).
 */

#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "vrt/virtualiseur/include/vrt_group.h"

/* is the node up */
static exa_nodeset_t nodes_up;

/** ID of the node on which this virtualizer is running */
static unsigned int vrt_node_id;


void vrt_nodes_init(int node_id)
{
    vrt_node_id = node_id;
    exa_nodeset_reset(&nodes_up);
}

unsigned int vrt_node_get_local_id(void)
{
    return vrt_node_id;
}

int vrt_node_get_upnodes_count(void)
{
    return exa_nodeset_count(&nodes_up);
}

void vrt_node_get_upnodes(exa_nodeset_t *nodeset)
{
    exa_nodeset_copy(nodeset, &nodes_up);
}

void vrt_node_set_upnodes(const exa_nodeset_t *new_nodes_up)
{
    exa_nodeset_copy(&nodes_up, new_nodes_up);
}

int vrt_node_get_upnode_id(void)
{
    int upnode_id = -1;
    exa_nodeid_t node;

    for (node = 0 ; node < EXA_MAX_NODES_NUMBER ; node++)
    {
	if (exa_nodeset_contains(&nodes_up, node))
	{
	    upnode_id++;
	    if (node == vrt_node_id)
		return upnode_id;
	}
    }

    EXA_ASSERT_VERBOSE(false,
		       "Upnode ID not found. upnode_id=%d vrt_node_get_upnodes_count=%d vrt_node_id=%d",
		       upnode_id,
		       vrt_node_get_upnodes_count(),
		       vrt_node_id);
    return 0;
}

