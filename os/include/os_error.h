/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef OS_ERROR_H
#define OS_ERROR_H

#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32

/* Define some errno macros that are not defined in Windows header. */
#define	ETIME		62	/* Timer expired */
#define	ENONET		64	/* Machine is not on the network */
#define	EPROTO          71      /* Protocol error */
#define	EOVERFLOW	75	/* Value too large for defined data type */
#define	EMSGSIZE	90	/* Message too long */
#define	EOPNOTSUPP	95	/* Operation not supported on transport endpoint */
#define	ENETDOWN	100	/* Network is down */
#define	ENETUNREACH	101	/* Network is unreachable */
#define	ECONNABORTED	103	/* Software caused connection abort */
#define	ECONNRESET	104	/* Connection reset by peer */
#define	ENOBUFS		105	/* No buffer space available */
#define	ETIMEDOUT	110	/* Connection timed out */
#define	EINPROGRESS	115	/* Operation now in progress */
#define	EWOULDBLOCK	EAGAIN	/* Operation would block */
#define	ENOMEDIUM       123     /* No medium found */

/**
 * When an error code has the OS_ERROR_WINDOWS_BIT set, this means its lower
 * 16 bits contain a Windows error code returned by GetLastError(). Else,
 * this is an errno.
 */
#define OS_ERROR_WINDOWS_BIT (1 << 29)

/**
 * Convert a Windows error code returned by GetLastError() to a libos error code.
 *
 * @param[in] error The error code
 *
 * @return A positive libos error code
 */
int os_error_from_win(int error);

#else /* WIN32 */

/**
 * When an error code has the OS_ERROR_GAI_BIT set, this means its lower
 * 16 bits contain a error code returned by getaddrinfo() instead of an errno.
 */
#define OS_ERROR_GAI_BIT (1 << 16)

/**
 * Convert an error code returned by getaddrinfo() to a libos error code.
 *
 * @param[in] ret    The return value of getaddrinfo().
 * @param[in] _errno The value of the errno just after the call to getaddrinfo().
 *
 * @return A positive libos error code corresponding to the result of
 *         getaddrinfo().
 */
int os_error_from_gai(int ret, int _errno);

#endif /* WIN32 */

/**
 * Return the string describing an error number.
 *
 * @param error  Non-negative error code
 *
 * @return A human readable string describing the error.
 */
const char *os_strerror(int error);

#ifdef __cplusplus
}
#endif

#endif
