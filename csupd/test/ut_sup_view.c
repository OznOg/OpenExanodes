/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "csupd/src/sup_view.h"

/*
 * NOTES:
 *
 * 1. Can't do coord- and clique-related tests, since there is nothing
 * about these in sup_view's API (the same goes for accepted, committed).
 *
 * 2. Keep in mind that sup_view_t.num_seen is the number of "significant
 *    node ids" (significant bits) in a view (nodeset), *not* the number of
 *    nodes!
 */

/**
 * Check that all valid states have a human-readable name (and that non-valid
 * states don't).
 */
ut_test(all_states_have_a_name)
{
    sup_state_t state;

    for (state = SUP_STATE_UNKNOWN; state <= SUP_STATE_COMMIT; state++)
	UT_ASSERT(sup_state_name(state) != NULL);

    /* Ideally, we should check all values outside the valid range */
    UT_ASSERT(sup_state_name(SUP_STATE_UNKNOWN - 1) == NULL);
    UT_ASSERT(sup_state_name(SUP_STATE_COMMIT + 1) == NULL);
}

/**
 * Check that an initialized view is all "zero" (i.e., that each field is
 * set to the zero of its type).
 */
ut_test(init_view)
{
    sup_view_t view;

    sup_view_init(&view);

    UT_ASSERT(view.state == SUP_STATE_UNKNOWN);
    UT_ASSERT(exa_nodeset_is_empty(&view.nodes_seen) && view.num_seen == 0);
    UT_ASSERT(exa_nodeset_is_empty(&view.clique));
    UT_ASSERT(view.coord == EXA_NODEID_NONE);
    UT_ASSERT(view.accepted == 0 && view.committed == 0);
}

/**
 * Check that the lowest valid node id (0) can be added to a view.
 */
ut_test(add_lowest_valid)
{
    sup_view_t view;

    sup_view_init(&view);
    sup_view_add_node(&view, 0);

    UT_ASSERT(view.num_seen == 1
              && exa_nodeset_contains(&view.nodes_seen, 0)
              && exa_nodeset_count(&view.nodes_seen) == 1);
}

/**
 * Check that the highest valid node id (max nodes number - 1) can be added
 * to a view.
 */
ut_test(add_highest_valid)
{
    sup_view_t view;

    sup_view_init(&view);
    sup_view_add_node(&view, EXA_MAX_NODES_NUMBER - 1);

    UT_ASSERT(view.num_seen == EXA_MAX_NODES_NUMBER
              && exa_nodeset_contains(&view.nodes_seen, EXA_MAX_NODES_NUMBER - 1)
              && exa_nodeset_count(&view.nodes_seen) == 1);
}

/**
 * Check that the lowest valid node id (0) can be deleted.
 */
ut_test(del_lowest_valid)
{
    sup_view_t view;

    sup_view_init(&view);
    sup_view_add_node(&view, 0);

    sup_view_del_node(&view, 0);

    UT_ASSERT(view.num_seen == 0
              && exa_nodeset_equals(&view.nodes_seen, &EXA_NODESET_EMPTY));
}

/**
 * Check that the highest valid node id (max nodes number - 1) can be
 * deleted.
 */
ut_test(test_del_highest_valid)
{
    sup_view_t view;

    sup_view_init(&view);
    sup_view_add_node(&view, EXA_MAX_NODES_NUMBER - 1);

    sup_view_del_node(&view, EXA_MAX_NODES_NUMBER - 1);

    UT_ASSERT(view.num_seen == 0
              && exa_nodeset_equals(&view.nodes_seen, &EXA_NODESET_EMPTY));
}

/**
 * Check that, when deleting from a view a node in-between other nodes seen
 * by the view, the resulting view's num_seen field is left unchanged
 * (since the node with the highest id is untouched).
 */
ut_test(del_inbetween)
{
    sup_view_t view;
    unsigned num_seen_before;

    sup_view_init(&view);
    sup_view_add_node(&view, 5);
    sup_view_add_node(&view, 13);
    sup_view_add_node(&view, 51);

    num_seen_before = view.num_seen;

    sup_view_del_node(&view, 13);

    UT_ASSERT(view.num_seen == num_seen_before
              && !exa_nodeset_contains(&view.nodes_seen, 13));
}

/**
 * Check that, when deleting the node whose id is the highest seen by the
 * view, the resulting view's num_seen field is updated properly (i.e., is
 * set to the new highest seen node + 1).
 */
ut_test(del_max)
{
    sup_view_t view;

    sup_view_init(&view);
    sup_view_add_node(&view, 5);
    sup_view_add_node(&view, 13);
    sup_view_add_node(&view, 51);

    sup_view_del_node(&view, 51);

    UT_ASSERT(view.num_seen = 13 + 1
              && !exa_nodeset_contains(&view.nodes_seen, 51));
}

/* helper */
static void
__fill_view(sup_view_t *view)
{
    exa_nodeid_t node;

    sup_view_init(view);
    for (node = 0; node < EXA_MAX_NODES_NUMBER; node++)
        sup_view_add_node(view, node);
}

/**
 * Add all valid node ids to a view and check that the view built is
 * indeed a full membership.
 */
ut_test(add_all_nodes)
{
    sup_view_t view;

    __fill_view(&view);
    UT_ASSERT(view.num_seen == EXA_MAX_NODES_NUMBER
              && exa_nodeset_equals(&view.nodes_seen, &EXA_NODESET_FULL));
}

/**
 * Add all valid node ids to a view, starting from the highest valid node
 * id and going down to 0, checking num_seen as we go.
 */
ut_test(add_all_nodes_reversed)
{
    sup_view_t view;
    exa_nodeid_t node;

    sup_view_init(&view);

    node = EXA_MAX_NODES_NUMBER - 1;
    while (true)
    {
	sup_view_add_node(&view, node);
	/* Since we start with the highest node, the number of significant
	 * ids is constant */
	UT_ASSERT(view.num_seen == EXA_MAX_NODES_NUMBER
                  && exa_nodeset_contains(&view.nodes_seen, node));

	if (node == 0)
	    break;

	node--;
    }
}

/**
 * Delete all nodes from a full view and check that the resulting view is
 * empty.
 */
ut_test(del_all_nodes)
{
    sup_view_t view;
    exa_nodeid_t node;

    __fill_view(&view);
    for (node = 0; node < EXA_MAX_NODES_NUMBER; node++)
	sup_view_del_node(&view, node);

    UT_ASSERT(view.num_seen == 0
              && exa_nodeset_equals(&view.nodes_seen, &EXA_NODESET_EMPTY));
}

/**
 * Delete all nodes from a full view, starting from the highest valid node
 * id and going down to 0, checking num_seen as we go.
 */
ut_test(del_all_nodes_reversed)
{
    sup_view_t view;
    exa_nodeid_t node;

    __fill_view(&view);

    node = EXA_MAX_NODES_NUMBER - 1;

    do {
	sup_view_del_node(&view, node);
	/* num_seen is decreased by one each time a node is removed (as
	 * that node was the highest one seen) */
	UT_ASSERT(view.num_seen == node
                  && !exa_nodeset_contains(&view.nodes_seen, node));

	if (node > 0)
	    node--;

    } while (node > 0);
}

ut_test(view_equals_itself)
{
    sup_view_t view;

    sup_view_init(&view);
    sup_view_add_node(&view, 17);
    sup_view_add_node(&view, 33);
    sup_view_add_node(&view, 64);
    sup_view_add_node(&view, EXA_MAX_NODES_NUMBER - 1);

    UT_ASSERT(sup_view_equals(&view, &view));
}

ut_test(view_does_not_equal_other)
{
    sup_view_t v1, v2;

    sup_view_init(&v1);
    sup_view_add_node(&v1, 1);
    sup_view_add_node(&v1, 16);
    sup_view_add_node(&v1, 92);

    sup_view_init(&v2);
    sup_view_add_node(&v2, 1);
    sup_view_add_node(&v2, 92);
    sup_view_add_node(&v2, 101);

    UT_ASSERT(!sup_view_equals(&v1, &v2));
}

/**
 * Check the copy of a view is equal to said view.
 */
ut_test(view_copy)
{
    sup_view_t v1, v2;

    sup_view_init(&v1);
    sup_view_copy(&v2, &v1);

    UT_ASSERT(sup_view_equals(&v2, &v1));
}

/**
 * The sup_view_debug() function is just meant for debugging, as its name
 * says. However, for the sake of full coverage, let's do a dumb test.
 */
ut_test(sup_view_debug)
{
    sup_view_t view;

    sup_view_debug(NULL);

    sup_view_init(&view);
    sup_view_debug(&view);
}
