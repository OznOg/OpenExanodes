/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** @file
 *  @brief string formatting routines
 */

#include "os/include/os_string.h"

#include <ctype.h>

char *os_str_trim_left(char *str)
{
    size_t len;
    size_t i = 0, n;

    if (str == NULL)
        return NULL;

    len = strlen(str);
    while (i < len && isblank(str[i]))
        i++;
    n = len - i;
    if (n == 0)
        str[0] = '\0';
    else if (i > 0)
    {
        memmove(str, str + i, n);
        str[n] = '\0';
    }

    return str;
}

char *os_str_trim_right(char *str)
{
    size_t len;

    if (str == NULL)
        return NULL;

    len = strlen(str);
    while (len > 0 && isblank(str[len - 1]))
        len--;
    str[len] = '\0';

    return str;
}

char *os_str_trim(char *str)
{
    return os_str_trim_left(os_str_trim_right(str));
}

size_t os_strlcpy(char *dst, const char *src, size_t siz)
{
    char *d = dst;
    const char *s = src;
    size_t n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0 && --n != 0)
        do
        {
            if ((*d++ = *s++) == 0)
                break;
        } while (--n != 0);

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0)
    {
        if (siz != 0)
            *d = '\0'; /* NUL-terminate dst */
        while (*s++)
            ;
    }

    return s - src - 1; /* count does not include null */
}

