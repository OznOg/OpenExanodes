/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "target/iscsi/include/iqn_filter.h"
#include "common/include/exa_assert.h"

bool iqn_filter_set(iqn_filter_t *filter, const iqn_t *pattern,
                    iqn_filter_policy_t policy)
{
    if (filter == NULL || pattern == NULL || !IQN_FILTER_POLICY_IS_VALID(policy))
        return false;

    iqn_copy(&filter->pattern, pattern);
    filter->policy = policy;

    return true;
}

const iqn_t *iqn_filter_get_pattern(const iqn_filter_t *filter)
{
    EXA_ASSERT(filter != NULL);
    return &filter->pattern;
}

iqn_filter_policy_t iqn_filter_get_policy(const iqn_filter_t *filter)
{
    EXA_ASSERT(filter != NULL);
    return filter->policy;
}

bool iqn_filter_is_equal(const iqn_filter_t *filter1, const iqn_filter_t *filter2)
{
    const iqn_t *iqn1, *iqn2;
    iqn_filter_policy_t policy1, policy2;

    EXA_ASSERT(filter1 != NULL);
    EXA_ASSERT(filter2 != NULL);

    iqn1 = iqn_filter_get_pattern(filter1);
    iqn2 = iqn_filter_get_pattern(filter2);

    policy1 = iqn_filter_get_policy(filter1);
    policy2 = iqn_filter_get_policy(filter2);

    return iqn_is_equal(iqn1, iqn2) && policy1 == policy2;
}

void iqn_filter_copy(iqn_filter_t *dest, const iqn_filter_t *src)
{
    if (dest == NULL || src == NULL)
        return;

    iqn_copy(&dest->pattern, &src->pattern);
    dest->policy = src->policy;
}

bool iqn_filter_matches(const iqn_filter_t *filter, const iqn_t *iqn,
                        iqn_filter_policy_t *policy)
{
    if (filter == NULL || iqn == NULL || policy == NULL)
        return false;

    /* We can't match a pattern against another */
    if (IQN_IS_PATTERN(iqn))
        return false;

    if (iqn_matches(iqn, &filter->pattern))
    {
        *policy = filter->policy;
        return true;
    }

    return false;
}

const char *iqn_filter_policy_to_str(iqn_filter_policy_t policy)
{
    switch (policy)
    {
    case IQN_FILTER_ACCEPT: return "accept";
    case IQN_FILTER_REJECT: return "reject";
    }

    return NULL;
}

iqn_filter_policy_t iqn_filter_policy_from_str(const char *str)
{
    if (str != NULL)
    {
        if (strcmp(str, "accept") == 0)
            return IQN_FILTER_ACCEPT;

        if (strcmp(str, "reject") == 0)
            return IQN_FILTER_REJECT;
    }

    return IQN_FILTER_NONE;
}
