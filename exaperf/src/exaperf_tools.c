/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <ctype.h>

#include "exaperf/src/exaperf_tools.h"

void remove_whitespace(char *str)
{
    unsigned space_count = 0;
    unsigned normal_count = 0;
    char *src, *dest;

    dest = src = str;
    while (*src)
    {
        if (isspace(*src))
            space_count++;
        else
        {
            if (space_count > 0 && normal_count > 0)
                *dest++ = ' ';
            *dest++ = *src;
            normal_count++;
            space_count = 0;
        }

        src++;
    }

    *dest = '\0';
}

const char *last_character(const char *str)
{
    const char *ptr_in = str;
    const char *last = str;

    while (*ptr_in != '\0')
    {
	last = ptr_in;
	ptr_in++;
    }
    return last;
}
