/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include "os/include/os_inttypes.h"
#include "os/include/os_stdio.h"

UT_SECTION(signed_ints)

ut_test(test_format_PRId8)
{
    char s[10];
    int8_t h = -17;
    os_snprintf(s, sizeof(s), "%"PRId8, h);
    UT_ASSERT(!strcmp(s, "-17"));
}

ut_test(test_format_PRId16)
{
    char s[10];
    int16_t h = -1700;
    os_snprintf(s, sizeof(s), "%"PRId16, h);
    UT_ASSERT(!strcmp(s, "-1700"));
}

ut_test(test_format_PRId32)
{
    char s[10];
    int32_t h = -1700000;
    os_snprintf(s, sizeof(s), "%"PRId32, h);
    UT_ASSERT(!strcmp(s, "-1700000"));
}

ut_test(test_format_prid64)
{
    char s[15];
    int64_t h = int64_t_C(-170000000000);
    os_snprintf(s, sizeof(s), "%"PRId64, h);
    UT_ASSERT(!strcmp(s, "-170000000000"));
}


UT_SECTION(unsigned_ints)

ut_test(test_format_PRIu8)
{
    char s[10];
    uint8_t h = 255;
    os_snprintf(s, sizeof(s), "%"PRIu8, h);
    UT_ASSERT(!strcmp(s, "255"));
}

ut_test(test_format_PRIu16)
{
    char s[10];
    uint16_t h = 65535;
    os_snprintf(s, sizeof(s), "%"PRIu16, h);
    UT_ASSERT(!strcmp(s, "65535"));
}

ut_test(test_format_PRIu32)
{
    char s[10];
    uint32_t h = 1700000;
    os_snprintf(s, sizeof(s), "%"PRIu32, h);
    UT_ASSERT(!strcmp(s, "1700000"));
}

ut_test(test_format_PRIu64)
{
    char s[15];
    uint64_t h = uint64_t_C(170000000000);
    os_snprintf(s, sizeof(s), "%"PRIu64, h);
    UT_ASSERT(!strcmp(s, "170000000000"));
}

UT_SECTION(hexa)
ut_test(test_format_)
{
    char s[15];
    uint64_t h = 0x1700000;
    os_snprintf(s, sizeof(s), "0x%08"PRIx64, h);
    UT_ASSERT(!strcmp(s, "0x01700000"));
}

UT_SECTION(sizes)

ut_test(test_format_PRIzu)
{
    char s[10];
    size_t h = 255;
    os_snprintf(s, sizeof(s), "%"PRIzu, h);
    UT_ASSERT(!strcmp(s, "255"));
}

ut_test(test_format_PRIzd)
{
    char s[10];
    ssize_t h = -255;
    ut_printf( "%"PRIzd, h);

    os_snprintf(s, sizeof(s), "%"PRIzd, h);
    UT_ASSERT(!strcmp(s, "-255"));
}

