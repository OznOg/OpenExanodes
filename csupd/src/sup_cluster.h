/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SUP_CLUSTER_H__
#define __SUP_CLUSTER_H__

#include "sup_view.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"

/** Information on a node / Csupd instance */
typedef struct sup_node
{
  exa_nodeid_t id;             /**< Node id */
  unsigned short incarnation;  /**< Node incarnation */
  sup_view_t view;             /**< Node's view */
  int last_seen;               /**< Last time seen, in seconds */
} sup_node_t;

/** Tell whether a node is defined */
#define sup_node_defined(sup_node)  \
  ((sup_node)->id < EXA_MAX_NODES_NUMBER && (sup_node)->id != EXA_NODEID_NONE)

void sup_node_init(sup_node_t *node);

/** Description of a cluster */
typedef struct sup_cluster
{
  sup_node_t nodes[EXA_MAX_NODES_NUMBER];  /**< Nodes */
  exa_nodeset_t known_nodes;               /**< Nodeset containing all known nodes */
  unsigned num_nodes;                      /**< Actual number of nodes */
  sup_node_t *self;                        /**< Points to our node */
} sup_cluster_t;

void sup_cluster_init(sup_cluster_t *cluster);

sup_node_t *sup_cluster_node(const sup_cluster_t *cluster, exa_nodeid_t node_id);

int sup_cluster_add_node(sup_cluster_t *cluster, exa_nodeid_t node_id);
int sup_cluster_del_node(sup_cluster_t *cluster, exa_nodeid_t node_id);

#endif
