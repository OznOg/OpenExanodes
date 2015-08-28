/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "target/iscsi/include/lun.h"
#include "os/include/os_stdio.h"

UT_SECTION(lun_from_str)

ut_test(parse_null_returns_LUN_NONE)
{
    UT_ASSERT_EQUAL(LUN_NONE, lun_from_str(NULL));
}

ut_test(parse_empty_string_returns_LUN_NONE_errno_EINVAL)
{
    UT_ASSERT_EQUAL(LUN_NONE, lun_from_str(""));
}

ut_test(parse_negative_number_returns_LUN_NONE_errno_EINVAL)
{
    UT_ASSERT_EQUAL(LUN_NONE, lun_from_str("-10"));
}

ut_test(parse_non_numeric_returns_LUN_NONE_errno_EINVAL)
{
    UT_ASSERT_EQUAL(LUN_NONE, lun_from_str("hello12345"));
}

ut_test(parse_out_of_range_lun_returns_LUN_NONE_errno_ERANGE)
{
    char str[16];

    /* Just one above the upper bound */
    os_snprintf(str, sizeof(str), "%d", MAX_LUNS);
    UT_ASSERT_EQUAL(LUN_NONE, lun_from_str(str));

    /* Ensure the special LUN_NONE case is not ok */
    os_snprintf(str, sizeof(str), "%d", LUN_NONE);
    UT_ASSERT_EQUAL(LUN_NONE, lun_from_str(str));
}

ut_test(parse_valid_string_returns_valid_lun)
{
    typedef struct {
        char *str;
        lun_t lun;
    } lun_entry_t;

    lun_entry_t t[] = {
        { "0", 0 },
        { "1", 1 },
        { "127", 127 },
        { "234", 234 },
        { NULL, 0 }
    };
    int i;
    char highest_lun_str[LUN_STR_LEN + 1];

    for (i = 0; t[i].str != NULL; i++)
        UT_ASSERT_EQUAL(t[i].lun, lun_from_str(t[i].str));

    UT_ASSERT(os_snprintf(highest_lun_str, sizeof(highest_lun_str), "%d", MAX_LUNS - 1)
              < sizeof(highest_lun_str));
    UT_ASSERT_EQUAL(MAX_LUNS - 1, lun_from_str(highest_lun_str));
}

UT_SECTION(lun_to_str)

ut_test(out_of_range_lun_returns_NULL_errno_ERANGE)
{
    UT_ASSERT(lun_to_str(MAX_LUNS) == NULL);
    UT_ASSERT(lun_to_str(LUN_NONE) == NULL);
}

ut_test(valid_lun_returns_valid_string)
{
    const char *str;
    char highest_lun_str[LUN_STR_LEN + 1];

    str = lun_to_str(0);
    UT_ASSERT(str != NULL && strcmp(str, "0") == 0);

    UT_ASSERT(os_snprintf(highest_lun_str, sizeof(highest_lun_str), "%d", MAX_LUNS - 1)
              < sizeof(highest_lun_str));
    str = lun_to_str(MAX_LUNS - 1);
    UT_ASSERT(str != NULL && strcmp(str, highest_lun_str) == 0);
}

ut_test(bigendian)
{
    unsigned char buffer[8];
    lun_t lun = 8;

    /* check with LUN under 255 (coded on 1 byte) */
    lun_set_bigendian(lun, buffer);

    UT_ASSERT(buffer[0] == 0);
    UT_ASSERT(buffer[1] == 8);
    UT_ASSERT(buffer[2] == 0);
    UT_ASSERT(buffer[3] == 0);
    UT_ASSERT(buffer[4] == 0);
    UT_ASSERT(buffer[5] == 0);
    UT_ASSERT(buffer[6] == 0);
    UT_ASSERT(buffer[7] == 0);

    /* check with LUN over 255 (coded on 2 bytes) */
    lun = lun_get_bigendian(buffer);

    UT_ASSERT(lun == 8);

    lun = 520;

    lun_set_bigendian(lun, buffer);

    UT_ASSERT(buffer[0] == 2);
    UT_ASSERT(buffer[1] == 8);
    UT_ASSERT(buffer[2] == 0);
    UT_ASSERT(buffer[3] == 0);
    UT_ASSERT(buffer[4] == 0);
    UT_ASSERT(buffer[5] == 0);
    UT_ASSERT(buffer[6] == 0);
    UT_ASSERT(buffer[7] == 0);

    lun = lun_get_bigendian(buffer);

    UT_ASSERT(lun == 520);

    /* check with LUN_NONE */
    lun = LUN_NONE;

    lun_set_bigendian(lun, buffer);
    lun = lun_get_bigendian(buffer);

    UT_ASSERT(lun == LUN_NONE);
}
