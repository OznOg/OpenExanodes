/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _EXA_ASSERT_H
#define _EXA_ASSERT_H

#include "os/include/os_assert.h"

#define EXA_ASSERT(expr) OS_ASSERT(expr)

#define EXA_ASSERT_VERBOSE(expr, ...) \
    OS_ASSERT_VERBOSE(expr, __VA_ARGS__)

#endif /* _EXA_ASSERT_H */
