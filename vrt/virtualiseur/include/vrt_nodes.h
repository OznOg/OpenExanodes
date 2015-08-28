/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __VRT_NODES_H__
#define __VRT_NODES_H__


#include "common/include/exa_nodeset.h"

/**
 * Initialize the virtualizer nodes infrastructure. It must be
 * called before any other node-related function is called.
 *
 * @param[in] node_id    ID of the current node
 */
void vrt_nodes_init(int node_id);

/**
 * Get the index of the local node ID
 */
unsigned int vrt_node_get_local_id(void);

/**
 * Set the list of nodes UP inside the virtualizer's list of nodes. It
 * can be called at anytime, during a recovery, i.e between a suspend
 * and a resume.
 *
 * @param[in] new_nodes_up      Set of nodes UP
 */
void vrt_node_set_upnodes (const exa_nodeset_t *nodes_up);

/**
 * Get the list of nodes UP from the virtualizer's list of nodes. It
 * can be called at anytime, during a recovery, i.e between a suspend
 * and a resume.
 *
 * @param[out] nodes_up     Set of nodes UP
 */
void vrt_node_get_upnodes(exa_nodeset_t *nodeset);

/**
 * Get the number of nodes UP
 */
int vrt_node_get_upnodes_count(void);

/**
 * Get the index of the local node regarding the set of nodes UP
 */
int vrt_node_get_upnode_id(void);

#endif /* __VRT_NODES_H__ */
