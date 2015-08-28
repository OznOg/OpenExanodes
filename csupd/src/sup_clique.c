/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "sup_cluster.h"
#include "sup_clique.h"

#include "common/include/exa_mkstr.h"
#include "common/include/exa_nodeset.h"

#include <stdlib.h>

/* For debugging purposes */
#include <stdio.h>
#include <stdarg.h>

/** Whether the membership calculation is verbose */
static bool __verbose = false;

/**
 * Print a membership in binary form.
 *
 * \param[in] name       Name of nodeset
 * \param[in] nodeset    Nodeset to print
 * \param[in] num_nodes  Number of nodes in the nodeset
 */
static void
__print_mship(const char *name, const exa_nodeset_t *mship,
	      unsigned num_nodes)
{
  exa_nodeid_t i = num_nodes - 1;

  printf("%-20s: ", name);
  while (1)
    {
      printf("%c", exa_nodeset_contains(mship, i) ? '1' : '0');
      if (i == 0)
	break;

      i--;
    }
  printf("\n");
}

#define __DEBUG_MSHIP(mship, num_nodes)				   \
  do {								   \
    if (__verbose)						   \
      __print_mship(exa_mkstr(mship), (mship), (num_nodes));	   \
  } while (0)

static void
__debug_printf(const char *fmt, ...)
{
  va_list al;

  if (!__verbose)
    return;

  va_start(al, fmt);
  vprintf(fmt, al);
  va_end(al);
}

/*
 * Originaly node_mship_cmp() was implemented as a nested function
 * inside sup_clique_compute in order to keep an access on the cluster
 * variable (the qsort api does not permit to pass some private data),
 * but the intel compiler does not implement nested functions, so
 * this 'global' is here to workaround the problem.
 * This variable MUST NOT be used for any other purpose thatn the qsort.
 */
const sup_cluster_t *qsort_cluster = NULL;

/*
 * Compare two nodes based on the size of their memberships.
 * A node with a larger membership is after than a node with a
 * smaller membership.
 * In case, both nodes see the same number of nodes, the node whose ID
 * is smallest is considered as inferior.
 */
static int node_mship_cmp(const void *a, const void *b)
{
    sup_node_t *node_a, *node_b;
    int diff_count;

    EXA_ASSERT(a && b);

    node_a = sup_cluster_node(qsort_cluster, *(exa_nodeid_t *)a);
    node_b = sup_cluster_node(qsort_cluster, *(exa_nodeid_t *)b);

    diff_count = exa_nodeset_count(&node_a->view.nodes_seen)
                 - exa_nodeset_count(&node_b->view.nodes_seen);

    if (diff_count)
        return diff_count;

    return *(exa_nodeid_t *)a - *(exa_nodeid_t *)b;
}

/**
 * \brief Compute the clique given the view of the cluster by each node
 *
 * This function is based on the Welsh & Powell algorithm that finds
 * cliques in a non oriented graph.
 *
 * The version implemented here is specific to our needs, ie it just tries
 * to find the clique in which the local node is.
 *
 * The algorithm is the following :
 *
 * 1) Sort the nodes by the number of nodes they can reach (min -> max order).
 *
 *    NOTE : The Welsh & Powell algorithm sorts nodes in the decreasing order,
 *    but in our case the algorithm is applied to *dead links*, not on working
 *    links. Thus sorting by good links in increasing order is actually
 *    equivalent to sorting by dead links in decreasing order.
 *    This is just a trick to limit the computations in this function).
 *
 * 2) Take the first node and try to integrate the following node into its
 *    membership (a node is accepted *iff* it can reach all the previously
 *    integrated nodes that are already in the list) ; if a node cannot enter
 *    the list, it is ignored and then we try with the next one.
 *
 * 3) When all nodes have been tested, we check if our node is in the clique
 *    found. If that is the case, we return the result, otherwise we remove
 *    the node of the clique from the node list and we restart from phase 2.
 *
 * NOTE : This function cannot guarantee that the membership found is
 *        the best (biggest).
 *
 * param[in]  cluster  Cluster
 * param[out] result   Resulting membership
 */
void
sup_clique_compute(const sup_cluster_t *cluster, exa_nodeset_t *result)
{
  int i, sort_idx;
  exa_nodeid_t sorted_nodes[cluster->num_nodes];
  exa_nodeset_t rem_nodes; /* remaining nodes */
  exa_nodeset_t best;

#define __debug_mship(mship) __DEBUG_MSHIP(mship, cluster->num_nodes)

  exa_nodeset_reset(&best);

  /* Sort nodes by decreasing membership size */
  sort_idx = 0;
  exa_nodeset_foreach(&cluster->known_nodes, i)
    {
      sup_node_t *node = sup_cluster_node(cluster, i);
      if (sup_node_defined(node))
	sorted_nodes[sort_idx++] = node->id;
    }

  /* See above comment for details on qsort_cluster */
  qsort_cluster = cluster;

  qsort(sorted_nodes, cluster->num_nodes, sizeof(exa_nodeid_t), node_mship_cmp);

  qsort_cluster = NULL;

  if (__verbose)
    {
      __debug_printf("sorted nodes:");
      for (i = 0; i < cluster->num_nodes; i++)
	__debug_printf(" %u", sorted_nodes[i]);
      __debug_printf("\n");
    }

  exa_nodeset_copy(&rem_nodes, &cluster->known_nodes);
  while (exa_nodeset_count(&rem_nodes) > 0)
    {
      /* list of nodes that CANNOT be part of the current membership */
      exa_nodeset_t impossible;

      exa_nodeset_reset(&impossible);
      exa_nodeset_reset(result);

      /* go thru the sorted list */
      for (i = 0; i < cluster->num_nodes; i++)
	{
	  sup_node_t *node;
	  exa_nodeset_t tmp, intersec;
	  exa_nodeset_t unseen;

	  __debug_printf("-- node %u\n", sorted_nodes[i]);

	  /* Skip the node if it was already used placed in a membership or if
	   * it is in the impossible list of the current membership */
	  if (!exa_nodeset_contains(&rem_nodes, sorted_nodes[i]) ||
	      exa_nodeset_contains(&impossible, sorted_nodes[i]))
	    {
	      __debug_printf("skipped\n");
	      continue;
	    }

	  __debug_mship(&rem_nodes);

	  node = sup_cluster_node(cluster, sorted_nodes[i]);

	  exa_nodeset_copy(&tmp, result);
	  exa_nodeset_add(&tmp, node->id);
	  __debug_mship(&tmp);

	  /* Calculate mship of nodes unseen by current node */
	  exa_nodeset_copy(&unseen, &cluster->known_nodes);
	  exa_nodeset_substract(&unseen, &node->view.nodes_seen);
	  __debug_mship(&unseen);

          /* The intersection tells if the current node doesn't see a node
	   * that is in the membership being calculated */
	  exa_nodeset_copy(&intersec, &unseen);
	  exa_nodeset_intersect(&intersec, &tmp);
	  __debug_mship(&intersec);

	  /* If the intersection is empty, it means the current node
	   * sees all nodes in the calculated membership, so it is
	   * added to that membership */
	  if (exa_nodeset_is_empty(&intersec))
	    {
	      exa_nodeset_add(result, node->id);
	      exa_nodeset_sum(&impossible, &unseen);
	      if (__verbose)
		{
		  __debug_printf("added\n");
		  __debug_mship(result);
		}
	    }
	  else
	    __debug_printf("rejected\n");
	}

      if (exa_nodeset_count(result) > exa_nodeset_count(&best))
	exa_nodeset_copy(&best, result);

      __debug_mship(&best);

      /* The local node is in the membership found so we can return */
      if (exa_nodeset_contains(result, cluster->self->id))
	return;

      /* The local node is not part of the clique, remove those nodes from list */
      exa_nodeset_substract(&rem_nodes, result);
    }

  /* Being here means the local node is not in any of the memberships
   * calculated, which is obviously impossible */
  EXA_ASSERT(false);
}
