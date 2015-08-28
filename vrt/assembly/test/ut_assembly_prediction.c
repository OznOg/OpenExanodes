/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/assembly/include/assembly_prediction.h"

UT_SECTION(assembly_predict_max_slots_without_sparing)

ut_test(slot_width_greater_than_nb_spof)
{
    uint64_t spof_chunks[3] = {10, 10, 10};
    UT_ASSERT_EQUAL(0, assembly_predict_max_slots_without_sparing(3, 4, spof_chunks));
}

ut_test(slot_width_equal_nb_spof)
{
    uint64_t spof_chunks[4] = {10, 10, 10, 10};
    UT_ASSERT_EQUAL(10, assembly_predict_max_slots_without_sparing(4, 4, spof_chunks));

    spof_chunks[3] = 1000;
    UT_ASSERT_EQUAL(10, assembly_predict_max_slots_without_sparing(4, 4, spof_chunks));

    spof_chunks[2] = 1000;
    UT_ASSERT_EQUAL(10, assembly_predict_max_slots_without_sparing(4, 4, spof_chunks));

    spof_chunks[1] = 1000;
    UT_ASSERT_EQUAL(10, assembly_predict_max_slots_without_sparing(4, 4, spof_chunks));

    spof_chunks[0] = 1000;
    UT_ASSERT_EQUAL(1000, assembly_predict_max_slots_without_sparing(4, 4, spof_chunks));
}

ut_test(slot_width_smaller_than_nb_spof)
{
    uint64_t spof_chunks[5] = {10, 10, 10, 10, 10};
    UT_ASSERT_EQUAL(12, assembly_predict_max_slots_without_sparing(5, 4, spof_chunks));

    spof_chunks[4] = 20;
    UT_ASSERT_EQUAL(13, assembly_predict_max_slots_without_sparing(5, 4, spof_chunks));

    spof_chunks[3] = 20;
    UT_ASSERT_EQUAL(15, assembly_predict_max_slots_without_sparing(5, 4, spof_chunks));

    spof_chunks[2] = 20;
    UT_ASSERT_EQUAL(20, assembly_predict_max_slots_without_sparing(5, 4, spof_chunks));

    spof_chunks[1] = 20;
    UT_ASSERT_EQUAL(22, assembly_predict_max_slots_without_sparing(5, 4, spof_chunks));

    spof_chunks[0] = 20;
    UT_ASSERT_EQUAL(25, assembly_predict_max_slots_without_sparing(5, 4, spof_chunks));
}

ut_test(10_nodes_homogeneous)
{
    uint64_t spof_chunks[10] = {15, 15, 15, 15, 15, 15, 15, 15, 15, 15};

    UT_ASSERT_EQUAL(75, assembly_predict_max_slots_without_sparing(10, 2, spof_chunks));
    UT_ASSERT_EQUAL(50, assembly_predict_max_slots_without_sparing(10, 3, spof_chunks));
    UT_ASSERT_EQUAL(37, assembly_predict_max_slots_without_sparing(10, 4, spof_chunks));
    UT_ASSERT_EQUAL(30, assembly_predict_max_slots_without_sparing(10, 5, spof_chunks));
    UT_ASSERT_EQUAL(25, assembly_predict_max_slots_without_sparing(10, 6, spof_chunks));
    UT_ASSERT_EQUAL(21, assembly_predict_max_slots_without_sparing(10, 7, spof_chunks));
    UT_ASSERT_EQUAL(18, assembly_predict_max_slots_without_sparing(10, 8, spof_chunks));
    UT_ASSERT_EQUAL(16, assembly_predict_max_slots_without_sparing(10, 9, spof_chunks));
    UT_ASSERT_EQUAL(15, assembly_predict_max_slots_without_sparing(10, 10, spof_chunks));
}

ut_test(10_nodes_heterogeneous)
{
    uint64_t spof_chunks[10] = {5, 5, 5, 7, 7, 10, 10, 20, 30, 50};

    UT_ASSERT_EQUAL(74, assembly_predict_max_slots_without_sparing(10, 2, spof_chunks));
    UT_ASSERT_EQUAL(49, assembly_predict_max_slots_without_sparing(10, 3, spof_chunks));
    UT_ASSERT_EQUAL(33, assembly_predict_max_slots_without_sparing(10, 4, spof_chunks));
    UT_ASSERT_EQUAL(23, assembly_predict_max_slots_without_sparing(10, 5, spof_chunks));
    UT_ASSERT_EQUAL(16, assembly_predict_max_slots_without_sparing(10, 6, spof_chunks));
    UT_ASSERT_EQUAL(12, assembly_predict_max_slots_without_sparing(10, 7, spof_chunks));
    UT_ASSERT_EQUAL(9, assembly_predict_max_slots_without_sparing(10, 8, spof_chunks));
    UT_ASSERT_EQUAL(7, assembly_predict_max_slots_without_sparing(10, 9, spof_chunks));
    UT_ASSERT_EQUAL(5, assembly_predict_max_slots_without_sparing(10, 10, spof_chunks));
}

ut_test(same_as_5_nodes_9_disk)
{
    uint64_t spof_chunks[5] = {638, 766, 766, 846, 1999};

    UT_ASSERT_EQUAL(2507, assembly_predict_max_slots_without_sparing(5, 2, spof_chunks));
    UT_ASSERT_EQUAL(1508, assembly_predict_max_slots_without_sparing(5, 3, spof_chunks));
    UT_ASSERT_EQUAL(1005, assembly_predict_max_slots_without_sparing(5, 4, spof_chunks));
    UT_ASSERT_EQUAL(638, assembly_predict_max_slots_without_sparing(5, 5, spof_chunks));
}

UT_SECTION(assembly_predict_max_slots_reserved_with_last)

ut_test(with_last_case1)
{
    uint64_t spof_chunks[11] = { 2, 3, 3, 4, 4, 5, 5, 5, 5, 6, 8 };
    UT_ASSERT_EQUAL(12, assembly_predict_max_slots_reserved_with_last(11, 2, 3, spof_chunks));
}

ut_test(with_last_case2)
{
    uint64_t spof_chunks[11] = { 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 8 };
    UT_ASSERT_EQUAL(12, assembly_predict_max_slots_reserved_with_last(11, 2, 3, spof_chunks));
}

UT_SECTION(assembly_predict_max_slots_reserved_without_last)

ut_test(without_last_case1)
{
    uint64_t spof_chunks[11] = { 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 8 };
    UT_ASSERT_EQUAL(13, assembly_predict_max_slots_reserved_without_last(11, 2, 3, spof_chunks));
}
