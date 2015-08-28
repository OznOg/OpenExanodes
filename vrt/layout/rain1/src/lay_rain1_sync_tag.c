/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/layout/rain1/src/lay_rain1_sync_tag.h"

#include "common/include/exa_math.h"
#include "common/include/exa_assert.h"

/**
 * Gives the absolute distance between two tags.
 *
 * It is the minimum of the distance through direct path and the distance
 * through wrapped path.
 *
 * @param[in] tag1   first tag
 * @param[in] tag2   second tag
 *
 * @return the absolute distance
 */
static uint16_t sync_tags_dist(sync_tag_t tag1, sync_tag_t tag2)
{
    if (tag1 > tag2)
        return MIN(tag1 - tag2, (tag2 + SYNC_TAG_INTERVAL) - tag1);
    else
        return MIN(tag2 - tag1, (tag1 + SYNC_TAG_INTERVAL) - tag2);
}

/**
 * Gives the difference between two tags.
 *
 * The difference corresponds to the distance:
 *    sync_tags_dist(tag1, tag2) == ABS(sync_tags_diff(tag1, tag2))
 *
 * @param[in] tag1   first tag
 * @param[in] tag2   second tag
 *
 * @return the absolute distance
 */
static int32_t sync_tags_diff(sync_tag_t tag1, sync_tag_t tag2)
{
    if (tag1 > tag2)
        if (tag1 - tag2 < (tag2 + SYNC_TAG_INTERVAL) - tag1)
            return (int32_t) tag1 - tag2;
        else
            return (int32_t) tag1 - tag2 - SYNC_TAG_INTERVAL;
    else
        if (tag2 - tag1 < (tag1 + SYNC_TAG_INTERVAL) - tag2)
            return (int32_t) tag1 - tag2;
        else
            return (int32_t) tag1 - tag2 + SYNC_TAG_INTERVAL;
}

bool sync_tags_are_comparable(sync_tag_t tag1, sync_tag_t tag2)
{
    if (tag1 == SYNC_TAG_BLANK || tag2 == SYNC_TAG_BLANK
        || tag1 == SYNC_TAG_MAX || tag2 == SYNC_TAG_MAX)
        return true;
    else
        return sync_tags_dist(tag1, tag2) <= SYNC_TAG_MAX_DIFF;
}

bool sync_tag_is_equal(sync_tag_t tag1, sync_tag_t tag2)
{
    return tag1 == tag2;
}

bool sync_tag_is_greater(sync_tag_t tag1, sync_tag_t tag2)
{
    EXA_ASSERT(sync_tags_are_comparable(tag1, tag2));

    if (tag1 == tag2)
        return false;
    else if (tag1 == SYNC_TAG_BLANK || tag2 == SYNC_TAG_MAX)
        return false;
    else if (tag1 == SYNC_TAG_MAX || tag2 == SYNC_TAG_BLANK)
        return true;
    else
        return sync_tags_diff(tag1, tag2) > 0;
}

sync_tag_t sync_tag_max(sync_tag_t tag1, sync_tag_t tag2)
{
    return sync_tag_is_greater(tag1, tag2) ? tag1 : tag2;
}

sync_tag_t sync_tag_inc(sync_tag_t tag)
{
    /* We rotate the values betwin SYNC_TAG_ZERO and SYNC_TAG_LAST */

    if (tag == SYNC_TAG_MAX)
        return SYNC_TAG_MAX;
    else if (tag == SYNC_TAG_BLANK)
        return SYNC_TAG_ZERO;
    else if (tag == SYNC_TAG_LAST)
        return SYNC_TAG_ZERO;
    else
        return tag + 1;
}

