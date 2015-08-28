/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "sup_cluster.h"

#include "os/include/strlcpy.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

/**
 * Initialize a node.
 *
 * \param node  Node
 */
void
sup_node_init(sup_node_t *node)
{
  EXA_ASSERT(node);

  node->id = EXA_NODEID_NONE;
  node->incarnation = 0;

  sup_view_init(&node->view);
  node->last_seen = 0;
}

/**
 * Get the node within a cluster given its id.
 *
 * \param     cluster  Cluster to get the node from
 * \param[in] node_id  Id of node to get
 *
 * \return Node if found, NULL otherwise
 */
sup_node_t *
sup_cluster_node(const sup_cluster_t *cluster, exa_nodeid_t node_id)
{
  EXA_ASSERT(cluster);
  EXA_ASSERT(node_id < EXA_MAX_NODES_NUMBER);

  if (cluster->nodes[node_id].id == EXA_NODEID_NONE)
    return NULL;

  return (sup_node_t *)&cluster->nodes[node_id];
}

/**
 * Initialize a cluster.
 *
 * \param cluster  Cluster
 */
void
sup_cluster_init(sup_cluster_t *cluster)
{
  exa_nodeid_t id;

  EXA_ASSERT(cluster);

  for (id = 0; id < EXA_MAX_NODES_NUMBER; id++)
    sup_node_init(&cluster->nodes[id]);

  exa_nodeset_reset(&cluster->known_nodes);

  cluster->num_nodes = 0;

  cluster->self = NULL;
}

/**
 * Add a node to a cluster.
 *
 * \param cluster        Cluster to add the node to
 * \param[in] node_id    Id of node to add
 *
 * \return 0 if added, negative error code otherwise:
 *     -EINVAL if node id invalid
 *     -ENOSPC if cluster full
 *     -EEXIST if node already present in cluster
 */
int
sup_cluster_add_node(sup_cluster_t *cluster, exa_nodeid_t node_id)
{
  sup_node_t *node;

  EXA_ASSERT(cluster);

  if (node_id >= EXA_MAX_NODES_NUMBER)
    return -EINVAL;

  if (cluster->num_nodes == EXA_MAX_NODES_NUMBER)
    return -ENOSPC;

  node = &cluster->nodes[node_id];
  if (sup_node_defined(node))
    return -EEXIST;

  sup_node_init(node);

  node->id = node_id;
  exa_nodeset_add(&cluster->known_nodes, node_id);

  cluster->num_nodes++;

  return 0;
}

/**
 * Remove a node from a cluster.
 *
 * \param     cluster  Cluster to remove the node from
 * \param[in] node_id  Id of node to remove
 *
 * \return 0 if removed, -ENOENT otherwise
 */
int
sup_cluster_del_node(sup_cluster_t *cluster, exa_nodeid_t node_id)
{
  sup_node_t *node;

  EXA_ASSERT(cluster);
  EXA_ASSERT(node_id < EXA_MAX_NODES_NUMBER);

  if (cluster->num_nodes == 0)
    return -ENOENT;

  node = &cluster->nodes[node_id];
  if (!sup_node_defined(node))
    return -ENOENT;

  sup_node_init(node);

  exa_nodeset_del(&cluster->known_nodes, node_id);

  cluster->num_nodes--;

  return 0;
}
