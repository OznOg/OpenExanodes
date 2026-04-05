/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _EXA_INTTYPES_H
#define _EXA_INTTYPES_H

/**
 * \file exa_inttypes.h
 * \brief fixed size integer types definitions and formatting
 */

#ifdef __KERNEL__

#include <linux/types.h>

#else /* __KERNEL__ */

#include <sys/types.h> /* size_t */
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>

#if __WORDSIZE == 64
#define int64_t_C(x) (x ## l)
#define uint64_t_C(x) (x ## ul)
#else
#define int64_t_C(x) (x ## ll)
#define uint64_t_C(x) (x ## ull)
#endif

#endif /* __KERNEL__ */

# define PRIzu          "zu"
# define PRIzd          "zd"

/* Common things */

/** Max length of the string representation of an uint64_t \b without the
 * trailing '\0'
 * stands for log10(UINT_LEAST64_MAX), this means that it is the good value
 * only for base 10 representation */
#define MAXLEN_UINT64	20

/** Max length of the string representation of an uint32_t \b without the
 * trailing '\0'
 * stands for log10(UINT_LEAST32_MAX), this means that it is the good value
 * only for base 10 representation */
#define MAXLEN_UINT32	10

#endif /* _EXA_INTTYPES_H */
