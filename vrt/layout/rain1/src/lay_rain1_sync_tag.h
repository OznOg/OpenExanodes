/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __RAIN1_SYNC_TAG_H__
#define __RAIN1_SYNC_TAG_H__

#include "os/include/os_inttypes.h"

typedef uint16_t sync_tag_t;

#define PRIsync_tag  PRIu16

#define SYNC_TAG_MAX   UINT16_MAX
#define SYNC_TAG_ZERO  1
#define SYNC_TAG_BLANK 0

/* Private defines
 * they appear in the header only for the unit testing
 */
#define SYNC_TAG_LAST      (SYNC_TAG_MAX - 1)
#define SYNC_TAG_INTERVAL  (SYNC_TAG_LAST - SYNC_TAG_ZERO + 1)
#define SYNC_TAG_GREY_ZONE 100
#define SYNC_TAG_MAX_DIFF  ((SYNC_TAG_INTERVAL - SYNC_TAG_GREY_ZONE) / 2)

/**
 * Equal operator between two synchonization tags
 *
 * @param[in] tag1   first tag
 * @param[in] tag2   second tag
 *
 * @return true if tag1 is equal to tag2
 */
bool sync_tag_is_equal(sync_tag_t tag1, sync_tag_t tag2);

/**
 * Strict comparison operator between two synchonization tags
 *
 * @param[in] tag1   first tag
 * @param[in] tag2   second tag
 *
 * specific cases:
 *  - SYNC_TAG_BLANK is smaller than every other tag
 *  - SYNC_TAG_MAX is greater than every other tag
 *
 * @return true if tag1 is strictly greater than tag2
 */
bool sync_tag_is_greater(sync_tag_t tag1, sync_tag_t tag2);

/**
 * Max operator
 *
 * @param[in] tag1   first tag
 * @param[in] tag2   second tag
 *
 * @return the greatest tag
 */
sync_tag_t sync_tag_max(sync_tag_t tag1, sync_tag_t tag2);

/**
 * Increment operator with rotation mechanism
 *
 * specific cases:
 *  - sync_tag_inc(SYNC_TAG_BLANK) returns SYNC_TAG_ZERO
 *  - sync_tag_inc(SYNC_TAG_MAX) returns SYNC_TAG_ZERO
 *
 * @param[in] tag   the initial tag
 *
 * @return the incremented value
 */
sync_tag_t sync_tag_inc(sync_tag_t tag);

/**
 * Tells if two tags are comparable with sync_tag_is_greater
 *
 * @param[in] tag1   first tag
 * @param[in] tag2   second tag
 *
 * specific cases:
 *  - SYNC_TAG_BLANK is comparable to all the tags
 *  - SYNC_TAG_MAX is comparable to all the tags
 *
 * @return true if one can compare tag1 to tag2 and tag2 to tag1
 */
bool sync_tags_are_comparable(sync_tag_t tag1, sync_tag_t tag2);

#endif
