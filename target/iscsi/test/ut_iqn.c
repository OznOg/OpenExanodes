/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "target/iscsi/include/iqn.h"

#define VALID_IQN_1  "iqn.2009-04.com.seanodes:exanodes.truc.machin"
#define VALID_IQN_2  "iqn.2010-06.com.seanodes:exanodes.truc.machin"
#define VALID_IQN_3  "iqn.2009-04.org.toto:dummy.stuff"

#define VALID_PATTERN    "iqn.2009-04.com.*:exanodes.truc.machin"
#define INVALID_PATTERN  "iqn.2009-04.com.*:exanodes.*.machin"

UT_SECTION(IQN_STR_IS_VALID)

ut_test(null_str_is_not_valid)
{
    char *str = NULL;
    UT_ASSERT(!IQN_STR_IS_VALID(str));
}

ut_test(pattern_is_not_valid)
{
    UT_ASSERT(!IQN_STR_IS_VALID(VALID_PATTERN));
    UT_ASSERT(!IQN_STR_IS_VALID(INVALID_PATTERN));
}

ut_test(valid_str_is_valid)
{
    UT_ASSERT(IQN_STR_IS_VALID(VALID_IQN_1));
}

UT_SECTION(IQN_IS_VALID)

ut_test(null_iqn_is_not_valid)
{
    iqn_t *iqn = NULL;
    UT_ASSERT(!IQN_IS_VALID(iqn));
}

ut_test(valid_iqn_is_valid)
{
    iqn_t iqn;
    UT_ASSERT(iqn_from_str(&iqn, VALID_IQN_1) == EXA_SUCCESS);
    UT_ASSERT(IQN_IS_VALID(&iqn));
}

UT_SECTION(IQN_STR_IS_PATTERN)

ut_test(regular_iqn_is_not_pattern)
{
    UT_ASSERT(!IQN_STR_IS_PATTERN(VALID_IQN_1));
}

ut_test(iqn_with_single_wildcard_is_pattern)
{
    UT_ASSERT(IQN_STR_IS_PATTERN(VALID_PATTERN));
}

ut_test(iqn_with_multiple_wildcards_is_NOT_pattern)
{
    UT_ASSERT(!IQN_STR_IS_PATTERN(INVALID_PATTERN));
}

UT_SECTION(iqn_from_str)

ut_test(null_iqn_from_string_returns_EINVAL)
{
    UT_ASSERT(iqn_from_str(NULL, VALID_IQN_1) == -EINVAL);
}

ut_test(iqn_from_null_returns_EINVAL)
{
    iqn_t iqn;
    UT_ASSERT(iqn_from_str(&iqn, NULL) == -EINVAL);
}

ut_test(iqn_from_valid_string_succeeds)
{
    iqn_t iqn;
    UT_ASSERT(iqn_from_str(&iqn, VALID_IQN_1) == EXA_SUCCESS);
}

ut_test(iqn_from_invalid_pattern_returns_EINVAL)
{
    iqn_t iqn;
    UT_ASSERT(iqn_from_str(&iqn, INVALID_PATTERN) == -EINVAL);
}

ut_test(iqn_from_valid_pattern_succeeds)
{
    iqn_t iqn;
    UT_ASSERT(iqn_from_str(&iqn, VALID_PATTERN) == EXA_SUCCESS);
}

UT_SECTION(iqn_to_str)

ut_test(null_iqn_to_str_returns_NULL)
{
    UT_ASSERT(iqn_to_str(NULL) == NULL);
}

ut_test(iqn_to_str_equals_original_string)
{
    iqn_t iqn;

    iqn_from_str(&iqn, VALID_IQN_1);
    UT_ASSERT(strcmp(iqn_to_str(&iqn), VALID_IQN_1) == 0);
}

UT_SECTION(iqn_compare)

ut_test(comparing_identical_iqns_returns_zero)
{
    iqn_t iqn;

    iqn_from_str(&iqn, VALID_IQN_1);
    UT_ASSERT(iqn_compare(&iqn, &iqn) == 0);
}

ut_test(comparing_different_iqns_returns_non_zero)
{
    iqn_t iqn1, iqn2, iqn3;

    UT_ASSERT(iqn_from_str(&iqn1, VALID_IQN_1) == EXA_SUCCESS);
    UT_ASSERT(iqn_from_str(&iqn2, VALID_IQN_2) == EXA_SUCCESS);
    UT_ASSERT(iqn_from_str(&iqn3, VALID_IQN_3) == EXA_SUCCESS);

    UT_ASSERT(iqn_compare(&iqn1, &iqn2) < 0);
    UT_ASSERT(iqn_compare(&iqn2, &iqn1) > 0);

    UT_ASSERT(iqn_compare(&iqn1, &iqn3) < 0);
    UT_ASSERT(iqn_compare(&iqn3, &iqn1) > 0);

    UT_ASSERT(iqn_compare(&iqn2, &iqn3) > 0);
    UT_ASSERT(iqn_compare(&iqn3, &iqn2) < 0);
}

UT_SECTION(iqn_is_equal)

ut_test(different_iqns_are_not_equal)
{
    iqn_t iqn1, iqn2;

    iqn_from_str(&iqn1, VALID_IQN_1);
    iqn_from_str(&iqn2, VALID_IQN_2);

    UT_ASSERT(!iqn_is_equal(&iqn1, &iqn2));
}

ut_test(identical_iqns_are_equal)
{
    iqn_t iqn;

    iqn_from_str(&iqn, VALID_IQN_1);
    UT_ASSERT(iqn_is_equal(&iqn, &iqn));
}

UT_SECTION(iqn_copy)

ut_test(iqn_copies_are_equal)
{
    iqn_t iqn1, iqn2;

    iqn_from_str(&iqn1, VALID_IQN_1);
    iqn_copy(&iqn2, &iqn1);
    UT_ASSERT(iqn_is_equal(&iqn2, &iqn1));
}

ut_test(pattern_copies_are_equal)
{
    iqn_t pattern1, pattern2;

    iqn_from_str(&pattern1, VALID_PATTERN);
    iqn_copy(&pattern2, &pattern1);
    UT_ASSERT(iqn_is_equal(&pattern1, &pattern2));
}

UT_SECTION(iqn_matches)

ut_test(iqn_does_not_match_filter)
{
    iqn_t iqn, filter;

    iqn_from_str(&iqn, VALID_IQN_1);
    iqn_from_str(&filter, "iqn.5392-04.com.seanodes:*");

    UT_ASSERT(!iqn_matches(&iqn, &filter));

}

ut_test(iqn_matches_itself)
{
    iqn_t iqn;

    iqn_from_str(&iqn, VALID_IQN_1);
    UT_ASSERT(iqn_matches(&iqn, &iqn));
}

ut_test(iqn_matches_filter_with_beginning_wildcard)
{
    iqn_t iqn, filter;

    iqn_from_str(&iqn, VALID_IQN_1);
    iqn_from_str(&filter, "*2009-04.com.seanodes:exanodes.truc.machin");

    UT_ASSERT(iqn_matches(&iqn, &filter));
}

ut_test(iqn_matches_filter_with_end_wildcard)
{
    iqn_t iqn, filter;

    iqn_from_str(&iqn, VALID_IQN_1);
    iqn_from_str(&filter, "iqn.2009-04.com.seanodes:*");

    UT_ASSERT(iqn_matches(&iqn, &filter));
}

ut_test(iqn_matches_filter_with_middle_wildcard)
{
    iqn_t iqn, filter;

    iqn_from_str(&iqn, VALID_IQN_1);
    iqn_from_str(&filter, "iqn.2009-04.*exanodes.truc.machin");

    UT_ASSERT(iqn_matches(&iqn, &filter));
}

UT_SECTION(iqn_set)

ut_test(iqn_set_null_iqn_returns_EINVAL)
{
    UT_ASSERT(iqn_set(NULL, VALID_IQN_1) == -EINVAL);
}

ut_test(iqn_set_null_format_returns_EINVAL)
{
    iqn_t iqn;
    UT_ASSERT(iqn_set(&iqn, NULL) == -EINVAL);
}

ut_test(iqn_set_invalid_pattern_returns_EINVAL)
{
    iqn_t iqn;
    UT_ASSERT(iqn_set(&iqn, INVALID_PATTERN) == -EINVAL);
}

ut_test(iqn_set_valid_pattern_returns_EINVAL)
{
    iqn_t iqn;
    UT_ASSERT(iqn_set(&iqn, INVALID_PATTERN) == -EINVAL);
}

ut_test(iqn_set_too_long_returns_EINVAL)
{
    iqn_t iqn;
    UT_ASSERT(iqn_set(&iqn, "iqn.2009-04.com.seanodes:%.500d", 33 ) == -EINVAL);
}

ut_test(iqn_set)
{
    iqn_t iqn, iqn_ref;

    UT_ASSERT(iqn_from_str(&iqn_ref, "iqn.2009-04.com.seanodes:exanodes.truc.machin") == EXA_SUCCESS);
    UT_ASSERT(iqn_set(&iqn, "iqn.%d-%.2d.com.seanodes:exanodes.%s", 2009, 4, "truc.machin") == EXA_SUCCESS);
    UT_ASSERT(iqn_is_equal(&iqn, &iqn_ref));
}

ut_test(iqn_from_str_does_lowercase)
{
    iqn_t iqn, iqn_ref;

    UT_ASSERT(iqn_from_str(&iqn_ref, "iqn.2009-04.com.seanodes:exanodes.truc.machin") == EXA_SUCCESS);
    UT_ASSERT(iqn_from_str(&iqn, "iqn.2009-04.com.seanodes:exanodes.TRUC.MACHIN") == EXA_SUCCESS);
    UT_ASSERT(iqn_is_equal(&iqn, &iqn_ref));
}

ut_test(iqn_set_does_lowercase)
{
    iqn_t iqn, iqn_ref;

    UT_ASSERT(iqn_from_str(&iqn_ref, "iqn.2009-04.com.seanodes:exanodes.truc.machin") == EXA_SUCCESS);
    UT_ASSERT(iqn_set(&iqn, "iqn.2009-04.com.seanodes:exanodes.%s", "TRUC.MACHIN") == EXA_SUCCESS);
    UT_ASSERT(iqn_is_equal(&iqn, &iqn_ref));
}
