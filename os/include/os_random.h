/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_RANDOM_H
#define _OS_RANDOM_H

#include "os/include/os_inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize random generator
 *
 * @os_replace{Linux, srand, srand48}
 * @os_replace{Windows, srand}
 */
void os_random_init(void);

/**
 * Cleanup random generator
 *
 * This function doesn't replace any platform-specific function
 * but it must be called once done with this module.
 */
void os_random_cleanup(void);

/**
 * See if random generator has been already initialized or not
 *
 * This function doesn't replace any platform-specific function.
 *
 * @return true if random generator initialised, false otherwise
 */
bool os_random_is_initialized(void);

/**
 * Fill a buffer with random generated numbers
 *
 * @param buf  The buffer to fill
 * @param len  The length of the buffer
 *
 * @os_replace{Linux, random, rand, drand48}
 * @os_replace{Windows, rand, rand_s}
 */
void os_get_random_bytes(void *buf, size_t len);

/**
 * Generate a double random number
 *
 * @return The generated random number
 *
 * @os_replace{Linux, random, rand, drand48}
 * @os_replace{Windows, rand, rand_s}
 */
double os_drand(void);

#ifdef __cplusplus
}
#endif

#endif /* _OS_RANDOM_H */
