/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "target/iscsi/include/iqn.h"
#include "common/include/exa_assert.h"
#include "os/include/os_string.h"
#include "os/include/os_stdio.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int iqn_set(iqn_t *iqn, const char *format, ...)
{
    va_list args;
    size_t size;
    int i;

    if (iqn == NULL || format == NULL)
        return -EINVAL;

    va_start(args, format);
    size = os_vsnprintf(iqn->value, IQN_MAX_LEN + 1, format, args);
    va_end(args);

    for (i = 0; i < strlen(iqn->value); i++)
        iqn->value[i] = tolower(iqn->value[i]);

    if (size >= IQN_MAX_LEN || !IQN_IS_VALID(iqn))
        return -EINVAL;

    return EXA_SUCCESS;
}

int iqn_from_str(iqn_t *iqn, const char *str)
{
    int i;

    if (iqn == NULL || str == NULL)
        return -EINVAL;

    if (!IQN_STR_IS_VALID(str) && !IQN_STR_IS_PATTERN(str))
        return -EINVAL;

    os_strlcpy(iqn->value, str, IQN_MAX_LEN + 1);

    for (i = 0; i < strlen(iqn->value); i++)
        iqn->value[i] = tolower(iqn->value[i]);

    return EXA_SUCCESS;
}

const char *iqn_to_str(const iqn_t *iqn)
{
    static __thread char str[IQN_MAX_LEN + 1];

    if (iqn == NULL)
        return NULL;

    os_strlcpy(str, iqn->value, IQN_MAX_LEN + 1);

    return str;
}

bool iqn_is_equal(const iqn_t *iqn1, const iqn_t *iqn2)
{
    return strncmp(iqn1->value, iqn2->value, IQN_MAX_LEN + 1) == 0;
}

int iqn_compare(const iqn_t *iqn1, const iqn_t *iqn2)
{
    return strncmp(iqn1->value, iqn2->value, IQN_MAX_LEN + 1);
}

void iqn_copy(iqn_t *dest, const iqn_t *src)
{
    if (dest == NULL || src == NULL)
        return;

    if (!IQN_IS_VALID(src) && !IQN_IS_PATTERN(src))
        return;

    os_strlcpy(dest->value, src->value, IQN_MAX_LEN + 1);
}

bool iqn_matches(const iqn_t *iqn, const iqn_t *pattern)
{
    char *wildcard_pos;
    char start[IQN_MAX_LEN + 1];
    char end[IQN_MAX_LEN + 1];
    char *end_offset;
    bool start_match;
    bool end_match;

    EXA_ASSERT(IQN_IS_VALID(iqn));
    EXA_ASSERT(IQN_IS_VALID(pattern) || IQN_IS_PATTERN(pattern));

    wildcard_pos = strchr(pattern->value, '*');
    if (wildcard_pos == NULL)
        return iqn_is_equal(iqn, pattern);

    /* At most one wildcard is supported */
    EXA_ASSERT(strrchr(pattern->value, '*') == wildcard_pos);

    /* Separate the IQN pattern at the wildcard */
    os_strlcpy(start, pattern->value, sizeof(start));
    os_strlcpy(end, wildcard_pos + 1, sizeof(end));
    *strchr(start, '*') = '\0';

    /* The start match is easy, strncmp does what we want */
    start_match = strncmp(start, iqn->value, strlen(start)) == 0;

    /* For the end of the match, we'll verify that the substring
     * starting with the end part of the wildcard is of the same
     * length as the end part of the wildcard. Of course, if the
     * end part of the wildcard doesn't even appear in the IQN,
     * it can't match. Also, if end is empty, it matches.
     */
    end_offset = strstr(iqn->value, end);
    end_match = end[0] == '\0' || end_offset == NULL
                || strlen(end_offset) == strlen(end);

    return start_match && end_match;
}
