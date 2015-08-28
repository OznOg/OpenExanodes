/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** @file
 *
 * Constants used by the virtualizer.
 */

#ifndef __VRT_CONSTANTES_H__
#define __VRT_CONSTANTES_H__


/** Block size used by the virtualizer (bytes). */
#define VRT_BLOCK_SIZE     4096

/**
 * Thread stack size in the VRT (in bytes).
 * This constant is voluntary set to a low value so that the creation
 * of a thread consumes little memory. As a result, one has to pay
 * attention to the stack size of the VRT functions.
 * A kernel script, named "checkstack.pl", can be used to track the
 * stack size of functions.
 */
#define VRT_THREAD_STACK_SIZE (MIN_THREAD_STACK_SIZE + 128 * 1024)

#endif /* __VRT_CONSTANTES_H__ */
