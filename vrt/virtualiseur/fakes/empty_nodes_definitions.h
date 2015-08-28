/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __UT_VRT_NODES_FAKES_H__
#define __UT_VRT_NODES_FAKES_H__

int vrt_node_get_upnodes_count(void)
{
    return 1;
}
void vrt_nodes_init(int node_id)
{
}

unsigned int vrt_node_get_local_id(void)
{
    return 0;
}
void vrt_node_set_upnodes (const exa_nodeset_t *nodes_up)
{
}

void vrt_node_get_upnodes(exa_nodeset_t *nodeset)
{
}

int vrt_node_get_upnode_id(void)
{
    return 0;
}

#endif /* __UT_VRT_NODES_FAKES_H__ */
