/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "csupd/src/sup_cluster.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

/**
 * Fill a given cluster. Helper function for tests below.
 *
 * \param cluster  Cluster to fill
 *
 * \return 1 if ok, 0 if not
 */
static int
__fill_cluster(sup_cluster_t *cluster)
{
  exa_nodeid_t node_id;
  unsigned node_count;
  bool ok = true;

  sup_cluster_init(cluster);
  node_count = 0;

  for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
    {
      sup_node_t *node;
      int r;

      r = sup_cluster_add_node(cluster, node_id);
      if (r < 0)
	{
	  fprintf(stderr, "failed adding node %u: %s\n", node_id, strerror(-r));
	  ok = false;
	  break;
	}

      node = sup_cluster_node(cluster, node_id);
      if (node == NULL)
	{
	  fprintf(stderr, "node %u doesn't seem to have been added\n", node_id);
	  ok = false;
	  break;
	}
      if (node->id != node_id)
	{
	  fprintf(stderr, "id of node %u should be %u, but is %u\n",
		  node_id, node_id, node->id);
	  ok = false;
	  break;
	}
      if (!exa_nodeset_contains(&cluster->known_nodes, node_id))
	{
	  fprintf(stderr, "cluster's known nodes does not contain %u\n", node_id);
	  ok = false;
	  break;
	}
      if (cluster->num_nodes != node_count + 1)
	{
	  fprintf(stderr, "number of nodes should be %u but is %u\n",
		  node_count + 1, cluster->num_nodes);
	  ok = false;
	  break;
	}

      node_count = cluster->num_nodes;
    }

  return cluster->num_nodes == EXA_MAX_NODES_NUMBER && ok;
}

/**
 * Add in the cluster as many nodes as supported
 */
ut_test(fill_cluster)
{
  sup_cluster_t cl;
  UT_ASSERT(__fill_cluster(&cl) == 1);
}

/**
 * Attemp to add a node to a full cluster
 */
ut_test(add_node_to_full_cluster)
{
  sup_cluster_t cl;

  __fill_cluster(&cl);
  UT_ASSERT(sup_cluster_add_node(&cl, 3) == -ENOSPC);
}

/**
 * Remove all nodes from the cluster
 */
ut_test(empty_cluster)
{
  sup_cluster_t cl;

  exa_nodeid_t node_id;

  __fill_cluster(&cl);
  for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
    {
      int r = sup_cluster_del_node(&cl, node_id);
      if (r < 0)
      {
	fprintf(stderr, "failed deleting node %u: %s\n", node_id, strerror(-r));
	break;
      }
    }

  UT_ASSERT(cl.num_nodes == 0 && exa_nodeset_is_empty(&cl.known_nodes));
}

/**
 * Attempt to remove a node from empty cluster
 */
ut_test(remove_node_from_empty_cluster)
{
  sup_cluster_t cl;

  sup_cluster_init(&cl);
  UT_ASSERT(sup_cluster_del_node(&cl, 3) == -ENOENT);
}

/**
 * Attempt to remove an unknown node from a cluster
 */
ut_test(del_unknown_node_from_cluster)
{
  sup_cluster_t cl;

  __fill_cluster(&cl);

  sup_cluster_del_node(&cl, 113);
  UT_ASSERT(sup_cluster_del_node(&cl, 113) == -ENOENT);
}

/**
 * Attempt to add a node already present in a cluster
 */

ut_test(add_existing_node)
{
  sup_cluster_t cl;

  sup_cluster_init(&cl);
  UT_ASSERT(sup_cluster_add_node(&cl, 10) == 0);

  UT_ASSERT(sup_cluster_add_node(&cl, 10) == -EEXIST);
}

/**
 * Attempt to add a node with an invalid id
 */
ut_test(add_node_with_invalid_id)
{
  sup_cluster_t cl;

  sup_cluster_init(&cl);
  UT_ASSERT(sup_cluster_add_node(&cl, EXA_MAX_NODES_NUMBER + 1) == -EINVAL);
}

/**
 * Attempt to get a (valid) node from an empty cluster
 */
ut_test(get_node_from_empty_cluster)
{
    sup_cluster_t cl;

    sup_cluster_init(&cl);
    UT_ASSERT(sup_cluster_node(&cl, 21) == NULL);
}
