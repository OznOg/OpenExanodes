/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_conversion.h"

#include <unit_testing.h>
#include <limits.h>
#include <errno.h>

ut_test(to_int64)
{
    int err;
    int64_t val;

    UT_ASSERT_EQUAL(-EINVAL, to_int64(NULL, &val));
    UT_ASSERT_EQUAL(-EINVAL, to_int64("0", NULL));

    err = to_int64("0", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(0, val);

    err = to_int64("452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(452, val);

    err = to_int64("-452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(-452, val);

    err = to_int64("  -452", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int64("\t-452", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int64("\n-452", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    /* MIN */
    err = to_int64("-9223372036854775808", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(INT64_MIN, val); /* -<:o) */

    /* MAX */
    err = to_int64("9223372036854775807", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(INT64_MAX, val);

    /* MAX + 1 */
    err = to_int64("9223372036854775808", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    /* MIN - 1 */
    err = to_int64("-9223372036854775809", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    err = to_int64("nimportenawak", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int64("", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int64("  ", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int64("  -", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);
}

ut_test(to_uint64)
{
    int err;
    uint64_t val;

    UT_ASSERT_EQUAL(-EINVAL, to_uint64(NULL, &val));
    UT_ASSERT_EQUAL(-EINVAL, to_uint64("0", NULL));

    err = to_uint64("0", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(0, val);

    err = to_uint64("452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(452, val);

    err = to_uint64("2147483647", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(2147483647, val);

    err = to_uint64("4294967295", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(4294967295UL, val);

    err = to_uint64("   123", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint64("\t123", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint64("\n123", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    /* MIN - 1 */
    err = to_uint64("-1", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    /* MAX + 1 */
    err = to_uint64("18446744073709551616", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    err = to_uint64("nimportenawak", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint64("", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint64("  ", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint64("  -", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);
}

ut_test(to_int32)
{
    int err;
    int32_t val;

    err = to_int32("0", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(0, val);

    err = to_int32("452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(452, val);

    err = to_int32("-452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(-452, val);

    err = to_int32("  -452", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int32("\t-452", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int32("\n-452", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    /* MIN */
    err = to_int32("-2147483648", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(INT32_MIN, val);

    /* MAX */
    err = to_int32("2147483647", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(2147483647, val);

    /* MAX + 1 */
    err = to_int32("2147483648", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    /* MIN - 1 */
    err = to_int32("-2147483649", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    err = to_int32("nimportenawak", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int32("", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int32("  ", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int32("  -", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);
}


ut_test(to_uint32)
{
    int err;
    uint32_t val;

    err = to_uint32("0", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(0, val);

    err = to_uint32("452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(452, val);

    err = to_uint32("2147483647", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(2147483647, val);

    /* MAX */
    err = to_uint32("4294967295", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(UINT32_MAX, val);

    /* MIN - 1 */
    err = to_uint32("-1", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    /* MAX + 1 */
    err = to_uint32("4294967296", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    err = to_uint32("nimportenawak", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint32("", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint32("  ", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint32("  -", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);
}

ut_test(to_uint16)
{
    int err;
    uint16_t val;

    err = to_uint16("0", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(0, val);

    err = to_uint16("452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(452, val);

    /* MAX */
    err = to_uint16("65535", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(UINT16_MAX, val);

    /* MIN - 1 */
    err = to_uint16("-1", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    /* MAX + 1 */
    err = to_uint16("65536", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    err = to_uint16("nimportenawak", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint16("", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint16("  ", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint16("  -", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);
}

ut_test(to_uint8)
{
    int err;
    uint8_t val;

    err = to_uint8("0", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(0, val);

    err = to_uint8("234", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(234, val);

    /* MAX */
    err = to_uint8("255", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(UINT8_MAX, val);

    /* MIN - 1 */
    err = to_uint8("-1", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    /* MAX + 1 */
    err = to_uint8("256", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    err = to_uint8("nimportenawak", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint8("", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint8("  ", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint8("  -", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);
}

ut_test(to_int)
{
    int err;
    int val;

    err = to_int("0", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(0, val);

    err = to_int("452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(452, val);

    err = to_int("-452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(-452, val);

    err = to_int("  -452", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int("\t-452", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int("\n-452", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    /* MIN */
    err = to_int("-2147483648", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(INT_MIN, val);

    /* MAX */
    err = to_int("2147483647", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(INT_MAX, val);

    /* MAX + 1 */
    err = to_int("2147483648", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    /* MIN - 1 */
    err = to_int("-2147483649", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    err = to_int("nimportenawak", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int("", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int("  ", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_int("  -", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);
}

ut_test(to_uint)
{
    int err;
    unsigned int val;

    err = to_uint("0", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(0, val);

    err = to_uint("452", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(452, val);

    err = to_uint("2147483647", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(2147483647, val);

    /* MAX */
    err = to_uint("4294967295", &val);
    UT_ASSERT_EQUAL(EXA_SUCCESS, err);
    UT_ASSERT_EQUAL(UINT_MAX, val);

    /* MIN - 1 */
    err = to_uint("-1", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    /* MAX + 1 */
    err = to_uint("4294967296", &val);
    UT_ASSERT_EQUAL(-ERANGE, err);

    err = to_uint("nimportenawak", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint("", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint("  ", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);

    err = to_uint("  -", &val);
    UT_ASSERT_EQUAL(-EINVAL, err);
}

ut_test(exa_get_size_kb)
{
    uint64_t size;
    int ret;

    ret = exa_get_size_kb("1K", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1);

    ret = exa_get_size_kb("1023.1234K", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1023);

    ret = exa_get_size_kb("1.6K", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 2);

    ret = exa_get_size_kb("1.5K", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1);

    ret = exa_get_size_kb("1M", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1024);

    ret = exa_get_size_kb("1.5M", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1536);

    ret = exa_get_size_kb("2M", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 2048);

    ret = exa_get_size_kb("1234.56M", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1264189);

    ret = exa_get_size_kb("1G", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1048576);

    ret = exa_get_size_kb("123.56G", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 129562050);

    ret = exa_get_size_kb("1T", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1073741824);

    ret = exa_get_size_kb("0.56T", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 601295421);

    ret = exa_get_size_kb("1P", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1099511627776LL);

    ret = exa_get_size_kb("12.56P", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 13809866044866LL);

    ret = exa_get_size_kb("1E", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 1125899906842624LL);

    ret = exa_get_size_kb("8.56e", &size);
    UT_ASSERT(ret == EXA_SUCCESS && size == 9637703202572862LL);

    ret = exa_get_size_kb("213446343", &size);
    UT_ASSERT(ret == -EINVAL);

    ret = exa_get_size_kb("9999999999999999E", &size);
    UT_ASSERT(ret == -ERANGE);
}

ut_test(exa_get_human_size)
{
    char human_string[256];

    exa_get_human_size(human_string, sizeof(human_string), 1);
    UT_ASSERT_EQUAL_STR("1.00 K",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 1023);
    UT_ASSERT_EQUAL_STR("1023.00 K",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 2);
    UT_ASSERT_EQUAL_STR("2.00 K",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 1);
    UT_ASSERT_EQUAL_STR("1.00 K",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 1024);
    UT_ASSERT_EQUAL_STR("1.00 M",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 1536);
    UT_ASSERT_EQUAL_STR("1.50 M",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 2048);
    UT_ASSERT_EQUAL_STR("2.00 M",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 1264189);
    UT_ASSERT_EQUAL_STR("1.21 G",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 1048576);
    UT_ASSERT_EQUAL_STR("1.00 G",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 129562050);
    UT_ASSERT_EQUAL_STR("123.56 G",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 1073741824);
    UT_ASSERT_EQUAL_STR("1.00 T",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 601295421);
    UT_ASSERT_EQUAL_STR("573.44 G",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 1099511627776LL);
    UT_ASSERT_EQUAL_STR("1.00 P",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 13809866044866LL);
    UT_ASSERT_EQUAL_STR("12.56 P",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 1125899906842624LL);
    UT_ASSERT_EQUAL_STR("1.00 E",  human_string);
    exa_get_human_size(human_string, sizeof(human_string), 9637703202572862LL);
    UT_ASSERT_EQUAL_STR("8.56 E",  human_string);
}

ut_test(kb_to_sector_uint32)
{
    {
        uint32_t sector;
        uint32_t kb = (1UL << 31) - 1;
        UT_ASSERT_EQUAL(EXA_SUCCESS, kb_to_sector_uint32(kb, &sector));
        UT_ASSERT_EQUAL(kb*2, sector);
    }
    {
        uint32_t sector;
        uint32_t kb = (1UL << 31) - 1234;
        UT_ASSERT_EQUAL(EXA_SUCCESS, kb_to_sector_uint32(kb, &sector));
        UT_ASSERT_EQUAL(kb*2, sector);
    }
    {
        uint32_t sector;
        uint32_t kb = 1UL << 31;
        UT_ASSERT_EQUAL(-ERANGE, kb_to_sector_uint32(kb, &sector));
    }
    {
        uint32_t sector;
        uint32_t kb = (1UL << 31) + 1234;
        UT_ASSERT_EQUAL(-ERANGE, kb_to_sector_uint32(kb, &sector));
    }
}


ut_test(kb_to_sector_uint64)
{
    {
        uint64_t sector;
        uint64_t kb = (1ULL << 63) - 1;
        UT_ASSERT_EQUAL(EXA_SUCCESS, kb_to_sector_uint64(kb, &sector));
        UT_ASSERT_EQUAL(kb*2, sector);
    }
    {
        uint64_t sector;
        uint64_t kb = (1ULL << 63) - 123456;
        UT_ASSERT_EQUAL(EXA_SUCCESS, kb_to_sector_uint64(kb, &sector));
        UT_ASSERT_EQUAL(kb*2, sector);
    }
    {
        uint64_t sector;
        uint64_t kb = (1ULL << 63);
        UT_ASSERT_EQUAL(-ERANGE, kb_to_sector_uint64(kb, &sector));
    }
    {
        uint64_t sector;
        uint64_t kb = (1ULL << 63) + 123456;
        UT_ASSERT_EQUAL(-ERANGE, kb_to_sector_uint64(kb, &sector));
    }
    {
        uint64_t sector;
        uint32_t kb = (1UL << 31) + 1234;
        UT_ASSERT_EQUAL(EXA_SUCCESS, kb_to_sector_uint64(kb, &sector));
        UT_ASSERT_EQUAL(((uint64_t)kb)*2, sector);
    }
}

