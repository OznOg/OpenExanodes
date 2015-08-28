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

#ifndef WIN32

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

#else

/* On windows, 64bits is always long long */
#define int64_t_C(x) (x ## ll)
#define uint64_t_C(x) (x ## ull)

/* under windows "long" have 32bits even if we are on a 64bits windows */

typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef signed short int int16_t;
typedef unsigned short int uint16_t;

typedef signed int int32_t;
typedef unsigned int uint32_t;

typedef unsigned int uint;

typedef signed long long int64_t;
typedef unsigned long long uint64_t;

/* Maximum of signed integral types.  */
# define INT8_MIN              (-128)
# define INT8_MAX              (127)
# define INT16_MIN             (-32768)
# define INT16_MAX             (32767)
# define INT32_MIN             (-2147483648)
# define INT32_MAX             (2147483647)
/* -9223372036854775808 is not parsed by gcc even if in long long domain */
# define INT64_MIN             (-9223372036854775807 - 1)
# define INT64_MAX             (9223372036854775807U)

/* Maximum of unsigned integral types.  */
# define UINT8_MAX              (255)
# define UINT16_MAX             (65535)
# define UINT32_MAX             (4294967295U)
# define UINT64_MAX             (18446744073709551615U)

#define BITS_PER_LONG 32
# define EXA_SIZE_T_C(c)        c ## U
#define uint64_t_C(x) (x ## ull)

/* Decimal notation.  */
# define PRId8          "d"
# define PRId16         "hd"
# define PRId32         "d"
# define PRId64         "I64d"

/* Unsigned integers.  */
/* !!! WARNING !!! Windows' [s]scanf() is unable to parse several fields
   from a single string with PRIzu (%Iu)! Split the string so that each
   substring contains only one such format, or use another format (both
   %u and %d work fine) */
# define PRIu8          "u"
# define PRIu16         "u"
# define PRIu32         "u"
# define PRIu64         "I64u"
# define PRIx64         "I64X"
# define PRIzu          "Iu"
# define PRIzd          "d"


#define ssize_t int

#ifndef __cplusplus
#define bool    _Bool
#define true    1
#define false   0
#endif

#endif /* WIN32 */

/* Common things */

/** Max length of the string representation of an uint64_t \b without the
 * trailing '\\0'
 * stands for log10(UINT_LEAST64_MAX), this means that it is the good value
 * only for base 10 representation */
#define MAXLEN_UINT64	20

/** Max length of the string representation of an uint32_t \b without the
 * trailing '\\0'
 * stands for log10(UINT_LEAST32_MAX), this means that it is the good value
 * only for base 10 representation */
#define MAXLEN_UINT32	10

#endif /* _EXA_INTTYPES_H */
