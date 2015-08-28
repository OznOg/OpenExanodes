/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "vrt/assembly/include/extent.h"
#include "vrt/common/include/memory_stream.h"

#include "os/include/os_error.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_random.h"

UT_SECTION(extent_list_basic_operations)

ut_test(extent_list_count_NULL_list_returns_zero)
{
    UT_ASSERT_EQUAL(0, extent_list_count(NULL));
}

ut_test(extent_list_count_list_returns_correct_count)
{
    int i;
    extent_t *list = NULL;

    for (i = 0; i < 10; i++)
        list = extent_list_add_value(list, i * 2);

    UT_ASSERT_EQUAL(10, extent_list_count(list));

    extent_list_free(list);

    UT_ASSERT_EQUAL(0, extent_list_count(NULL));
}

ut_test(extent_list_get_nb_values_return_correct_value)
{
    int i;
    extent_t *list = NULL;

    for (i = 0; i < 10; i++)
        list = extent_list_add_value(list, i);


    UT_ASSERT_EQUAL(10, extent_list_get_num_values(list));

    extent_list_free(list);
}

ut_test(extent_list_get_nb_values_return_correct_value_with_multiple_extents)
{
    int i;
    extent_t *list = NULL;

    for (i = 0; i < 10; i++)
        list = extent_list_add_value(list, i);

    for (i = 20; i < 30; i++)
        list = extent_list_add_value(list, i);


    UT_ASSERT_EQUAL(20, extent_list_get_num_values(list));

    extent_list_free(list);
}

ut_test(extent_list_for_each_value)
{
    int i, value;
    extent_t *list = NULL;
    extent_iter_t *cur;

    for (i = 0; i < 10; i++)
        list = extent_list_add_value(list, i);

    for (i = 20; i < 30; i++)
        list = extent_list_add_value(list, i);

    i = 0;
    extent_iter_init(list, cur);
    extent_list_iter_for_each(cur, value)
    {
        UT_ASSERT_EQUAL(i, value);

        i++;
        /* After 9, go to 20 */
        if (i == 10)
            i = 20;
    }

    extent_list_free(list);
}

ut_test(extent_list_contains_value)
{
    int i;
    extent_t *list = NULL;

    for (i = 0; i < 10; i++)
        list = extent_list_add_value(list, i);

    for (i = 20; i < 30; i++)
        list = extent_list_add_value(list, i);

    for (i = 0; i < 40; i++)
    {
        if (i < 10 || (i >= 20 && i < 30))
            UT_ASSERT(extent_list_contains_value(list, i));
        else
            UT_ASSERT(!extent_list_contains_value(list, i));
    }

    extent_list_free(list);
}

UT_SECTION(extent_list_serialization)

#define FLAT_SET_SIZE 10

ut_setup()
{
    os_random_init();
}

ut_cleanup()
{
    os_random_cleanup();
}

ut_test(extent_list_serialize_deserialize_is_identity)
{
    char buf[1024];
    stream_t *stream;
    extent_t *list, *list2;
    extent_t *c1, *c2;
    int i, j;

    list = NULL;
    for (i = 0; i < FLAT_SET_SIZE; i++)
    {
        int n = (int)(os_drand() * 10);
        for (j = 0; j < n; j++)
            list = extent_list_add_value(list, i * 10 + j);
    }

    UT_ASSERT_EQUAL(0, memory_stream_open(&stream, buf, sizeof(buf), STREAM_ACCESS_RW));

    UT_ASSERT_EQUAL(0, extent_list_serialize(list, stream));

    stream_rewind(stream);
    UT_ASSERT_EQUAL(0, extent_list_deserialize(&list2, stream));

    stream_close(stream);

    UT_ASSERT(extent_list_count(list2) == extent_list_count(list));
    UT_ASSERT(extent_list_get_num_values(list2) == extent_list_get_num_values(list));

    c1 = list;
    c2 = list2;

    while (c1 != NULL && c2 != NULL)
    {
        UT_ASSERT_EQUAL(c1->start, c2->start);
        UT_ASSERT_EQUAL(c1->end, c2->end);
        c1 = c1->next;
        c2 = c2->next;
    }
    UT_ASSERT(c1 == NULL && c2 == NULL);

    extent_list_free(list);
    extent_list_free(list2);
}

UT_SECTION(extent_list_add_value)

ut_test(extent_list_add_first_value_returns_correct_result)
{
    extent_t *list = extent_list_add_value(NULL, 1);

    UT_ASSERT_EQUAL(1, extent_list_count(list));
    UT_ASSERT_EQUAL(1, list->start);
    UT_ASSERT_EQUAL(1, list->end);

    extent_list_free(list);
}

ut_test(extent_list_add_two_disjoint_extents)
{
    extent_t *list = NULL, *cur;
    int i;

    for (i = 0; i < 10; i++)
        list = extent_list_add_value(list, i);

    for (i = 20; i < 30; i++)
        list = extent_list_add_value(list, i);

    /* The list should now contain:
     * 0-9, 20-29 */
    UT_ASSERT_EQUAL(2, extent_list_count(list));

    cur = list;
    UT_ASSERT_EQUAL(0, cur->start);
    UT_ASSERT_EQUAL(9, cur->end);

    cur = cur->next;
    UT_ASSERT_EQUAL(20, cur->start);
    UT_ASSERT_EQUAL(29, cur->end);

    extent_list_free(list);
}

ut_test(extent_list_add_inside_extent)
{
    extent_t *list = NULL;
    int i;

    for (i = 0; i < 10; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(1, extent_list_count(list));

    UT_ASSERT_EQUAL(0, list->start);
    UT_ASSERT_EQUAL(9, list->end);

    list = extent_list_add_value(list, 4);
    /* The list should still contain:
     * 0-9 */
    UT_ASSERT_EQUAL(1, extent_list_count(list));

    UT_ASSERT_EQUAL(0, list->start);
    UT_ASSERT_EQUAL(9, list->end);

    extent_list_free(list);
}

ut_test(extent_list_before_first_extent)
{
    extent_t *list = NULL, *cur;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(1, extent_list_count(list));

    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(19, list->end);

    list = extent_list_add_value(list, 8);
    /* The list should now contain:
     * 8, 10-19 */
    UT_ASSERT_EQUAL(2, extent_list_count(list));

    cur = list;
    UT_ASSERT_EQUAL(8, cur->start);
    UT_ASSERT_EQUAL(8, cur->end);
    cur = cur->next;
    UT_ASSERT_EQUAL(10, cur->start);
    UT_ASSERT_EQUAL(19, cur->end);

    extent_list_free(list);
}

ut_test(extent_list_after_last_extent)
{
    extent_t *list = NULL, *cur;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(1, extent_list_count(list));

    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(19, list->end);

    list = extent_list_add_value(list, 21);
    /* The list should now contain:
     * 10-19, 21 */
    UT_ASSERT_EQUAL(2, extent_list_count(list));

    cur = list;
    UT_ASSERT_EQUAL(10, cur->start);
    UT_ASSERT_EQUAL(19, cur->end);
    cur = cur->next;
    UT_ASSERT_EQUAL(21, cur->start);
    UT_ASSERT_EQUAL(21, cur->end);

    extent_list_free(list);
}

ut_test(extent_list_join_extents_before)
{
    extent_t *list = NULL, *cur;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(1, extent_list_count(list));

    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(19, list->end);

    list = extent_list_add_value(list, 7);
    /* The list should now contain:
     * 7, 10-19 */
    UT_ASSERT_EQUAL(2, extent_list_count(list));

    cur = list;
    UT_ASSERT_EQUAL(7, cur->start);
    UT_ASSERT_EQUAL(7, cur->end);
    cur = cur->next;
    UT_ASSERT_EQUAL(10, cur->start);
    UT_ASSERT_EQUAL(19, cur->end);

    list = extent_list_add_value(list, 9);
    /* The list should now contain:
     * 7, 9-19 */
    UT_ASSERT_EQUAL(2, extent_list_count(list));

    cur = list;
    UT_ASSERT_EQUAL(7, cur->start);
    UT_ASSERT_EQUAL(7, cur->end);

    cur = cur->next;
    UT_ASSERT_EQUAL(9, cur->start);
    UT_ASSERT_EQUAL(19, cur->end);

    extent_list_free(list);
}

ut_test(extent_list_join_extents_after)
{
    extent_t *list = NULL, *cur;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(1, extent_list_count(list));

    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(19, list->end);

    list = extent_list_add_value(list, 21);
    /* The list should now contain:
     * 10-19, 21 */
    UT_ASSERT_EQUAL(2, extent_list_count(list));

    cur = list;
    UT_ASSERT_EQUAL(10, cur->start);
    UT_ASSERT_EQUAL(19, cur->end);
    cur = cur->next;
    UT_ASSERT_EQUAL(21, cur->start);
    UT_ASSERT_EQUAL(21, cur->end);

    list = extent_list_add_value(list, 20);
    /* The list should now contain:
     * 10-21 */
    UT_ASSERT_EQUAL(1, extent_list_count(list));

    cur = list;
    UT_ASSERT_EQUAL(10, cur->start);
    UT_ASSERT_EQUAL(21, cur->end);

    extent_list_free(list);
}

ut_test(extent_list_join_both_ways)
{
    extent_t *list = NULL, *cur;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(1, extent_list_count(list));

    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(19, list->end);

    list = extent_list_add_value(list, 21);
    /* The list should now contain:
     * 10-19, 21 */
    UT_ASSERT_EQUAL(2, extent_list_count(list));

    list = extent_list_add_value(list, 7);
    /* The list should now contain:
     * 8, 10-19, 21 */
    UT_ASSERT_EQUAL(3, extent_list_count(list));

    cur = list;
    UT_ASSERT_EQUAL(7, cur->start);
    UT_ASSERT_EQUAL(7, cur->end);
    cur = cur->next;
    UT_ASSERT_EQUAL(10, cur->start);
    UT_ASSERT_EQUAL(19, cur->end);
    cur = cur->next;
    UT_ASSERT_EQUAL(21, cur->start);
    UT_ASSERT_EQUAL(21, cur->end);

    list = extent_list_add_value(list, 20);
    list = extent_list_add_value(list, 8);
    /* The list should now contain:
     * 7-8, 10-21 */
    UT_ASSERT_EQUAL(2, extent_list_count(list));

    list = extent_list_add_value(list, 9);
    /* The list should now contain:
     * 7-21 */
    UT_ASSERT_EQUAL(1, extent_list_count(list));

    cur = list;
    UT_ASSERT_EQUAL(7, cur->start);
    UT_ASSERT_EQUAL(21, cur->end);

    extent_list_free(list);
}

UT_SECTION(extent_list_remove_value)

ut_test(extent_list_remove_non_found_value_returns_correct_result)
{
    extent_t *list = NULL;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    list = extent_list_remove_value(list, 9);

    UT_ASSERT_EQUAL(1, extent_list_count(list));
    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(19, list->end);

    extent_list_free(list);
}

ut_test(extent_list_remove_at_start)
{
    extent_t *list = NULL;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    list = extent_list_remove_value(list, 10);

    UT_ASSERT_EQUAL(1, extent_list_count(list));
    UT_ASSERT_EQUAL(11, list->start);
    UT_ASSERT_EQUAL(19, list->end);

    extent_list_free(list);
}

ut_test(extent_list_remove_at_end)
{
    extent_t *list = NULL;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    list = extent_list_remove_value(list, 19);

    UT_ASSERT_EQUAL(1, extent_list_count(list));
    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(18, list->end);

    extent_list_free(list);
}

ut_test(extent_list_remove_in_the_middle)
{
    extent_t *list = NULL;
    extent_t *cur;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    list = extent_list_remove_value(list, 15);

    UT_ASSERT_EQUAL(2, extent_list_count(list));
    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(14, list->end);

    cur = list->next;
    UT_ASSERT_EQUAL(16, cur->start);
    UT_ASSERT_EQUAL(19, cur->end);

    extent_list_free(list);
}

ut_test(extent_list_remove_value_until_extent_removal)
{
    extent_t *list = NULL;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(1, extent_list_count(list));

    for (i = 10; i < 20; i++)
        list = extent_list_remove_value(list, i);

    UT_ASSERT(list == NULL);
}

ut_test(extent_list_remove_value_until_first_extent_removal)
{
    extent_t *list = NULL;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    for (i = 30; i < 40; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(2, extent_list_count(list));

    for (i = 10; i < 20; i++)
        list = extent_list_remove_value(list, i);

    UT_ASSERT_EQUAL(1, extent_list_count(list));
    UT_ASSERT_EQUAL(30, list->start);
    UT_ASSERT_EQUAL(39, list->end);

    extent_list_free(list);
}

ut_test(extent_list_remove_value_until_middle_extent_removal)
{
    extent_t *list = NULL;
    extent_t *cur;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    for (i = 30; i < 40; i++)
        list = extent_list_add_value(list, i);

    for (i = 50; i < 60; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(3, extent_list_count(list));

    for (i = 30; i < 40; i++)
        list = extent_list_remove_value(list, i);

    UT_ASSERT_EQUAL(2, extent_list_count(list));
    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(19, list->end);

    cur = list->next;
    UT_ASSERT_EQUAL(50, cur->start);
    UT_ASSERT_EQUAL(59, cur->end);

    extent_list_free(list);
}

ut_test(extent_list_remove_value_until_last_extent_removal)
{
    extent_t *list = NULL;
    extent_t *cur;
    int i;

    for (i = 10; i < 20; i++)
        list = extent_list_add_value(list, i);

    for (i = 30; i < 40; i++)
        list = extent_list_add_value(list, i);

    for (i = 50; i < 60; i++)
        list = extent_list_add_value(list, i);

    UT_ASSERT_EQUAL(3, extent_list_count(list));

    for (i = 50; i < 60; i++)
        list = extent_list_remove_value(list, i);

    UT_ASSERT_EQUAL(2, extent_list_count(list));
    UT_ASSERT_EQUAL(10, list->start);
    UT_ASSERT_EQUAL(19, list->end);

    cur = list->next;
    UT_ASSERT_EQUAL(30, cur->start);
    UT_ASSERT_EQUAL(39, cur->end);

    extent_list_free(list);
}
