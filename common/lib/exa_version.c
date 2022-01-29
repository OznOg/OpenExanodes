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
#include <stdio.h>

/* XXX Check that all non-dots are digits? */

bool exa_version_is_major(const exa_version_t *str)
{
    if (str != NULL)
    {
        char *first_dot = strchr(str->v, '.');
        char *last_dot  = strrchr(str->v, '.');

        return first_dot != NULL && first_dot == last_dot;
    }

    return false;
}

bool exa_version_get_major(const exa_version_t *version, exa_version_t *major)
{
    char *dot;

    if (version == NULL || major == NULL)
        return false;

    dot = strchr(version->v, '.');
    if (dot == NULL)
        return false;

    dot = strchr(dot + 1, '.');
    if (dot == NULL)
        strlcpy(major->v, version->v, sizeof(major->v));
    else
    {
        size_t len = dot - version->v;

        memcpy(major->v, version->v, len);
        major->v[len] = '\0';
    }

    return true;
}

void exa_version_copy(exa_version_t *version_dest, const exa_version_t *version_src)
{
    if (version_dest == NULL || version_src == NULL)
        return;
    memcpy(version_dest, version_src, sizeof(*version_dest));
}

bool exa_version_is_equal(const exa_version_t *vers1, const exa_version_t *vers2)
{
    return strcmp(vers1->v, vers2->v) == 0;
}

int exa_version_from_str(exa_version_t *version, const char *src)
{
    EXA_ASSERT(src != NULL && version != NULL);

    if (strnlen(src, sizeof(exa_version_t)) >= sizeof(exa_version_t))
        return -EXA_ERR_VERSION;

    snprintf(version->v, sizeof(version->v), "%s", src);

    return EXA_SUCCESS;
}
