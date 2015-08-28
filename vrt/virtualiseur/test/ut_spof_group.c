/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/virtualiseur/include/storage.h"
#include "vrt/assembly/fakes/empty_extent_definitions.h"
#include "vrt/virtualiseur/fakes/empty_realdev_definitions.h"
#include "vrt/virtualiseur/fakes/empty_storage_definitions.h"


UT_SECTION(spof_group_init)

ut_test(spof_group_init_gives_a_clean_spof_group)
{
    spof_group_t spof_group;
    spof_group_init(&spof_group);

    UT_ASSERT_EQUAL(SPOF_ID_NONE, spof_group.nb_realdevs);
    UT_ASSERT_EQUAL(0, spof_group.nb_realdevs);
}

UT_SECTION(spof_group_add_rdev)

ut_test(spof_group_add_rdev_with_space_left_returns_zero)
{
    spof_group_t spof_group;
    vrt_realdev_t rdev[NBMAX_DISKS_PER_SPOF_GROUP];
    int i;

    spof_group_init(&spof_group);

    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        UT_ASSERT_EQUAL(0, spof_group_add_rdev(&spof_group, &rdev[i]));

    spof_group_cleanup(&spof_group);
}

ut_test(spof_group_add_rdev_with_no_space_left_returns_ENOSPC)
{
    spof_group_t spof_group;
    vrt_realdev_t rdev[NBMAX_DISKS_PER_SPOF_GROUP + 1];
    int i;

    spof_group_init(&spof_group);

    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        spof_group_add_rdev(&spof_group, &rdev[i]);

    UT_ASSERT_EQUAL(-ENOSPC, spof_group_add_rdev(&spof_group, &rdev[i]));

    spof_group_cleanup(&spof_group);
}

ut_test(spof_group_add_all_rdevs_and_remove_them_in_the_same_order)
{
    spof_group_t spof_group;
    vrt_realdev_t rdev[NBMAX_DISKS_PER_SPOF_GROUP + 1];
    int i;

    spof_group_init(&spof_group);

    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        UT_ASSERT_EQUAL(0, spof_group_add_rdev(&spof_group, &rdev[i]));
    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        UT_ASSERT_EQUAL(0, spof_group_del_rdev(&spof_group, &rdev[i]));
    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        UT_ASSERT(spof_group.realdevs[i] == NULL);

    UT_ASSERT_EQUAL(0, spof_group.nb_realdevs);
    spof_group_cleanup(&spof_group);
}

ut_test(spof_group_add_all_rdevs_and_remove_them_in_the_reverse_order)
{
    spof_group_t spof_group;
    vrt_realdev_t rdev[NBMAX_DISKS_PER_SPOF_GROUP + 1];
    int i;

    spof_group_init(&spof_group);

    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        UT_ASSERT_EQUAL(0, spof_group_add_rdev(&spof_group, &rdev[i]));
    for (i = NBMAX_DISKS_PER_SPOF_GROUP - 1; i >= 0; i--)
        UT_ASSERT_EQUAL(0, spof_group_del_rdev(&spof_group, &rdev[i]));
    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        UT_ASSERT(spof_group.realdevs[i] == NULL);

    UT_ASSERT_EQUAL(0, spof_group.nb_realdevs);
    spof_group_cleanup(&spof_group);
}

ut_test(spof_group_del_rdev_returns_ENOENT_if_not_found)
{
    spof_group_t spof_group;
    vrt_realdev_t rdev[2];

    spof_group_init(&spof_group);

    spof_group_add_rdev(&spof_group, &rdev[0]);
    UT_ASSERT_EQUAL(-ENOENT, spof_group_del_rdev(&spof_group, &rdev[1]));

    spof_group_cleanup(&spof_group);
}

ut_test(spof_group_get_nodes_returns_nodes_in_spof_group)
{
    spof_group_t spof_group;
    vrt_realdev_t rdev[16];
    unsigned i;
    exa_nodeid_t node_id;
    exa_nodeset_t nodes;
    exa_nodeset_iter_t iter;

    spof_group_init(&spof_group);

    for (i = 0; i < 16; i++)
    {
        rdev[i].node_id = i;
        spof_group_add_rdev(&spof_group, &rdev[i]);
    }
    spof_group_get_nodes(&spof_group, &nodes);
    UT_ASSERT_EQUAL(16, exa_nodeset_count(&nodes));

    exa_nodeset_iter_init(&nodes, &iter);
    i = 0;
    while (exa_nodeset_iter(&iter, &node_id))
        UT_ASSERT_EQUAL(i++, node_id);

    spof_group_cleanup(&spof_group);
}

ut_test(spof_group_del_rdev_returns_zero_if_found)
{
    spof_group_t spof_group;
    vrt_realdev_t rdev;

    spof_group_init(&spof_group);

    spof_group_add_rdev(&spof_group, &rdev);
    UT_ASSERT_EQUAL(0, spof_group_del_rdev(&spof_group, &rdev));

    spof_group_cleanup(&spof_group);
}

ut_test(spof_group_add_max_possible_rdevs_then_remove_and_readd_works)
{
    spof_group_t spof_group;
    vrt_realdev_t rdev[NBMAX_DISKS_PER_SPOF_GROUP + 1];
    int i, j;

    spof_group_init(&spof_group);

    /* Add everything possible */
    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        spof_group_add_rdev(&spof_group, &rdev[i]);

    /* Now verify that they're OK */
    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++)
        UT_ASSERT(spof_group.realdevs[i] == &rdev[i]);

    /* Remove one out of two rdevs */
    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i += 2)
        UT_ASSERT_EQUAL(0, spof_group_del_rdev(&spof_group, &rdev[i]));

    /* Re-add them */
    for (i = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i += 2)
        UT_ASSERT_EQUAL(0, spof_group_add_rdev(&spof_group, &rdev[i]));

    /* Now verify that they're OK */
    for (i = 0, j = 1; i < NBMAX_DISKS_PER_SPOF_GROUP / 2; i++, j += 2)
        UT_ASSERT(spof_group.realdevs[i] == &rdev[j]);

    for (i = NBMAX_DISKS_PER_SPOF_GROUP / 2, j = 0; i < NBMAX_DISKS_PER_SPOF_GROUP; i++, j += 2)
        UT_ASSERT(spof_group.realdevs[i] == &rdev[j]);

    spof_group_cleanup(&spof_group);
}
