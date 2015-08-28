/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _EXA_MATH_H
#define _EXA_MATH_H

#include "os/include/os_inttypes.h"

/** Maximum of two values */
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

/** Minimum of two values */
#define MIN(a, b)  ((a) < (b) ? (a) : (b))

/**
 * Align val on a boundary so that return value <= val. Boundary
 * must be a power of 2
 */
#define ALIGN_INF(val,boundary,type) \
    ((val) & (~((type)(boundary)-1)))

/**
 * Align val on a boundary so that val <= return value. Boundary
 * must be a power of 2
 */
#define ALIGN_SUP(val,boundary,type) \
    ((((val)-1) & (~((type)(boundary)-1))) + (type)(boundary))

/**
 * Test if a value is a power of two
 */
#define IS_POWER_OF_TWO(x) \
    (((x) != 0) && ((x) & ((x) - 1)) == 0)

/**
 * Helper function that returns the ceil value of a / b
 *
 * @param[in] a  dividend
 * @param[in] q  divisor
 *
 * @returns the ceil value of a / b
 */
static inline uint64_t quotient_ceil64(uint64_t a, uint32_t q)
{
    return ((a + q - 1) / q);
}

#endif /* _EXA_MATH_H */


