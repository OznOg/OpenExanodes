/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unit_testing.h>

#include "common/include/exa_nodeset.h"
#include "common/include/exa_error.h"

#include "os/include/os_stdio.h"

typedef struct
{
    exa_nodeset_t empty_set;
    exa_nodeset_t full_set;
}  empty_full_sets_t;


UT_SECTION(exa_nodeid_valid)

ut_test(id_equals_max_nodes)
{
    UT_ASSERT(!EXA_NODEID_VALID(EXA_MAX_NODES_NUMBER));
}

ut_test(id_equals_localhost)
{
    UT_ASSERT(!EXA_NODEID_VALID(EXA_NODEID_LOCALHOST));
}

ut_test(id_equals_none)
{
    UT_ASSERT(!EXA_NODEID_VALID(EXA_NODEID_NONE));
}

ut_test(id_is_negative)
{
    UT_ASSERT(!EXA_NODEID_VALID(-1));
}

ut_test(id_in_valid_range)
{
    exa_nodeid_t id;

    for (id = 0; id < EXA_MAX_NODES_NUMBER; id++)
        UT_ASSERT(EXA_NODEID_VALID(id));
}

UT_SECTION(exa_nodeid_to_str)

ut_test(invalid_node_ids_yield_null)
{
    UT_ASSERT(exa_nodeid_to_str(EXA_MAX_NODES_NUMBER) == NULL);
    UT_ASSERT(exa_nodeid_to_str(EXA_MAX_NODES_NUMBER + 1) == NULL);

    UT_ASSERT(exa_nodeid_to_str(EXA_NODEID_LOCALHOST) == NULL);
    UT_ASSERT(exa_nodeid_to_str(EXA_NODEID_NONE) == NULL);
}

ut_test(valid_node_ids_yield_valid_strings)
{
    exa_nodeid_t node_id;

    for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
    {
        char str[8];
        const char *s;

        os_snprintf(str, sizeof(str), "%"PRInodeid, node_id);
        s = exa_nodeid_to_str(node_id);
        UT_ASSERT(s != NULL);
        UT_ASSERT(strcmp(s, str) == 0);
    }
}

UT_SECTION(exa_nodeid_from_str)

ut_test(null_nodeid_returns_EINVAL)
{
    const char *s = "53";
    UT_ASSERT(exa_nodeid_from_str(NULL, s) == -EINVAL);
}

ut_test(from_null_string_returns_EINVAL)
{
    exa_nodeid_t node_id;
    UT_ASSERT(exa_nodeid_from_str(&node_id, NULL) == -EINVAL);
}

ut_test(to_str_then_from_str_is_identity)
{
    exa_nodeid_t node_id;

    for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
    {
        exa_nodeid_t node_id_2;
        const char *s;

        s = exa_nodeid_to_str(node_id);
        UT_ASSERT(exa_nodeid_from_str(&node_id_2, s) == EXA_SUCCESS);
        UT_ASSERT(node_id_2 == node_id);
    }
}

static void __identity_holds(const char *str)
{
    exa_nodeid_t node_id;
    const char *s;

    UT_ASSERT(exa_nodeid_from_str(&node_id, str) == EXA_SUCCESS);
    s = exa_nodeid_to_str(node_id);
    UT_ASSERT(s != NULL);
    UT_ASSERT(strcmp(s, str) == 0);
}

ut_test(from_str_then_to_str_is_identity)
{
    const char *values[] = { "0", "1", "2", "32", "55", "127", NULL };
    int i;
    char str[8];

    exa_nodeid_t node_id;
    const char *s;

    for (i = 0; values[i] != NULL; i++)
    {
        UT_ASSERT(exa_nodeid_from_str(&node_id, values[i]) == EXA_SUCCESS);
        s = exa_nodeid_to_str(node_id);
        UT_ASSERT(s != NULL);
        UT_ASSERT(strcmp(s, values[i]) == 0);
    }

    os_snprintf(str, sizeof(str), "%"PRInodeid, EXA_MAX_NODES_NUMBER - 1);
    __identity_holds(str);
}

UT_SECTION(empty_nodesets)

ut_test(count_empty_set)
{
    exa_nodeid_t node;
    exa_nodeset_t sets[2];

    exa_nodeset_reset(&sets[0]);
    exa_nodeset_reset(&sets[1]);

    UT_ASSERT_EQUAL(0, exa_nodeset_count(&sets[0]));

    node = 1;
    exa_nodeset_add(&sets[1], node);
    UT_ASSERT_EQUAL(1, exa_nodeset_count(&sets[1]));

    exa_nodeset_del(&sets[1], node);
    UT_ASSERT_EQUAL(0, exa_nodeset_count(&sets[1]));

}

ut_test(compare_empty_sets)
{
    exa_nodeset_t sets[2];

    exa_nodeset_reset(&sets[0]);
    exa_nodeset_reset(&sets[1]);

    UT_ASSERT(exa_nodeset_equals(&sets[0], &sets[1]));
    UT_ASSERT_EQUAL(0, exa_nodeset_count(&sets[0]));
}

ut_test(sum_empty_sets)
{
    exa_nodeset_t sets[2];

    exa_nodeset_reset(&sets[0]);
    exa_nodeset_reset(&sets[1]);

    exa_nodeset_sum(&sets[0], &sets[1]);
    UT_ASSERT(exa_nodeset_is_empty(&sets[0]));
}

ut_test(substract_empty_sets)
{
    exa_nodeset_t sets[2];

    exa_nodeset_reset(&sets[0]);
    exa_nodeset_reset(&sets[1]);

    exa_nodeset_substract(&sets[0], &sets[1]);
    UT_ASSERT(exa_nodeset_is_empty(&sets[0]));
}

ut_test(copy_empty_sets)
{
    exa_nodeset_t sets[2];

    exa_nodeset_reset(&sets[0]);

    exa_nodeset_copy(&sets[1], &sets[0]);
    UT_ASSERT(exa_nodeset_is_empty(&sets[1]));
}

ut_test(intersect_empty_sets)
{
    exa_nodeset_t sets[2];

    exa_nodeset_reset(&sets[0]);
    exa_nodeset_reset(&sets[1]);

    exa_nodeset_intersect(&sets[0], &sets[1]);
    UT_ASSERT(exa_nodeset_is_empty(&sets[0]));
}

ut_test(first_at_empty_set)
{
    exa_nodeset_t set;

    exa_nodeset_reset(&set);
    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER, exa_nodeset_first_at(&set, 1));
}

ut_test(first_empty_set)
{
    exa_nodeset_t set;

    exa_nodeset_reset(&set);
    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER, exa_nodeset_first(&set));
}

ut_test(contains_empty_set_special_nodes)
{
    exa_nodeset_t set;

    exa_nodeset_reset(&set);
    UT_ASSERT(!exa_nodeset_contains(&set, EXA_NODEID_LOCALHOST));
    UT_ASSERT(!exa_nodeset_contains(&set, EXA_NODEID_NONE));
}

UT_SECTION(non_empty_nodesets)

ut_test(copying_empty_set)
{
    exa_nodeset_t set;

    exa_nodeset_copy(&set, &EXA_NODESET_EMPTY);
    UT_ASSERT(exa_nodeset_is_empty(&set));
    UT_ASSERT(exa_nodeset_equals(&set, &EXA_NODESET_EMPTY));
}

ut_test(copying_full_set)
{
    exa_nodeset_t set;

    exa_nodeset_copy(&set, &EXA_NODESET_FULL);

    UT_ASSERT(exa_nodeset_equals(&set, &EXA_NODESET_FULL));
}

ut_test(adding_one_node_empty_set)
{
    exa_nodeset_t set;
    exa_nodeid_t i;

    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
    {
	exa_nodeset_reset(&set);
	exa_nodeset_add(&set, i);

	UT_ASSERT_EQUAL(1, exa_nodeset_count(&set));
	UT_ASSERT(exa_nodeset_contains(&set, i));
	UT_ASSERT(!exa_nodeset_is_empty(&set));
    }

}

ut_test(adding_all_nodes_empty_set)
{
    exa_nodeset_t set;
    int i;

    exa_nodeset_reset(&set);
    for (i = 0 ; i < EXA_MAX_NODES_NUMBER ; i++)
    {
	exa_nodeset_add(&set, i);
	UT_ASSERT_EQUAL(i + 1, exa_nodeset_count(&set));
    }

    for (i = 0 ; i < EXA_MAX_NODES_NUMBER ; i++)
    {
	UT_ASSERT(exa_nodeset_contains(&set, i));
    }

    UT_ASSERT(!exa_nodeset_is_empty(&set));
}

ut_test(del_all_nodes_full_set)
{
    exa_nodeset_t set;
    int count;
    exa_nodeid_t i;

    exa_nodeset_copy(&set, &EXA_NODESET_FULL);

    count = exa_nodeset_count(&set);

    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
    {
	exa_nodeset_del(&set, i);
	count--;
	UT_ASSERT(!exa_nodeset_contains(&set, i));
	UT_ASSERT_EQUAL(count, exa_nodeset_count(&set));
    }
    UT_ASSERT(exa_nodeset_is_empty(&set));
}

ut_test(not_equals_sets)
{
    exa_nodeset_t set1, set2;
    int i;

    exa_nodeset_copy(&set1, &EXA_NODESET_EMPTY);

    for (i = 0 ; i < 5 ; i++)
	exa_nodeset_add(&set1, i);
    UT_ASSERT(!exa_nodeset_equals(&set1,&EXA_NODESET_EMPTY));

    exa_nodeset_copy(&set2, &EXA_NODESET_FULL);

    for (i = 0 ; i < 5 ; i++)
	exa_nodeset_del(&set2, i);
    UT_ASSERT(!exa_nodeset_equals(&set2,&EXA_NODESET_FULL));
}

ut_test(sum_on_two_sets_into_full_set)
{
    exa_nodeset_t set1, set2;
    exa_nodeid_t i;

    exa_nodeset_reset(&set1);
    exa_nodeset_reset(&set2);
    for (i = 0; i <= EXA_MAX_NODES_NUMBER / 2; i++)
    {
	exa_nodeset_add(&set1, i);
    }
    for (i = EXA_MAX_NODES_NUMBER / 2 + 1; i < EXA_MAX_NODES_NUMBER; i++)
    {
	exa_nodeset_add(&set2, i);
    }

    exa_nodeset_sum(&set1, &set2);

    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER, exa_nodeset_count(&set1));
    UT_ASSERT(exa_nodeset_equals(&set1, &EXA_NODESET_FULL));
}

ut_test(substract_two_full_sets)
{
    exa_nodeset_t set1, set2;

    exa_nodeset_copy(&set1, &EXA_NODESET_FULL);
    exa_nodeset_copy(&set2, &EXA_NODESET_FULL);

    exa_nodeset_substract(&set1, &set2);

    UT_ASSERT(exa_nodeset_is_empty(&set1));
}

ut_test(substract_two_sets)
{
    exa_nodeset_t set1, set2, set3;
    int i;

    exa_nodeset_reset(&set1);
    exa_nodeset_copy(&set1, &EXA_NODESET_FULL);
    exa_nodeset_copy(&set2, &EXA_NODESET_FULL);
    exa_nodeset_reset(&set3);

    for (i = 0 ; i < 10 ; i++)
    {
	exa_nodeset_del(&set2, i);
	exa_nodeset_add(&set3, i);
    }

    exa_nodeset_substract(&set1, &set2);

    UT_ASSERT(exa_nodeset_equals(&set1, &set3));

}

ut_test(intersect_on_two_full_sets)
{
    exa_nodeset_t set;

    exa_nodeset_copy(&set, &EXA_NODESET_FULL);

    exa_nodeset_intersect(&set, &EXA_NODESET_FULL);

    UT_ASSERT(exa_nodeset_equals(&set, &EXA_NODESET_FULL));
}

ut_test(intersect_on_two_sets)
{
    exa_nodeset_t set;
    exa_nodeid_t node;

    exa_nodeset_reset(&set);
    exa_nodeset_add(&set, 25);

    exa_nodeset_intersect(&set, &EXA_NODESET_FULL);
    UT_ASSERT(exa_nodeset_contains(&set, 25));

    for (node = 0; node < EXA_MAX_NODES_NUMBER; node++)
    {
	if (node != 25)
	    UT_ASSERT(!exa_nodeset_contains(&set, node));
    }
}

ut_test(get_next_node_two_nodes_set)
{
    exa_nodeset_t set;

    exa_nodeset_reset(&set);
    exa_nodeset_add(&set, 25);
    exa_nodeset_add(&set, 27);

    UT_ASSERT_EQUAL(25, exa_nodeset_first(&set));

    UT_ASSERT_EQUAL(27, exa_nodeset_first_at(&set, 26));
}

ut_test(get_next_node_empty_set)
{
    exa_nodeset_t set;

    exa_nodeset_reset(&set);
    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER, exa_nodeset_first(&set));
}

ut_test(get_next_node_full_set)
{
    exa_nodeset_t set;

    exa_nodeset_copy(&set, &EXA_NODESET_FULL);

    UT_ASSERT_EQUAL(0, exa_nodeset_first(&set));

    UT_ASSERT_EQUAL(127, exa_nodeset_first_at(&set, 127));

    exa_nodeset_del(&set, 19);
    UT_ASSERT_EQUAL(20, exa_nodeset_first_at(&set, 19));
}

ut_test(number_of_sig_bits)
{
    exa_nodeset_t set;
    exa_nodeid_t node;
    exa_nodeset_reset(&set);

    for (node = 0; node < EXA_MAX_NODES_NUMBER / 2; node++)
    {
	exa_nodeset_add(&set, node);
    }
    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER / 2, exa_nodeset_num_bits(&set));

    exa_nodeset_reset(&set);
    for (node = EXA_MAX_NODES_NUMBER - 1; node > EXA_MAX_NODES_NUMBER / 2; node--)
    {
	exa_nodeset_add(&set, node);
    }
    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER, exa_nodeset_num_bits(&set));
    UT_ASSERT_EQUAL(EXA_MAX_NODES_NUMBER, exa_nodeset_num_bits(&EXA_NODESET_FULL));
}

UT_SECTION(hex_and_bin_representations)

ut_test(empty_set_in_hex)
{
    int i;
    empty_full_sets_t sets;
    exa_nodeset_t * empty;
    char hex1[EXA_NODESET_HEX_SIZE + 1];
    char hex2[EXA_NODESET_HEX_SIZE + 1];

    exa_nodeset_reset(&sets.empty_set);
    exa_nodeset_reset(&sets.full_set);

    empty = &sets.empty_set;

    exa_nodeset_to_hex(empty, hex1);
    strcpy(hex2, "00000000000000000000000000000000");
    UT_ASSERT_EQUAL_STR(hex2, hex1);

    for (i = 0; i < 16; i++)
    {
	exa_nodeset_add(empty, i);
    }

    exa_nodeset_to_hex(empty, hex1);
    strcpy(hex2, "0000000000000000000000000000ffff");
    UT_ASSERT_EQUAL_STR(hex2, hex1);
}

ut_test(full_set_in_hex)
{
    int i;
    empty_full_sets_t sets;
    exa_nodeset_t * full;
    char hex1[EXA_NODESET_HEX_SIZE + 1];
    char hex2[EXA_NODESET_HEX_SIZE + 1];

    exa_nodeset_reset(&sets.empty_set);
    exa_nodeset_copy(&sets.full_set, &EXA_NODESET_FULL);

    full = &sets.full_set;

    exa_nodeset_to_hex(full, hex1);
    strcpy(hex2, "ffffffffffffffffffffffffffffffff");
    UT_ASSERT_EQUAL_STR(hex2, hex1);

    for (i = 0; i < 16; i++)
    {
	exa_nodeset_del(full, i);
    }
    exa_nodeset_to_hex(full, hex1);
    strcpy(hex2, "ffffffffffffffffffffffffffff0000");
    UT_ASSERT_EQUAL_STR(hex2, hex1);

    exa_nodeset_copy(full, &EXA_NODESET_FULL);
}

ut_test(nodeset_from_hex_with_invalid_length_returns_INVALID_VALUE)
{
    const char *hex = "13579ace";
    exa_nodeset_t set;

    UT_ASSERT(exa_nodeset_from_hex(&set, hex) == -EXA_ERR_INVALID_VALUE);
}

ut_test(nodeset_from_hex_with_nonhex_returns_INVALID_VALUE)
{
    /* Must be of EXA_NODESET_HEX_SIZE long to ensure we test what we say */
    char hex[EXA_NODESET_HEX_SIZE + 1];
    int i;
    exa_nodeset_t set;

    for (i = 0; i < 16; i++)
        hex[i] = (i < 10 ? '0' + i : 'a' + i - 10);
    hex[i++] = 'z'; /* non-hex character */
    while (i < EXA_NODESET_HEX_SIZE)
        hex[i++] = 'f';
    hex[i] = '\0';

    UT_ASSERT(strlen(hex) == EXA_NODESET_HEX_SIZE);
    UT_ASSERT(exa_nodeset_from_hex(&set, hex) == -EXA_ERR_INVALID_VALUE);
}

ut_test(any_set_in_hex)
{
    exa_nodeset_t set1, set2;
    char hex1[EXA_NODESET_HEX_SIZE + 1];
    char hex2[EXA_NODESET_HEX_SIZE + 1];

    strcpy(hex1,"1574fedc563eab569fff5481fabcdef1");
    /* hex1    -> set1 */
    exa_nodeset_from_hex(&set1, hex1);
    /* set1 -> hex2 */
    exa_nodeset_to_hex(&set1, hex2);
    /* hex1    =  hex2 */
    UT_ASSERT_EQUAL_STR(hex2, hex1);

    exa_nodeset_del(&set1,124);
    strcpy(hex2,"0574fedc563eab569fff5481fabcdef1");
    exa_nodeset_from_hex(&set2, hex2);

    UT_ASSERT_NOT_EQUAL_STR(hex2, hex1);
    UT_ASSERT(exa_nodeset_equals(&set1, &set2));
}

ut_test(full_set_to_bin)
{
    empty_full_sets_t sets;
    exa_nodeset_t * full;
    char bin[EXA_MAX_NODES_NUMBER + 1];
    int i;

    exa_nodeset_reset(&sets.empty_set);
    exa_nodeset_copy(&sets.full_set, &EXA_NODESET_FULL);

    full = &sets.full_set;
    exa_nodeset_to_bin(full, bin);

    for (i = 0 ; i < EXA_MAX_NODES_NUMBER ; i++ )
    {
	UT_ASSERT(bin[i] != '0');
    }
}

ut_test(full_set_less_zero_to_bin)
{
    empty_full_sets_t sets;
    exa_nodeset_t * full;
    char bin[EXA_MAX_NODES_NUMBER + 1];
    int i;

    exa_nodeset_reset(&sets.empty_set);
    exa_nodeset_copy(&sets.full_set, &EXA_NODESET_FULL);
    full = &sets.full_set;

    if (!exa_nodeset_equals(full, &EXA_NODESET_FULL))
	exa_nodeset_copy(full, &EXA_NODESET_FULL);

    exa_nodeset_del(full, 0);
    exa_nodeset_to_bin(full, bin);

    UT_ASSERT(bin[EXA_MAX_NODES_NUMBER - 1] != '1');

    for (i = 0 ; i < EXA_MAX_NODES_NUMBER - 2 ; i++ )
    {
	UT_ASSERT(bin[i] != '0');
    }
}

ut_test(full_set_less_max_nodeid_to_bin)
{
    empty_full_sets_t sets;
    exa_nodeset_t * full;
    char bin[EXA_MAX_NODES_NUMBER + 1];
    int i;

    exa_nodeset_reset(&sets.empty_set);
    exa_nodeset_copy(&sets.full_set, &EXA_NODESET_FULL);
    full = &sets.full_set;

    exa_nodeset_del(full, EXA_MAX_NODES_NUMBER - 1);
    exa_nodeset_to_bin(full, bin);

    /* when the highest node(s) is(are) deleted the binary has less elements
       initialized  than the full nodeset */
    for (i = 0 ; i < EXA_MAX_NODES_NUMBER - 1 ; i++ )
    {
	/* The last element of bin is 0 as node 0 was deleted in previous test */
	UT_ASSERT((bin[i] != '0') || (i == EXA_MAX_NODES_NUMBER - 2));
    }
}

UT_SECTION(inclusion)

ut_test(empty_included_in_empty)
{
    exa_nodeset_t empty;

    exa_nodeset_reset(&empty);
    UT_ASSERT(exa_nodeset_included(&empty, &empty));
}

ut_test(no_nonempty_included_in_empty)
{
    exa_nodeset_t empty;
    exa_nodeset_t non_empty;
    int i;

    exa_nodeset_reset(&empty);

    exa_nodeset_reset(&non_empty);
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
    {
        exa_nodeset_add(&non_empty, i);
        UT_ASSERT(!exa_nodeset_included(&non_empty, &empty));
    }
}

ut_test(empty_included_in_all_nonempty)
{
    exa_nodeset_t empty;
    exa_nodeset_t non_empty;
    int i;

    exa_nodeset_reset(&empty);

    exa_nodeset_reset(&non_empty);
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
    {
        exa_nodeset_add(&non_empty, i);
        UT_ASSERT(exa_nodeset_included(&empty, &non_empty));
    }
}

UT_SECTION(disjoint)

ut_test(empty_and_empty_are_disjoint)
{
    UT_ASSERT(exa_nodeset_disjoint(&EXA_NODESET_EMPTY, &EXA_NODESET_EMPTY));
}

ut_test(non_empty_not_disjoint_with_itself)
{
    exa_nodeset_t non_empty;

    exa_nodeset_reset(&non_empty);
    exa_nodeset_add(&non_empty, 0);
    exa_nodeset_add(&non_empty, 5);
    exa_nodeset_add(&non_empty, 11);
    exa_nodeset_add(&non_empty, 32);
    exa_nodeset_add(&non_empty, 64);

    UT_ASSERT(!exa_nodeset_disjoint(&non_empty, &non_empty));
}

ut_test(non_empty_with_no_common_nodes_are_disjoint)
{
    exa_nodeset_t a;
    exa_nodeset_t b;

    exa_nodeset_reset(&a);
    exa_nodeset_add(&a, 1);
    exa_nodeset_add(&a, 3);
    exa_nodeset_add(&a, 5);
    exa_nodeset_add(&a, 127);

    exa_nodeset_reset(&b);
    exa_nodeset_add(&b, 2);
    exa_nodeset_add(&b, 4);
    exa_nodeset_add(&b, 16);
    exa_nodeset_add(&b, 125);

    UT_ASSERT(exa_nodeset_disjoint(&a, &b));
}

ut_test(non_empty_with_common_nodes_are_not_disjoint)
{
    exa_nodeset_t a;
    exa_nodeset_t b;

    exa_nodeset_reset(&a);
    exa_nodeset_add(&a, 1);
    exa_nodeset_add(&a, 3);
    exa_nodeset_add(&a, 5);
    exa_nodeset_add(&a, 127);

    exa_nodeset_reset(&b);
    exa_nodeset_add(&b, 2);
    exa_nodeset_add(&b, 5);
    exa_nodeset_add(&b, 49);
    exa_nodeset_add(&b, 127);

    UT_ASSERT(!exa_nodeset_disjoint(&a, &b));
}

ut_test(full_and_empty_are_disjoint)
{
    UT_ASSERT(exa_nodeset_disjoint(&EXA_NODESET_FULL, &EXA_NODESET_EMPTY));
}

ut_test(full_and_non_empty_are_not_disjoint)
{
    exa_nodeset_t full;
    exa_nodeset_t a;

    exa_nodeset_copy(&full, &EXA_NODESET_FULL);

    exa_nodeset_reset(&a);
    exa_nodeset_add(&a, 0);
    exa_nodeset_add(&a, 33);
    exa_nodeset_add(&a, 57);
    exa_nodeset_add(&a, 88);
    exa_nodeset_add(&a, 89);
    exa_nodeset_add(&a, 113);

    UT_ASSERT(!exa_nodeset_disjoint(&full, &a));
}
