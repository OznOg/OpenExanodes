/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/*
 * This file intend to add missing fucntionalities of the win32 string.h
 */
#ifndef _OS_STRING_H
#define _OS_STRING_H

#include <string.h>

#ifdef WIN32
/* isblank is not defined on Windows */
#define isblank(c)  ((c) == ' ' || (c) == '\t')
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Find the next token in a string
 *
 * @param str      The string containing tokens
 * @param delim    The set of delimiter characters
 * @param saveptr  Used to store position information between calls to os_strtok
 *
 * @return A pointer to the token found, NULL otherwise
 *
 * @os_replace{Linux, strtok, strtok_r}
 * @os_replace{Windows, strtok, strtok_s}
 */
char *os_strtok(char *str, const char *delim, char **saveptr);

/**
 * Case-insensitive strings comparison.
 *
 * @param[in] str1  Non-null string
 * @param[in] str2  Non-null string
 *
 * @return < 0 if str1 < str2, 0 if str1 == str2, > 0 if str1 > str2
 *
 * @os_replace{Linux, strcasecmp}
 * @os_replace{Windows, _stricmp, strcmpi}
 */
int os_strcasecmp(const char *str1, const char *str2);

/**
 * Compare two version strings
 *
 * @param s1  The first string to compare
 * @param s2  The second string to compare
 *
 * @return An integer less than, equal to, or greater than zero if s1 is found,
 *         respectively, to be earlier than, equal to, or later than s2
 */
int os_strverscmp(const char *s1, const char *s2);

#ifdef WIN32
/* FIXME WIN32
 * We should probably wrap this function call and add a unit test to make
 * sure le behaviour under linux and windows is REALLY the same... */
#define strcasecmp lstrcmpi
#endif /* WIN32 */

/**
 * Trim blanks (spaces and tabs) on the left of a string.
 *
 * @param[in,out] str  String to trim
 *
 * @return str (trimed)
 *
 */
char *os_str_trim_left(char *str);

/**
 * Trim blanks (spaces and tabs) on the right of a string.
 *
 * @param[in,out] str  String to trim
 *
 * @return str (trimmed)
 */
char *os_str_trim_right(char *str);

/**
 * Trim blanks (spaces and tabs) on the left and right of a string.
 *
 * @param[in,out] str  String to trim
 *
 * @return str (trimmed)
 */
char *os_str_trim(char *str);

#ifdef WIN32
char *strndup(const char *s, size_t n);
#endif

/**
 * Copy a string to another.
 *
 * @param[out] dst  Destination string
 * @param[in]  src  Source string
 * @param[in]  siz  Destination string size
 *
 * At most siz-1 characters are copied. The resulting string is always null
 * terminated (unless siz == 0).
 *
 * @return strlen(src); if the returned value is >= siz, truncation occurred.
 *
 * @os_replace{Linux, strcpy, strncpy}
 * @os_replace{Windows, strcpy, strncpy}
 */
size_t os_strlcpy(char *dst, const char *src, size_t siz);

#ifdef __cplusplus
}
#endif

#endif /* _OS_STRING_H */

