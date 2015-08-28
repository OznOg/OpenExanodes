/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_string.h"

char *os_strtok(char *str, const char *delim, char **saveptr)
{
    if (delim == NULL || saveptr == NULL)
        return NULL;

    return strtok_r(str, delim, saveptr);
}

int os_strcasecmp(const char *str1, const char *str2)
{
    return strcasecmp(str1, str2);
}

int os_strverscmp(const char *s1, const char *s2)
{
    return strverscmp(s1, s2);
}
