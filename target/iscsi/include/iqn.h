/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef IQN_H
#define IQN_H

#include "common/include/exa_error.h"

#include "os/include/os_inttypes.h"
#include "os/include/os_string.h"

/* Maximum length of an IQN */
#define IQN_MAX_LEN   127

#if (IQN_MAX_LEN + 1) & 0x7
#error "IQN_MAX_LEN + 1 is not a multiple of 8 bytes"
#endif

/** An IQN */
typedef struct {
    char value[IQN_MAX_LEN + 1];
} iqn_t;

/* Base checks on an IQN string. Don't use it. */
#define __IQN_STR_CHECK(str) \
    ((str) != NULL && (str)[0] != '\0' && strlen(str) <= IQN_MAX_LEN)

/* Tell whether a string is a regular IQN, ie it does not contain any
   wildcard. Don't use it. */
#define __IQN_STR_IS_REGULAR(iqn_str)  (strchr((iqn_str), '*') == NULL)

/** Check whether an IQN string is valid.
    XXX Should we check the iqn is well-formed
    ("iqn.YYYY-MM.REVERSED.DOMAIN.NAME:SOMETHING")? */
#define IQN_STR_IS_VALID(str)  (__IQN_STR_CHECK(str) && __IQN_STR_IS_REGULAR(str))

/* Do NOT use this function. This is a helper for IQN_STR_IS_PATTERN(), which
   would not work if it didn't use a function: in debug mode (at least on
   Windows), C doesn't store a single instance of identical immediate
   strings, contrarily to what it does in release mode. Thus strchr() and
   strrchr() would get different instances, which means different addresses,
   and the equality check would fail. */
static inline bool __one_wildcard(const char *s)
{
    const char *p = strchr(s, '*');
    return p != NULL && strrchr(s, '*') == p;
}

/** Tell whether a string is a pattern, ie contains a single wildcard.
    Should be used together with IQN_STR_IS_VALID() */
#define IQN_STR_IS_PATTERN(str)  \
    (__IQN_STR_CHECK(str) && __one_wildcard(str))

/** Check whether an IQN is valid */
static inline bool IQN_IS_VALID(const iqn_t *iqn)
{
    return iqn != NULL && IQN_STR_IS_VALID(iqn->value);
}

/** Check whether an IQN is a pattern */
#define IQN_IS_PATTERN(iqn)  ((iqn) != NULL && IQN_STR_IS_PATTERN((iqn)->value))

/* IQN format and valud macros, for use in printf-like functions */
#define IQN_FMT       "%s"
#define IQN_VAL(iqn)  (iqn)->value

/**
 * @brief Set an IQN with based on a printf-like format.
 *
 * @param[out] iqn  IQN to set
 * @param[in]  fmt  printf-like format
 *
 * @return EXA_SUCCESS if successful, -EINVAL otherwise
 */
int iqn_set(iqn_t *iqn, const char *fmt, ...)
    __attribute__ ((__format__ (__printf__, 2, 3)));

/**
 * Convert a string to an IQN.
 *
 * @param[out] iqn  IQN parsed
 * @param[in]  str  String to convert
 *
 * @return EXA_SUCCESS if successful, -EINVAL otherwise
 */
int iqn_from_str(iqn_t *iqn, const char *str);

/**
 * Convert an IQN to a string.
 *
 * For the conversion to succeed, the string must be either a regular IQN (no
 * wildcard) or a valid pattern (a single wildcard).
 *
 * @param[in] iqn  IQN to convert
 *
 * @return IQN if successful, NULL otherwise
 */
const char *iqn_to_str(const iqn_t *iqn);

/**
 * Copy an IQN.
 *
 * @param[out] dest  Destination IQN
 * @param[in]  src   Source IQN
 */
void iqn_copy(iqn_t *dest, const iqn_t *src);

/**
 * Check whether an IQN is equal to another.
 *
 * @param[in] iqn1  An IQN
 * @param[in] iqn2  An IQN
 *
 * @return true if both IQNs are equal, false otherwise
 */
bool iqn_is_equal(const iqn_t *iqn1, const iqn_t *iqn2);

/**
 * Compare two IQNs.
 *
 * @param[in] iqn1  An IQN
 * @param[in] iqn2  An IQN
 *
 * @return < 0 if iqn1 < iqn2, 0 if iqn1 == iqn1, > 0 if iqn1 > iqn2
 */
int iqn_compare(const iqn_t *iqn1, const iqn_t *iqn2);

/**
 * Check whether an IQN matches a filter.
 *
 * A filter is either a regular IQN or an IQN containing a single wildcard
 * ('*' in its string representation).
 *
 * @param[in] iqn     IQN to check
 * @param[in] filter  Filter to match against
 *
 * @return true if the iqn matches the filter, false otherwise
 */
bool iqn_matches(const iqn_t *iqn, const iqn_t *filter);

#endif /* IQN_H */
