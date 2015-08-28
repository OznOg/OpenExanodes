/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __OS_STDIO_H
#define __OS_STDIO_H

#include <stdio.h>
#include <stdarg.h>

/* NOTE: When adding colors here, add code to handle it in Windows'
   implementation of os_colorful_vfprintf() */
#define OS_COLOR_RED     "\033[1;31m"
#define OS_COLOR_GREEN   "\033[1;32m"
#define OS_COLOR_BLUE    "\033[1;34m"
#define OS_COLOR_PURPLE  "\033[1;35m"
#define OS_COLOR_CYAN    "\033[1;36m"
#define OS_COLOR_DEFAULT "\033[0m"
#define OS_COLOR_LIGHT   "\033[1m"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32

/* va_copy is not implemented on windows...
 * the following is a crapy workaround made necessary because of the little
 * case MS has with standards... */
#ifndef va_copy
# define va_copy(d, s) (d) = (s)
#endif

#endif /* WIN32*/

/**
 * Linux-like snprintf for windows.
 * (The native snprintf does not conform to standard nor to linux one.)
 *
 * This version provides a snprintf() which has the same return values as the
 * Linux one. Thus it ALWAYS returns the amount of bytes which WOULD have
 * been written if size were big enough. This means that a return value equal
 * to size means the string was truncated.
 *
 * This function guarantees that str will be null terminated in any case.
 *
 * @param[out] str     String to print to
 * @param[in]  size    Size of str
 * @param[in]  format  Format for printing the arguments
 * @param[in]  ...     Arguments
 *
 * @return Same as Linux's snprintf().
 *
 * @os_replace{Linux, snprintf}
 * @os_replace{Windows, _snprintf}
 */
int os_snprintf(char *str, size_t size, const char *format, ...);

/**
 * Implementation of a Linux-like vsnprintf for Windows.
 * Same explanation as for os_snprintf().
 *
 * @os_replace{Linux, vsnprintf}
 * @os_replace{Windows, vsnprintf, _vsnprintf}
 */
int os_vsnprintf(char *str, size_t size, const char *format, va_list ap);

/**
 * Colorful printing in a file.
 *
 * To use colors, use the string constants OS_COLOR_xxx.
 *
 * @param[in] stream  File to print to
 * @param[in] format  Format for printing the arguments
 * @param[in] ...     Arguments
 *
 * @return Same as Linux's fprintf().
 */
int os_colorful_fprintf(FILE *stream, const char *format, ...);

/**
 * Colorful printing of an argument list in a file.
 *
 * To use colors, use the string constants OS_COLOR_xxx.
 *
 * @param[in] stream  File to print to
 * @param[in] format  Format for printing the arguments
 * @param[in] ap      Argument list
 *
 * @return Same as Linux's vfprintf().
 */
int os_colorful_vfprintf(FILE *stream, const char *format, va_list ap);

/* FIXME WIN32  os_ prefixes would be better for consistency's sake */
#ifdef WIN32
#define popen _popen
#define pclose _pclose
#define setbuffer(stream, buf, size) \
    setvbuf((stream), (buf), (buf) ? _IOFBF : _IONBF, (size))
#endif

#ifdef __cplusplus
}
#endif

#endif /* __OS_STDIO_H */
