/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "csupd/test/sup_helpers.h"

#include "csupd/src/sup_clique.h"
#include "csupd/src/sup_cluster.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

ut_setup()
{
}

ut_cleanup()
{
}

/**
 * Read a mship file and check that the memberships calculated
 * (on all nodes) defined in the file matches the expected memberships.
 *
 * \param[in] filename  Name of mship file
 *
 * \return 0 if all calculated mships match expected ones,
 *         negative error otherwise
 */
static int
__check_mship_on_all_nodes(const char *filename)
{
  sup_cluster_t cluster;
  exa_nodeset_t *expected;
  exa_nodeset_t result;
  exa_nodeid_t node_id;

  UT_ASSERT(open_cluster(filename, &cluster, &expected) == 0);

  for (node_id = 0; node_id < cluster.num_nodes; node_id++)
    {
      cluster.self = sup_cluster_node(&cluster, node_id);
      sup_clique_compute(&cluster, &result);
      UT_ASSERT(exa_nodeset_equals(&result, &expected[node_id]));
    }

  free(expected);

  return 0;
}

/**
 * Calculate the common membership for an asymmetric bitfield array
 */
ut_test(asymmetric2_mship)
{
    __check_mship_on_all_nodes("asymmetric2_mship.txt");
}

/**
 * Calculate the common membership for an asymmetric bitfield array
 */
ut_test(asymmetric_mship)
{
    __check_mship_on_all_nodes("asymmetric_mship.txt");
}

/**
 * Calculate the common membership for a specific test found on wikipedia
 */
ut_test(wiki_8nodes_mship)
{
    __check_mship_on_all_nodes("wikipedia_8nodes_mship.txt");
}

/**
 * Calculate the common membership when the best one is
 * about half the nodes.
 */
ut_test(half_mship)
{
    __check_mship_on_all_nodes("half_mship.txt");
}

/**
 * Calculate the common membership when nodes have two distinct,
 * interleaved views.
 */
ut_test(interleaved_mship)
{
    __check_mship_on_all_nodes("interleaved_mship.txt");
}

/**
 * Calculate the common membership when all nodes have a full view
 * of the cluster (ie, each node sees all nodes).
 */
ut_test(full_mship)
{
    __check_mship_on_all_nodes("full_mship.txt");
}

ut_test(bug_2996)
{
    __check_mship_on_all_nodes("bug_2996.txt");
}
