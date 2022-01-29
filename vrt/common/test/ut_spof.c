/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/common/include/spof.h"
#include "os/include/os_inttypes.h"

UT_SECTION(spof_id_t)

ut_test(spof_id_range)
{
    spof_id_t spof_id = SPOF_ID_NONE;

    UT_ASSERT(!SPOF_ID_IS_VALID(spof_id));

    spof_id = 1;
    UT_ASSERT(SPOF_ID_IS_VALID(spof_id));

    spof_id = UINT32_MAX;
    UT_ASSERT(SPOF_ID_IS_VALID(spof_id));
}

ut_test(spof_id_from_str_with_null_str_returns_einval)
{
    spof_id_t spof_id;

    UT_ASSERT_EQUAL(-EINVAL, spof_id_from_str(&spof_id, NULL));
}

ut_test(spof_id_from_str_with_empty_str_returns_einval)
{
    spof_id_t spof_id;

    UT_ASSERT_EQUAL(-EINVAL, spof_id_from_str(&spof_id, ""));
}

ut_test(spof_id_from_str_with_zero_returns_einval)
{
    spof_id_t spof_id;

    UT_ASSERT_EQUAL(-EINVAL, spof_id_from_str(&spof_id, "0"));
}

ut_test(spof_id_from_str_with_negative_num_returns_erange)
{
    spof_id_t spof_id;

    UT_ASSERT_EQUAL(-ERANGE, spof_id_from_str(&spof_id, "-1"));
}

ut_test(spof_id_from_str_with_too_big_num_returns_erange)
{
    spof_id_t spof_id;

    UT_ASSERT_EQUAL(-ERANGE, spof_id_from_str(&spof_id, "4294967296"));
}

ut_test(spof_id_from_str_with_crap_num_returns_einvali)
{
    spof_id_t spof_id;

    UT_ASSERT_EQUAL(-EINVAL, spof_id_from_str(&spof_id, "123soleil"));
}

ut_test(spof_id_from_str_with_correct_input_returns_zero)
{
    spof_id_t spof_id;

    UT_ASSERT_EQUAL(0, spof_id_from_str(&spof_id, "1"));
    UT_ASSERT_EQUAL(1, spof_id);
    UT_ASSERT_EQUAL(0, spof_id_from_str(&spof_id, "4294967295"));
    UT_ASSERT_EQUAL(UINT32_MAX, spof_id);
}

ut_test(spof_id_to_str_with_invalid_id_returns_null)
{
    UT_ASSERT(spof_id_to_str(SPOF_ID_NONE) == NULL);
}

ut_test(spof_id_to_str_with_valid_id_returns_correct_string)
{
    UT_ASSERT_EQUAL_STR("1", spof_id_to_str(1));
    UT_ASSERT_EQUAL_STR("4294967295", spof_id_to_str(UINT32_MAX));
}

UT_SECTION(spof_t)

ut_test(spof_set_id_with_SPOF_ID_NONE_returns_EINVAL)
{
    spof_t spof;

    UT_ASSERT_EQUAL(-EINVAL, spof_set_id(&spof, SPOF_ID_NONE));
}

ut_test(spof_get_id_is_SPOF_ID_NONE_after_init)
{
    spof_t spof;

    spof_init(&spof);

    UT_ASSERT_EQUAL(SPOF_ID_NONE, spof_get_id(&spof));
}

ut_test(spof_set_id_valid_returns_zero_and_sets_id)
{
    spof_t spof;

    spof_init(&spof);

    UT_ASSERT_EQUAL(0, spof_set_id(&spof, 1));
    UT_ASSERT_EQUAL(1, spof_get_id(&spof));

    UT_ASSERT_EQUAL(0, spof_set_id(&spof, UINT32_MAX));
    UT_ASSERT_EQUAL(UINT32_MAX, spof_get_id(&spof));
}

ut_test(spof_get_num_nodes_on_empty_spof_returns_zero)
{
    spof_t spof;

    spof_init(&spof);

    UT_ASSERT_EQUAL(0, spof_set_id(&spof, 1));

    UT_ASSERT_EQUAL(0, spof_get_num_nodes(&spof));
}

ut_test(spof_get_num_nodes_on_non_empty_spof_returns_correct_value)
{
    spof_t spof;
    exa_nodeid_t i;

    spof_init(&spof);

    UT_ASSERT_EQUAL(0, spof_set_id(&spof, 1));

    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
    {
        spof_add_node(&spof, i);
        UT_ASSERT_EQUAL(i + 1, spof_get_num_nodes(&spof));
    }
}

ut_test(spof_get_nodes_with_null_nodes_returns_EINVAL)
{
    spof_t spof;

    spof_init(&spof);
    UT_ASSERT_EQUAL(-EINVAL, spof_get_nodes(&spof, NULL));
}

ut_test(spof_get_nodes_of_empty_spof_yields_empty_nodeset)
{
    spof_t spof;
    exa_nodeset_t nodes;

    spof_init(&spof);
    UT_ASSERT_EQUAL(0, spof_get_nodes(&spof, &nodes));
    UT_ASSERT(exa_nodeset_is_empty(&nodes));
}

ut_test(spof_get_nodes_yields_correct_nodeset)
{
    spof_t spof;
    exa_nodeset_t nodes;
    exa_nodeid_t i;

    spof_init(&spof);
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        if (i %2 == 0)
            spof_add_node(&spof, i);

    UT_ASSERT_EQUAL(0, spof_get_nodes(&spof, &nodes));
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        if (i % 2 == 0)
            UT_ASSERT(exa_nodeset_contains(&nodes, i));
        else
            UT_ASSERT(!exa_nodeset_contains(&nodes, i));
}

ut_test(spof_add_nodes_adds_nodes)
{
    spof_t spof;
    exa_nodeid_t i = 0;

    spof_init(&spof);

    UT_ASSERT_EQUAL(0, spof_set_id(&spof, 1));
    UT_ASSERT_EQUAL(0, spof_get_num_nodes(&spof));

    /* Add every node possible */
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
    {
        spof_add_node(&spof, i);
        UT_ASSERT_EQUAL(i + 1, spof_get_num_nodes(&spof));
    }

    /* Verify the SPOF contains the max number of nodes */
    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER, spof_get_num_nodes(&spof));
}

ut_test(spof_remove_node_removes_nodes)
{
    spof_t spof;
    exa_nodeid_t i = 0;

    spof_init(&spof);

    UT_ASSERT_EQUAL(0, spof_set_id(&spof, 1));

    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        spof_add_node(&spof, i);

    /* Verify the SPOF contains the max number of nodes */
    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER, spof_get_num_nodes(&spof));

    /* Remove everything */
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
    {
        spof_remove_node(&spof, i);
        UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER - i - 1, spof_get_num_nodes(&spof));
    }

    /* Verify they're gone */
    UT_ASSERT_EQUAL(0, spof_get_num_nodes(&spof));
}

ut_test(spof_contains_nodes_returns_correct_value)
{
    spof_t spof;
    exa_nodeid_t i = 0;

    spof_init(&spof);

    UT_ASSERT_EQUAL(0, spof_set_id(&spof, 1));
    UT_ASSERT_EQUAL(0, spof_get_num_nodes(&spof));

    /* Verify the spof contains no node. */
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        UT_ASSERT_EQUAL(false, spof_contains_node(&spof, i));

    /* add even node ids. */
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i+= 2)
        spof_add_node(&spof, i);

    /* check SPOF contains only even node ids */
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        UT_ASSERT_EQUAL((i % 2 == 0), spof_contains_node(&spof, i));
    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER / 2, spof_get_num_nodes(&spof));

    /* Remove even node ids. */
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i+= 2)
        spof_remove_node(&spof, i);

    /* Verify the spof contains no node again. */
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
        UT_ASSERT_EQUAL(false, spof_contains_node(&spof, i));
}

ut_test(spof_copy_copies_spof)
{
    spof_t src;
    spof_t dest;
    int i;

    spof_init(&src);

    spof_set_id(&src, 654987);
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i+= 2)
        spof_add_node(&src, i);

    spof_copy(&dest, &src);

    UT_ASSERT_EQUAL(spof_get_id(&src), spof_get_id(&dest));
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i+= 2)
        UT_ASSERT_EQUAL(spof_contains_node(&src, i), spof_contains_node(&dest, i));
}

UT_SECTION(spof_lookup)

ut_test(lookup_empty_array_returns_null)
{
    spof_t spofs[5] = { 0 };

    UT_ASSERT(spof_lookup(spofs, 0, (spof_id_t)3) == NULL);
}

ut_test(lookup_non_existent_spof_returns_null)
{
    spof_t spofs[5];
    int i;

    for (i = 0; i < 5; i++)
    {
        spof_init(&spofs[i]);
        spof_set_id(&spofs[i], i + 1);
        /* No need to set the spof's nodes */
    }

    UT_ASSERT(spof_lookup(spofs, 5, (spof_id_t)18) == NULL);
}

ut_test(lookup_existing_entry_returns_spof)
{
    spof_t spofs[5];
    int i;

    for (i = 0; i < 5; i++)
    {
        spof_init(&spofs[i]);
        spof_set_id(&spofs[i], i + 1);
        /* No need to set the spof's nodes */
    }

    for (i = 0; i < 5; i++)
        UT_ASSERT(spof_lookup(spofs, 5, (spof_id_t)(i + 1)) == &spofs[i]);
}
