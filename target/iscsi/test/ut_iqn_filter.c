/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "target/iscsi/include/iqn_filter.h"

#define VALID_IQN_1      "iqn.2009-04.com.seanodes:exanodes.truc.machin"
#define VALID_IQN_2      "iqn.2010-05.org.seanodes:exanodes.chose.bidule"
#define VALID_PATTERN    "iqn.2009-04.com.*:exanodes.truc.machin"
#define VALID_PATTERN_2  "iqn.2009-04.com.seanodes:*.truc.machin"

UT_SECTION(iqn_filter_set)

ut_test(setting_null_filter_returns_false)
{
    iqn_t pattern;

    iqn_from_str(&pattern, VALID_PATTERN);
    UT_ASSERT_EQUAL(false, iqn_filter_set(NULL, &pattern, IQN_FILTER_ACCEPT));
}

ut_test(setting_filter_with_null_pattern_returns_false)
{
    iqn_filter_t filter;
    UT_ASSERT_EQUAL(false, iqn_filter_set(&filter, NULL, IQN_FILTER_ACCEPT));
}

ut_test(setting_filter_with_invalid_policy_returns_false)
{
    iqn_filter_t filter;
    iqn_t pattern;

    iqn_from_str(&pattern, VALID_PATTERN);
    UT_ASSERT_EQUAL(false, iqn_filter_set(&filter, &pattern,
                                          IQN_FILTER_POLICY__FIRST - 1));
    UT_ASSERT_EQUAL(false, iqn_filter_set(&filter, &pattern,
                                          IQN_FILTER_POLICY__LAST + 1));
}

ut_test(setting_filter_with_valid_values_returns_true)
{
    iqn_filter_t filter;
    iqn_t pattern;
    iqn_filter_policy_t p;

    iqn_from_str(&pattern, VALID_PATTERN);
    for (p = IQN_FILTER_POLICY__FIRST; p <= IQN_FILTER_POLICY__LAST; p++)
        UT_ASSERT_EQUAL(true, iqn_filter_set(&filter, &pattern, p));
}

UT_SECTION(iqn_filter_get_pattern)

ut_test(get_pattern_of_filter)
{
    iqn_filter_t filter;
    iqn_t pattern;

    iqn_from_str(&pattern, VALID_PATTERN);
    iqn_filter_set(&filter, &pattern, IQN_FILTER_REJECT);
    UT_ASSERT(iqn_is_equal(&pattern, iqn_filter_get_pattern(&filter)));
}

UT_SECTION(iqn_filter_get_policy)

ut_test(get_policy_of_filter)
{
    iqn_filter_t filter;
    iqn_t pattern;

    iqn_from_str(&pattern, VALID_PATTERN);
    iqn_filter_set(&filter, &pattern, IQN_FILTER_REJECT);
    UT_ASSERT_EQUAL(IQN_FILTER_REJECT, iqn_filter_get_policy(&filter));
}

UT_SECTION(iqn_filter_is_equal)

ut_test(different_patterns_make_filters_different)
{
    iqn_filter_t filter1, filter2;
    iqn_t pattern1, pattern2;

    iqn_from_str(&pattern1, VALID_PATTERN);
    iqn_from_str(&pattern2, VALID_PATTERN_2);

    iqn_filter_set(&filter1, &pattern1, IQN_FILTER_ACCEPT);
    iqn_filter_set(&filter2, &pattern2, IQN_FILTER_ACCEPT);

    UT_ASSERT(!iqn_filter_is_equal(&filter1, &filter2));
}

ut_test(different_policies_make_filters_different)
{
    iqn_filter_t filter1, filter2;
    iqn_t pattern1, pattern2;

    iqn_from_str(&pattern1, VALID_PATTERN);
    iqn_from_str(&pattern2, VALID_PATTERN);

    iqn_filter_set(&filter1, &pattern1, IQN_FILTER_ACCEPT);
    iqn_filter_set(&filter2, &pattern2, IQN_FILTER_REJECT);

    UT_ASSERT(!iqn_filter_is_equal(&filter1, &filter2));
}

ut_test(identical_filters_are_identical)
{
    iqn_filter_t filter1, filter2;
    iqn_t pattern1, pattern2;

    iqn_from_str(&pattern1, VALID_PATTERN);
    iqn_from_str(&pattern2, VALID_PATTERN);

    iqn_filter_set(&filter1, &pattern1, IQN_FILTER_ACCEPT);
    iqn_filter_set(&filter2, &pattern2, IQN_FILTER_ACCEPT);

    UT_ASSERT(iqn_filter_is_equal(&filter1, &filter2));
}

UT_SECTION(iqn_filter_copy)

ut_test(copy_is_identical_to_original)
{
    iqn_filter_t filter1, filter2;
    iqn_t pattern;

    iqn_from_str(&pattern, VALID_PATTERN);
    iqn_filter_set(&filter1, &pattern, IQN_FILTER_ACCEPT);

    iqn_filter_copy(&filter2, &filter1);
    UT_ASSERT(iqn_is_equal(iqn_filter_get_pattern(&filter1),
                           iqn_filter_get_pattern(&filter2)));
    UT_ASSERT_EQUAL(iqn_filter_get_policy(&filter1),
                    iqn_filter_get_policy(&filter2));
}

UT_SECTION(iqn_filter_match)

ut_test(null_filter_matching_returns_false)
{
    iqn_t pattern;
    iqn_filter_policy_t policy;

    iqn_from_str(&pattern, VALID_PATTERN);
    UT_ASSERT_EQUAL(false, iqn_filter_matches(NULL, &pattern, &policy));
}

ut_test(matching_null_iqn_returns_false)
{
    iqn_filter_t filter;
    iqn_filter_policy_t policy;
    iqn_t pattern;

    iqn_from_str(&pattern, VALID_PATTERN);
    iqn_filter_set(&filter, &pattern, IQN_FILTER_REJECT);
    UT_ASSERT_EQUAL(false, iqn_filter_matches(&filter, NULL, &policy));
}

ut_test(matching_with_null_policy_returns_false)
{
    iqn_filter_t filter;
    iqn_t pattern, iqn;

    iqn_from_str(&pattern, VALID_PATTERN);
    iqn_filter_set(&filter, &pattern, IQN_FILTER_REJECT);

    iqn_from_str(&iqn, VALID_IQN_1);
    UT_ASSERT_EQUAL(false, iqn_filter_matches(&filter, &iqn, NULL));
}

ut_test(matching_a_pattern_returns_false)
{
    iqn_filter_t filter;
    iqn_t pattern;
    iqn_filter_policy_t policy;

    iqn_from_str(&pattern, VALID_PATTERN);
    iqn_filter_set(&filter, &pattern, IQN_FILTER_ACCEPT);

    /* Check the pattern doesn't match itself */
    UT_ASSERT_EQUAL(false, iqn_filter_matches(&filter, &pattern, &policy));
}

ut_test(non_matching_iqn_returns_false)
{
    iqn_filter_t filter;
    iqn_t pattern, iqn;
    iqn_filter_policy_t policy;

    iqn_from_str(&pattern, VALID_PATTERN);
    iqn_filter_set(&filter, &pattern, IQN_FILTER_REJECT);

    iqn_from_str(&iqn, VALID_IQN_2);
    UT_ASSERT_EQUAL(false, iqn_filter_matches(&filter, &iqn, &policy));
}

ut_test(matching_iqn_returns_true)
{
    iqn_filter_t filter;
    iqn_t pattern, iqn;
    iqn_filter_policy_t policy;

    iqn_from_str(&pattern, VALID_PATTERN);
    iqn_filter_set(&filter, &pattern, IQN_FILTER_REJECT);

    iqn_from_str(&iqn, VALID_IQN_1);
    UT_ASSERT_EQUAL(true, iqn_filter_matches(&filter, &iqn, &policy));
}

UT_SECTION(iqn_filter_policy_to_str)

ut_test(to_str_invalid_policy_returns_null)
{
    UT_ASSERT(iqn_filter_policy_to_str(IQN_FILTER_POLICY__FIRST - 1) == NULL);
    UT_ASSERT(iqn_filter_policy_to_str(IQN_FILTER_POLICY__LAST + 1) == NULL);
}

ut_test(to_str_valid_policies_all_return_different_strings)
{
#define N_POLICIES  (IQN_FILTER_POLICY__LAST - IQN_FILTER_POLICY__FIRST + 1)
    const char *s[N_POLICIES];
    iqn_filter_policy_t p1, p2;

    for (p1 = IQN_FILTER_POLICY__FIRST; p1 <= IQN_FILTER_POLICY__LAST; p1++)
    {
        s[p1] = iqn_filter_policy_to_str(p1);
        UT_ASSERT(s[p1] != NULL);

        for (p2 = IQN_FILTER_POLICY__FIRST; p2 < p1; p2++)
            UT_ASSERT_VERBOSE(strcmp(s[p1], s[p2]) != 0,
                              "to_str(%d) is same as to_str(%d)", p1, p2);
    }
}

UT_SECTION(iqn_filter_policy_from_str)

ut_test(from_str_null_returns_IQN_FILTER_NONE)
{
    UT_ASSERT(iqn_filter_policy_from_str(NULL) == IQN_FILTER_NONE);
}

ut_test(from_str_empty_returns_IQN_FILTER_NONE)
{
    UT_ASSERT(iqn_filter_policy_from_str("") == IQN_FILTER_NONE);
}

ut_test(from_str_garbage_returns_IQN_FILTER_NONE)
{
    UT_ASSERT(iqn_filter_policy_from_str("abcdef") == IQN_FILTER_NONE);
    UT_ASSERT(iqn_filter_policy_from_str("0123") == IQN_FILTER_NONE);
}

ut_test(from_str_valid_values_return_valid_policy)
{
    iqn_filter_policy_t p;

    for (p = IQN_FILTER_POLICY__FIRST; p <= IQN_FILTER_POLICY__LAST; p++)
        UT_ASSERT_EQUAL(p, iqn_filter_policy_from_str(iqn_filter_policy_to_str(p)));
}
