/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>

#include <unit_testing.h>

#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"
#include "os/include/os_mem.h"
#include "os/include/os_inttypes.h"
#include "admind/src/adm_group.h"
#include "admind/services/vrt/vrt_layout.h"
#include "vrt/common/include/spof.h"

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

UT_SECTION(replication_rule)
static struct adm_group *group;
ut_setup()
{
    group = os_malloc(sizeof(struct adm_group));
}

ut_cleanup()
{
    os_free(group);
    group = NULL;
}

/**
 * Contains the configuration of a test.
 */
struct test_config
{
    uint32_t spof_groups[SPOFS_MAX];
    uint32_t nb_spof_groups;
    uint32_t nb_spare;
    uint32_t slot_width;
};

ut_test(replication_rule_satisfied)
{
    {
        struct test_config config =
        {
            .spof_groups    = {1, 1, 1},
            .nb_spof_groups = 3,
            .nb_spare       = 0,
            .slot_width     = 3
        };
        cl_error_desc_t err;
        UT_ASSERT(rainX_rule_replication_satisfied(config.slot_width,
                                                   config.nb_spare,
                                                   &err));
    }
    {
        struct test_config config =
        {
            .spof_groups    = {1, 1, 2},
            .nb_spof_groups = 3,
            .nb_spare       = 0,
            .slot_width     = 3
        };
        cl_error_desc_t err;
        UT_ASSERT(rainX_rule_replication_satisfied(config.slot_width,
                                                   config.nb_spare,
                                                   &err));
    }
    {
        struct test_config config =
        {
            .spof_groups    = {1, 1, 1},
            .nb_spof_groups = 3,
            .nb_spare       = 0,
            .slot_width     = 2
        };
        cl_error_desc_t err;
        UT_ASSERT(rainX_rule_replication_satisfied(config.slot_width,
                                                   config.nb_spare,
                                                   &err));
    }
    /* Suggested during a discussion with Michael and Mathieu */
    {
        struct test_config config =
        {
            .spof_groups    = {1, 1, 1, 1, 1, 1, 1},
            .nb_spof_groups = 7,
            .nb_spare       = 2,
            .slot_width     = 4
        };
        cl_error_desc_t err;
        UT_ASSERT(rainX_rule_replication_satisfied(config.slot_width,
                                                   config.nb_spare,
                                                   &err));
    }
    {
        struct test_config config =
        {
            .spof_groups    = {1, 1, 1, 1, 1, 1, 1, 1, 1},
            .nb_spof_groups = 9,
            .nb_spare       = 3,
            .slot_width     = 5
        };
        cl_error_desc_t err;
        UT_ASSERT(rainX_rule_replication_satisfied(config.slot_width,
                                                   config.nb_spare,
                                                   &err));
    }
}

ut_test(replication_rule_infringed)
{
    {
        struct test_config config =
        {
            .spof_groups    = {1},
            .nb_spof_groups = 1,
            .nb_spare       = 0,
            .slot_width     = 1
        };
        cl_error_desc_t err;
        UT_ASSERT(!rainX_rule_replication_satisfied(config.slot_width,
                                                    config.nb_spare,
                                                    &err));
    }
    {
        struct test_config config =
        {
            .spof_groups    = {1, 1, 1},
            .nb_spof_groups = 3,
            .nb_spare       = 0,
            .slot_width     = 1  /* Minimum slot width is 2 */
        };
        cl_error_desc_t err;
        UT_ASSERT(!rainX_rule_replication_satisfied(config.slot_width,
                                                    config.nb_spare,
                                                    &err));
    }
    {
        struct test_config config =
        {
            .spof_groups    = {1, 1, 1, 1, 1},
            .nb_spof_groups = 5,
            .nb_spare       = 1,
            .slot_width     = 2  /* Minimum slot width is 3 (i.e. 2 + 1) */
        };
        cl_error_desc_t err;
        UT_ASSERT(!rainX_rule_replication_satisfied(config.slot_width,
                                                    config.nb_spare,
                                                    &err));
    }
    /* Suggested during a discussion with Michael and Mathieu */
    {
        struct test_config config =
        {
            .spof_groups    = {1, 1, 1, 1, 1, 1, 1},
            .nb_spof_groups = 7,
            .nb_spare       = 2,
            .slot_width     = 2  /* Minimum slot width is 4 (i.e. 2 + 2) */
        };
        cl_error_desc_t err;
        UT_ASSERT(!rainX_rule_replication_satisfied(config.slot_width,
                                                    config.nb_spare,
                                                    &err));
    }
    {
        struct test_config config =
        {
            .spof_groups    = {1, 1, 1, 1, 1, 1, 1},
            .nb_spof_groups = 7,
            .nb_spare       = 2,
            .slot_width     = 3  /* Minimum slot width is 4 (i.e 2 + 2) */
        };
        cl_error_desc_t err;
        UT_ASSERT(!rainX_rule_replication_satisfied(config.slot_width,
                                                    config.nb_spare,
                                                    &err));
    }
}

UT_SECTION(administrable_rule)

/**
 * Helper function to set up spofs with given number of nodes.
 *
 * All spofs are involved: there is no point in the test cases below to
 * define spofs besides those involved.
 *
 * @param[out] spofs              Array of SPOFs to setup (num_spofs entries)
 * @param[out] involved_spof_ids  Ids of SPOFs involved (num_spofs entries)
 * @param[in]  num_spofs          Number of SPOFs in array
 * @param[in]  nodes_per_spof     Array of per-SPOF number of nodes (num_spof entries)
 */
static void setup_spofs(exa_nodeset_t *spofs, spof_id_t *involved_spof_ids,
                        unsigned num_spofs, const unsigned *nodes_per_spof)
{
    exa_nodeid_t node_id;
    unsigned i, n;

    node_id = 0;
    for (i = 0; i < num_spofs; i++)
    {
        exa_nodeset_reset(&spofs[i + 1]);
        for (n = 0; n < nodes_per_spof[i]; n++)
            exa_nodeset_add(&spofs[i + 1], node_id++);

        involved_spof_ids[i] = i + 1;
    }
}

ut_test(administrable_rule_with_minimum_number_of_spofs_required_is_satisfied)
{
    unsigned int num_spares;
    cl_error_desc_t err;

    for (num_spares = 0; num_spares <= NBMAX_SPARES_PER_GROUP; num_spares++)
    {
        unsigned num_spofs = num_spares * 2 + 2;
        exa_nodeset_t spofs[SPOFS_MAX];
        spof_id_t involved_spof_ids[num_spofs];
        unsigned nodes_per_spof[num_spofs];
        int i;

        for (i = 0; i < num_spofs; i++)
            nodes_per_spof[i] = 1;

        setup_spofs(spofs, involved_spof_ids, num_spofs, nodes_per_spof);

        UT_ASSERT(rainX_rule_administrability_satisfied(involved_spof_ids, num_spofs,
                                                        spofs, SPOFS_MAX, num_spares,
                                                        &err));
    }
}

ut_test(administrable_rule_with_one_more_spof_than_minimum_required_is_satisfied)
{
    unsigned int num_spares;
    cl_error_desc_t err;

    for (num_spares = 0; num_spares <= NBMAX_SPARES_PER_GROUP; num_spares++)
    {
        unsigned min_required_spofs = num_spares * 2 + 2;
        /* One more SPOF than the minimum required for the given number of spares */
        unsigned num_spofs = min_required_spofs + 1;
        exa_nodeset_t spofs[SPOFS_MAX];
        spof_id_t involved_spof_ids[num_spofs];
        unsigned nodes_per_spof[num_spofs];
        int i;

        for (i = 0; i < num_spofs; i++)
            nodes_per_spof[i] = 1;

        setup_spofs(spofs, involved_spof_ids, num_spofs, nodes_per_spof);

        UT_ASSERT(rainX_rule_administrability_satisfied(involved_spof_ids, num_spofs,
                                                        spofs, SPOFS_MAX, num_spares,
                                                        &err));
    }
}

ut_test(administrable_rule_with_one_less_spof_than_minimum_required_is_infringed)
{
    unsigned int num_spares;
    cl_error_desc_t err;

    for (num_spares = 0; num_spares <= NBMAX_SPARES_PER_GROUP; num_spares++)
    {
        unsigned min_required_spofs = num_spares * 2 + 2;
        /* One less SPOF than the minimum required for the given number of spares */
        unsigned num_spofs = min_required_spofs - 1;
        exa_nodeset_t spofs[SPOFS_MAX];
        spof_id_t involved_spof_ids[num_spofs];
        unsigned nodes_per_spof[num_spofs];
        int i;

        for (i = 0; i < num_spofs; i++)
            nodes_per_spof[i] = 1;

        setup_spofs(spofs, involved_spof_ids, num_spofs, nodes_per_spof);

        UT_ASSERT(!rainX_rule_administrability_satisfied(involved_spof_ids, num_spofs,
                                                         spofs, SPOFS_MAX, num_spares,
                                                         &err));
    }
}

ut_test(administrable_rule_different_spof_sizes_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[7] = { 2, 3, 1, 1, 1, 1, 1 };
    spof_id_t involved_spof_ids[7];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 7, nodes_per_spof);

    UT_ASSERT(!rainX_rule_administrability_satisfied(involved_spof_ids, 7,
                                                     spofs, SPOFS_MAX, 2, &err));
    ut_printf("err.msg = '%s'", err.msg);
}

/* User story 9 */
ut_test(administrable_rule_2_spofs_3_4_nodes_0_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[2] = { 3, 4 };
    spof_id_t involved_spof_ids[2];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 2, nodes_per_spof);

    UT_ASSERT(!rainX_rule_administrability_satisfied(involved_spof_ids, 2,
                                                     spofs, SPOFS_MAX, 0, &err));

    ut_printf("err.msg = '%s'", err.msg);
}

ut_test(administrable_rule_user_story_7)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[3] = { 2, 2, 3 };
    spof_id_t involved_spof_ids[3];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 3, nodes_per_spof);

    UT_ASSERT(rainX_rule_administrability_satisfied(involved_spof_ids, 3,
                                                    spofs, SPOFS_MAX, 0, &err));
}

UT_SECTION(quorum_rule)

ut_test(node_quorum_rule_3_spofs_1_1_1_nodes_0_spare_satisfied)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[3] = { 1, 1, 1 };
    spof_id_t involved_spof_ids[3];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 3, nodes_per_spof);
    UT_ASSERT(rainX_rule_quorum_satisfied(involved_spof_ids, 3, spofs, SPOFS_MAX, 0,
                                          3, &err));
}

ut_test(node_quorum_rule_4_spofs_1_1_1_1_nodes_0_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[4] = { 1, 1, 1, 1 };
    spof_id_t involved_spof_ids[4];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 4, nodes_per_spof);
    UT_ASSERT(rainX_rule_quorum_satisfied(involved_spof_ids, 4, spofs, SPOFS_MAX, 0,
                                          4, &err));
}

ut_test(node_quorum_rule_2_spofs_1_1_nodes_0_spare_satisfied)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[2] = { 1, 1 };
    spof_id_t involved_spof_ids[2];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 2, nodes_per_spof);
    UT_ASSERT(rainX_rule_quorum_satisfied(involved_spof_ids, 2, spofs, SPOFS_MAX, 0,
                                          2, &err));
}

ut_test(node_quorum_rule_2_spofs_1_1_nodes_1_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[2] = { 1, 1 };
    spof_id_t involved_spof_ids[2];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 2, nodes_per_spof);
    UT_ASSERT(!rainX_rule_quorum_satisfied(involved_spof_ids, 2, spofs, SPOFS_MAX, 1,
                                           2, &err));
}

/* [node1] [node2 node3] [node4 node5 node6 node7] */
ut_test(node_quorum_rule_3_spofs_1_2_4_nodes_0_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[3] = { 1, 2, 4 };
    spof_id_t involved_spof_ids[3];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 3, nodes_per_spof);
    UT_ASSERT(!rainX_rule_quorum_satisfied(involved_spof_ids, 3, spofs, SPOFS_MAX, 0,
                                           7, &err));
    ut_printf("err.msg = '%s'", err.msg);
}

UT_SECTION(multi_node_spof_groups_user_stories)

/* Part of user story 3 */
ut_test(node_quorum_rule_2_spofs_4_4_nodes_0_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[2] = { 4, 4 };
    spof_id_t involved_spof_ids[2];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 2, nodes_per_spof);
    UT_ASSERT(!rainX_rule_quorum_satisfied(involved_spof_ids, 2, spofs, SPOFS_MAX, 0,
                                           8, &err));
}

/* Part of user story 4 */
ut_test(node_quorum_rule_3_spofs_2_3_3_nodes_0_spare_satisfied)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[3] = { 2, 3, 3 };
    spof_id_t involved_spof_ids[3];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 3, nodes_per_spof);
    UT_ASSERT(rainX_rule_quorum_satisfied(involved_spof_ids, 3, spofs, SPOFS_MAX, 0,
                                           8, &err));
}

ut_test(node_quorum_rule_3_spofs_2_3_3_nodes_1_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[3] = { 2, 3, 3 };
    spof_id_t involved_spof_ids[3];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 3, nodes_per_spof);
    UT_ASSERT(!rainX_rule_quorum_satisfied(involved_spof_ids, 3, spofs, SPOFS_MAX, 1,
                                           8, &err));
}

/* User story 5 */

ut_test(node_quorum_rule_5_spofs_2_2_2_1_1_nodes_1_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[5] = { 2, 2, 2, 1, 1 };
    spof_id_t involved_spof_ids[5];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 5, nodes_per_spof);
    UT_ASSERT(!rainX_rule_quorum_satisfied(involved_spof_ids, 5, spofs, SPOFS_MAX, 1,
                                           8, &err));
}

ut_test(node_quorum_rule_6_spofs_2_2_1_1_1_1_nodes_1_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[6] = { 2, 2, 1, 1, 1, 1 };
    spof_id_t involved_spof_ids[6];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 6, nodes_per_spof);
    UT_ASSERT(!rainX_rule_quorum_satisfied(involved_spof_ids, 6, spofs, SPOFS_MAX, 1,
                                           8, &err));
}

ut_test(node_quorum_rule_7_spofs_2_1_1_1_1_1_1_nodes_1_spare_satisfied)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[7] = { 2, 1, 1, 1, 1, 1, 1 };
    spof_id_t involved_spof_ids[7];
    cl_error_desc_t
             err;

    setup_spofs(spofs, involved_spof_ids, 7, nodes_per_spof);

    UT_ASSERT(rainX_rule_quorum_satisfied(involved_spof_ids, 7, spofs, SPOFS_MAX, 1,
                                           8, &err));
}

/* User story 6 */

ut_test(node_administrable_rule_2_spofs_5_3_nodes_with_2_1_participating_0_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    /* unsigned nodes_per_spof[2] = { 5, 3 }; */
    unsigned nodes_with_storage_per_spof[2] = { 2, 1 };
    spof_id_t involved_spof_ids[2];
    cl_error_desc_t
             err;

    setup_spofs(spofs, involved_spof_ids, 2, nodes_with_storage_per_spof);

    UT_ASSERT(!rainX_rule_administrability_satisfied(involved_spof_ids, 2, spofs, SPOFS_MAX,
                                           0, &err));
}

/* User story 9 */

ut_test(node_administrable_rule_2_spofs_3_4_nodes_0_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[2] = { 3, 4 };
    spof_id_t involved_spof_ids[2];
    cl_error_desc_t
             err;

    setup_spofs(spofs, involved_spof_ids, 2, nodes_per_spof);

    UT_ASSERT(!rainX_rule_administrability_satisfied(involved_spof_ids, 2,
                                            spofs, SPOFS_MAX, 0, &err));
}

/* Part of user story 10 */
ut_test(administrable_rule_3_spofs_2_2_3_nodes_1_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[3] = { 2, 2, 3 };
    spof_id_t involved_spof_ids[3];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 3, nodes_per_spof);
    UT_ASSERT(!rainX_rule_administrability_satisfied(involved_spof_ids, 3,
                                                     spofs, SPOFS_MAX, 1, &err));
    ut_printf("err.msg = '%s'", err.msg);
}

/* Part of user story 11 */
ut_test(administrable_rule_5_spofs_2_2_1_1_1_nodes_1_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[5] = { 2, 2, 1, 1, 1 };
    spof_id_t involved_spof_ids[5];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 5, nodes_per_spof);
    UT_ASSERT(!rainX_rule_administrability_satisfied(involved_spof_ids, 5,
                                                     spofs, SPOFS_MAX, 1, &err));
    ut_printf("err.msg = '%s'", err.msg);
}

/* Part of user story 11 */
ut_test(administrable_rule_6_spofs_2_2_1_1_1_1_nodes_1_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[6] = { 2, 2, 1, 1, 1, 1 };
    spof_id_t involved_spof_ids[6];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 6, nodes_per_spof);
    UT_ASSERT(rainX_rule_administrability_satisfied(involved_spof_ids, 6,
                                                    spofs, SPOFS_MAX, 1, &err));
}

/* Part 2 of user story 12 */
ut_test(administrable_rule_2_spofs_4_3_nodes_0_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[2] = { 4, 3 };
    spof_id_t involved_spof_ids[2];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 2, nodes_per_spof);
    UT_ASSERT(!rainX_rule_administrability_satisfied(involved_spof_ids, 2,
                                                    spofs, SPOFS_MAX, 0, &err));
}

/* Part 3 of user story 12 */
ut_test(administrable_and_quorum_rule_3_spofs_2_2_3_nodes_1_1_1_participating_0_spare_satisfied)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[3] = { 2, 2, 3 };
    unsigned nodes_with_storage_per_spof[3] = { 1, 1, 1 };
    spof_id_t involved_spof_ids[3];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 3, nodes_with_storage_per_spof);
    UT_ASSERT(rainX_rule_administrability_satisfied(involved_spof_ids, 3,
                                                    spofs, SPOFS_MAX, 0, &err));

    setup_spofs(spofs, involved_spof_ids, 3, nodes_per_spof);
    UT_ASSERT(rainX_rule_quorum_satisfied(involved_spof_ids, 3,
                                          spofs, SPOFS_MAX, 0, 7, &err));
}

/* Part 4 of user story 12 */
ut_test(quorum_rule_3_spofs_1_2_4_nodes_1_1_1_participating_0_spare_infringed)
{
    exa_nodeset_t spofs[SPOFS_MAX];
    unsigned nodes_per_spof[3] = { 1, 2, 4 };
    unsigned nodes_with_storage_per_spof[3] = { 1, 1, 1 };
    spof_id_t involved_spof_ids[3];
    cl_error_desc_t err;

    setup_spofs(spofs, involved_spof_ids, 3, nodes_with_storage_per_spof);
    UT_ASSERT(rainX_rule_administrability_satisfied(involved_spof_ids, 3,
                                                    spofs, SPOFS_MAX, 0, &err));

    setup_spofs(spofs, involved_spof_ids, 3, nodes_per_spof);
    UT_ASSERT(!rainX_rule_quorum_satisfied(involved_spof_ids, 3,
                                          spofs, SPOFS_MAX, 0, 7, &err));
}
