/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef IQN_FILTER_H
#define IQN_FILTER_H

#include "target/iscsi/include/iqn.h"

typedef enum
{
    IQN_FILTER_ACCEPT = 0x55,
    IQN_FILTER_REJECT
} iqn_filter_policy_t;

#define IQN_FILTER_POLICY__FIRST  IQN_FILTER_ACCEPT
#define IQN_FILTER_POLICY__LAST   IQN_FILTER_REJECT

#define IQN_FILTER_POLICY_IS_VALID(type) \
    ((type) >= IQN_FILTER_POLICY__FIRST && (type) <= IQN_FILTER_POLICY__LAST)

/* Symbolic value for 'no policy' or 'invalid policy' */
#define IQN_FILTER_NONE  (IQN_FILTER_POLICY__LAST + 5)

typedef struct iqn_filter
{
    iqn_t               pattern;
    iqn_filter_policy_t policy;
} iqn_filter_t;

/**
 * Set (construct) an IQN filter.
 *
 * @param[out] filter   Constructed filter
 * @param[in]  pattern  IQN pattern
 * @param[in]  policy   filter policy (action)
 *
 * @return true if successful, false otherwise
 */
bool iqn_filter_set(iqn_filter_t *filter, const iqn_t *pattern,
                    iqn_filter_policy_t policy);

/**
 * Get the IQN pattern of a filter.
 *
 * @param[in] filter  Filter to get the pattern of. Must be non null.
 *
 * @return The filter's pattern
 */
const iqn_t *iqn_filter_get_pattern(const iqn_filter_t *filter);

/**
 * Get the policy of a filter.
 *
 * @param[in] filter  Filter to get the policy of. Must be non null.
 *
 * @return The filter's policy
 */
iqn_filter_policy_t iqn_filter_get_policy(const iqn_filter_t *filter);

/**
 * Test whether an IQN filter is equal to another (same pattern, same policy).
 *
 * @param[in] filter1  IQN filter
 * @param[in] filter2  IQN filter
 *
 * @return true if the two filters are equal, false otherwise
 */
bool iqn_filter_is_equal(const iqn_filter_t *filter1, const iqn_filter_t *filter2);

/**
 * Copy an IQN filter.
 *
 * @param[out] dest  Destination filter
 * @param[in]  src   Source filter
 */
void iqn_filter_copy(iqn_filter_t *dest, const iqn_filter_t *src);

/**
 * Match an IQN against a filter.
 *
 * @param[in]  filter  IQN filter to match against
 * @param[in]  iqn     IQN
 * @param[out] policy  Filter's policy for the given IQN if the match
 *                     is successful
 *
 * @return true if the filter matches the IQN, false otherwise
 */
bool iqn_filter_matches(const iqn_filter_t *filter, const iqn_t *iqn,
                        iqn_filter_policy_t *policy);

/**
 * Return the string representation of an IQN filter policy.
 *
 * The function is thread safe and the caller need not maintain
 * a copy of the returned string.
 *
 * @param[in] policy  IQN Filter policy
 *
 * @return Filter policy as a string if the policy is valid,
 *         NULL otherwise
 */
const char *iqn_filter_policy_to_str(iqn_filter_policy_t policy);

/**
 * Parse an IQN filter policy string.
 *
 * @param[in] str  String to parse
 *
 * @return IQN filter policy if successful, IQN_FILTER_NONE otherwise
 */
iqn_filter_policy_t iqn_filter_policy_from_str(const char *str);

#endif /* IQN_FILTER_H */
