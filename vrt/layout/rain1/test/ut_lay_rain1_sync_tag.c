/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>

#include <unit_testing.h>

#include "common/include/exa_assert.h"

#include "vrt/layout/rain1/src/lay_rain1_sync_tag.h"

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

UT_SECTION(sync_tag_is_equal)

ut_test(equal_on_same_tags_gives_true)
{
    UT_ASSERT(sync_tag_is_equal(SYNC_TAG_BLANK, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tag_is_equal(SYNC_TAG_ZERO, SYNC_TAG_ZERO));
    UT_ASSERT(sync_tag_is_equal(SYNC_TAG_MAX, SYNC_TAG_MAX));
    UT_ASSERT(sync_tag_is_equal(42, 42));
}

ut_test(equal_on_different_tags_gives_false)
{
    UT_ASSERT(!sync_tag_is_equal(SYNC_TAG_BLANK, SYNC_TAG_ZERO));
    UT_ASSERT(!sync_tag_is_equal(SYNC_TAG_BLANK, SYNC_TAG_MAX));
    UT_ASSERT(!sync_tag_is_equal(SYNC_TAG_BLANK, 42));

    UT_ASSERT(!sync_tag_is_equal(SYNC_TAG_ZERO, SYNC_TAG_BLANK));
    UT_ASSERT(!sync_tag_is_equal(SYNC_TAG_ZERO, SYNC_TAG_MAX));
    UT_ASSERT(!sync_tag_is_equal(SYNC_TAG_ZERO, 42));

    UT_ASSERT(!sync_tag_is_equal(SYNC_TAG_MAX, SYNC_TAG_BLANK));
    UT_ASSERT(!sync_tag_is_equal(SYNC_TAG_MAX, SYNC_TAG_ZERO));
    UT_ASSERT(!sync_tag_is_equal(SYNC_TAG_MAX, 42));

    UT_ASSERT(!sync_tag_is_equal(42, SYNC_TAG_BLANK));
    UT_ASSERT(!sync_tag_is_equal(42, SYNC_TAG_ZERO));
    UT_ASSERT(!sync_tag_is_equal(42, SYNC_TAG_MAX));
}

UT_SECTION(sync_tags_are_comparable)

ut_test(blank_is_comparable_with_everything)
{
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_ZERO, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_MAX, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tags_are_comparable(42, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_LAST - 42, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_LAST, SYNC_TAG_BLANK));

    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_BLANK, SYNC_TAG_ZERO));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_BLANK, SYNC_TAG_MAX));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_BLANK, 42));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_BLANK, SYNC_TAG_LAST - 42));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_BLANK, SYNC_TAG_LAST));
}

ut_test(max_is_comparable_with_everything)
{
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_MAX, SYNC_TAG_ZERO));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_MAX, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_MAX, 42));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_MAX, SYNC_TAG_LAST - 42));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_MAX, SYNC_TAG_LAST));

    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_ZERO, SYNC_TAG_MAX));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_BLANK, SYNC_TAG_MAX));
    UT_ASSERT(sync_tags_are_comparable(42, SYNC_TAG_MAX));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_LAST - 42, SYNC_TAG_MAX));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_LAST, SYNC_TAG_MAX));
}

ut_test(tags_comparable_outside_grey_zone)
{
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_LAST, SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF    ));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_LAST, SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF - 1));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_ZERO, SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF    ));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_ZERO, SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF + 1));

    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF    , SYNC_TAG_LAST));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF - 1, SYNC_TAG_LAST));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF    , SYNC_TAG_ZERO));
    UT_ASSERT(sync_tags_are_comparable(SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF + 1, SYNC_TAG_ZERO));
}

ut_test(tags_not_comparable_in_grey_zone)
{
    UT_ASSERT(!sync_tags_are_comparable(SYNC_TAG_LAST, SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF + 1));
    UT_ASSERT(!sync_tags_are_comparable(SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF + 1, SYNC_TAG_LAST));

    UT_ASSERT(!sync_tags_are_comparable(SYNC_TAG_ZERO, SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF - 1));
    UT_ASSERT(!sync_tags_are_comparable(SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF - 1, SYNC_TAG_ZERO));
}

UT_SECTION(sync_tag_is_greater)

ut_test(greater_on_same_tags_gives_false)
{
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_BLANK, SYNC_TAG_BLANK));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_ZERO, SYNC_TAG_ZERO));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_MAX, SYNC_TAG_MAX));
    UT_ASSERT(!sync_tag_is_greater(42, 42));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST - 42, SYNC_TAG_LAST - 42));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST, SYNC_TAG_LAST));
}

ut_test(everything_is_greater_than_blank)
{
    UT_ASSERT(sync_tag_is_greater(SYNC_TAG_ZERO, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tag_is_greater(SYNC_TAG_MAX, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tag_is_greater(42, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tag_is_greater(SYNC_TAG_LAST - 42, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tag_is_greater(SYNC_TAG_LAST, SYNC_TAG_BLANK));

    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_BLANK, SYNC_TAG_ZERO));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_BLANK, SYNC_TAG_MAX));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_BLANK, 42));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_BLANK, SYNC_TAG_LAST - 42));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_BLANK, SYNC_TAG_LAST));
}

ut_test(max_is_greater_than_everything)
{
    UT_ASSERT(sync_tag_is_greater(SYNC_TAG_MAX, SYNC_TAG_ZERO));
    UT_ASSERT(sync_tag_is_greater(SYNC_TAG_MAX, SYNC_TAG_BLANK));
    UT_ASSERT(sync_tag_is_greater(SYNC_TAG_MAX, 42));
    UT_ASSERT(sync_tag_is_greater(SYNC_TAG_MAX, SYNC_TAG_LAST - 42));
    UT_ASSERT(sync_tag_is_greater(SYNC_TAG_MAX, SYNC_TAG_LAST));

    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_ZERO, SYNC_TAG_MAX));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_BLANK, SYNC_TAG_MAX));
    UT_ASSERT(!sync_tag_is_greater(42, SYNC_TAG_MAX));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST - 42, SYNC_TAG_MAX));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST, SYNC_TAG_MAX));
}

ut_test(greater_with_zero)
{
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_ZERO, SYNC_TAG_BLANK        ));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_ZERO, 42                    ));
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_ZERO, SYNC_TAG_LAST - 42    ));
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_ZERO, SYNC_TAG_LAST         ));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_ZERO, SYNC_TAG_MAX          ));

    UT_ASSERT( sync_tag_is_greater(42                    , SYNC_TAG_ZERO));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST - 42    , SYNC_TAG_ZERO));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST         , SYNC_TAG_ZERO));
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_MAX          , SYNC_TAG_ZERO));
}

ut_test(greater_with_last)
{
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST, SYNC_TAG_ZERO     ));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST, 42                ));
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_LAST, SYNC_TAG_LAST - 42));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST, SYNC_TAG_MAX      ));

    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_ZERO     , SYNC_TAG_LAST));
    UT_ASSERT( sync_tag_is_greater(42                , SYNC_TAG_LAST));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST - 42, SYNC_TAG_LAST));
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_MAX      , SYNC_TAG_LAST));
}

ut_test(greater_works_at_grey_zone_limits)
{
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_LAST, SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF    ));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST, SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF - 1));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_ZERO, SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF    ));
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_ZERO, SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF + 1));

    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF    , SYNC_TAG_LAST));
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF - 1, SYNC_TAG_LAST));
    UT_ASSERT( sync_tag_is_greater(SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF    , SYNC_TAG_ZERO));
    UT_ASSERT(!sync_tag_is_greater(SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF + 1, SYNC_TAG_ZERO));
}

UT_SECTION(sync_tag_inc)

ut_test(inc_blank_gives_zero)
{
    UT_ASSERT(sync_tag_is_equal(SYNC_TAG_ZERO, sync_tag_inc(SYNC_TAG_BLANK)));
}

ut_test(inc_max_stays_max)
{
    UT_ASSERT(sync_tag_is_equal(SYNC_TAG_MAX, sync_tag_inc(SYNC_TAG_MAX)));
}

ut_test(inc_last_wrap_to_zero)
{
    UT_ASSERT(sync_tag_is_equal(SYNC_TAG_ZERO, sync_tag_inc(SYNC_TAG_LAST)));
}

ut_test(inc_is_greater_than_initial)
{
    sync_tag_t tag1 = SYNC_TAG_ZERO;

    while (!sync_tag_is_equal(tag1, SYNC_TAG_LAST))
    {
        sync_tag_t old_tag1 = tag1;

        tag1 = sync_tag_inc(tag1);

        UT_ASSERT(!sync_tag_is_equal(tag1, old_tag1));
        UT_ASSERT(sync_tag_is_greater(tag1, old_tag1));
        UT_ASSERT(!sync_tag_is_greater(old_tag1, tag1));
    }
}

ut_test(inc_two_tags_maintain_greater_relation_1)
{
    sync_tag_t tag1 = SYNC_TAG_ZERO;
    sync_tag_t tag2 = SYNC_TAG_ZERO + SYNC_TAG_MAX_DIFF;

    do {
        UT_ASSERT(!sync_tag_is_equal(tag1, tag2));

        UT_ASSERT( sync_tag_is_greater(tag2, tag1));
        UT_ASSERT(!sync_tag_is_greater(tag1, tag2));

        tag1 = sync_tag_inc(tag1);
        tag2 = sync_tag_inc(tag2);
    } while (!sync_tag_is_equal(tag1, SYNC_TAG_ZERO));
}

ut_test(inc_two_tags_maintain_greater_relation_2)
{
    sync_tag_t tag1 = SYNC_TAG_LAST;
    sync_tag_t tag3 = SYNC_TAG_LAST - SYNC_TAG_MAX_DIFF;

    do {
        UT_ASSERT(!sync_tag_is_equal(tag1, tag3));

        UT_ASSERT(!sync_tag_is_greater(tag3, tag1));
        UT_ASSERT( sync_tag_is_greater(tag1, tag3));

        tag1 = sync_tag_inc(tag1);
        tag3 = sync_tag_inc(tag3);
    } while (!sync_tag_is_equal(tag1, SYNC_TAG_LAST));
}

UT_SECTION(operator_consistence)

ut_test(consistence_with_ref_in_interval)
{
    sync_tag_t tag_ref = 42;
    sync_tag_t tag = tag_ref;

    do {
        if (sync_tags_are_comparable(tag, tag_ref))
        {
            UT_ASSERT(sync_tags_are_comparable(tag_ref, tag));

            if (sync_tag_is_equal(tag, tag_ref))
            {
                UT_ASSERT(!sync_tag_is_greater(tag, tag_ref));
                UT_ASSERT(!sync_tag_is_greater(tag_ref, tag));
            }
            else if (sync_tag_is_greater(tag, tag_ref))
                UT_ASSERT(!sync_tag_is_greater(tag_ref, tag));
            else
                UT_ASSERT(sync_tag_is_greater(tag_ref, tag));
        }

        tag = sync_tag_inc(tag);
    } while (!sync_tag_is_equal(tag, tag_ref));
}

ut_test(consistence_with_zero)
{
    sync_tag_t tag_ref = SYNC_TAG_ZERO;
    sync_tag_t tag = tag_ref;

    do {
        if (sync_tags_are_comparable(tag, tag_ref))
        {
            UT_ASSERT(sync_tags_are_comparable(tag_ref, tag));

            if (sync_tag_is_equal(tag, tag_ref))
            {
                UT_ASSERT(!sync_tag_is_greater(tag, tag_ref));
                UT_ASSERT(!sync_tag_is_greater(tag_ref, tag));
            }
            else if (sync_tag_is_greater(tag, tag_ref))
                UT_ASSERT(!sync_tag_is_greater(tag_ref, tag));
            else
                UT_ASSERT(sync_tag_is_greater(tag_ref, tag));
        }

        tag = sync_tag_inc(tag);
    } while (!sync_tag_is_equal(tag, tag_ref));
}

ut_test(consistence_with_last)
{
    sync_tag_t tag_ref = SYNC_TAG_LAST;
    sync_tag_t tag = tag_ref;

    do {
        if (sync_tags_are_comparable(tag, tag_ref))
        {
            UT_ASSERT(sync_tags_are_comparable(tag_ref, tag));

            if (sync_tag_is_equal(tag, tag_ref))
            {
                UT_ASSERT(!sync_tag_is_greater(tag, tag_ref));
                UT_ASSERT(!sync_tag_is_greater(tag_ref, tag));
            }
            else if (sync_tag_is_greater(tag, tag_ref))
                UT_ASSERT(!sync_tag_is_greater(tag_ref, tag));
            else
                UT_ASSERT(sync_tag_is_greater(tag_ref, tag));
        }

        tag = sync_tag_inc(tag);
    } while (!sync_tag_is_equal(tag, tag_ref));
}

