/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "common/include/exa_version.h"
#include "os/include/strlcpy.h"

#include <ctype.h>

/* XXX Check that all non-dots are digits? */

bool exa_version_is_major(const exa_version_t str)
{
    if (str != NULL)
    {
        char *first_dot = strchr(str, '.');
        char *last_dot  = strrchr(str, '.');

        return first_dot != NULL && first_dot == last_dot;
    }

    return false;
}

bool exa_version_get_major(const exa_version_t version, exa_version_t major)
{
    char *dot;

    if (version == NULL || major == NULL)
        return false;

    dot = strchr(version, '.');
    if (dot == NULL)
        return false;

    dot = strchr(dot + 1, '.');
    if (dot == NULL)
        strlcpy(major, version, sizeof(exa_version_t));
    else
    {
        size_t len = dot - version;

        memcpy(major, version, len);
        major[len] = '\0';
    }

    return true;
}

void exa_version_copy(exa_version_t version_dest, const exa_version_t version_src)
{
    strlcpy(version_dest, version_src, sizeof(exa_version_t));
}

bool exa_version_is_equal(const exa_version_t vers1, const exa_version_t vers2)
{
    return strcmp(vers1, vers2) == 0;
}

int exa_version_from_str(exa_version_t version, const char *src)
{
    EXA_ASSERT(src != NULL);

    if (strnlen(src, sizeof(exa_version_t)) >= sizeof(exa_version_t))
        return -EXA_ERR_VERSION;

    exa_version_copy(version, src);

    return EXA_SUCCESS;
}
