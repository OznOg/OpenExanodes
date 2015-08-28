/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include "common/include/checksum.h"
#include "os/include/os_random.h"

UT_SECTION(checksum)

ut_setup()
{
    os_random_init();
}

ut_cleanup()
{
    os_random_cleanup();
}

/* We'll checksum the whole structure */
struct cksum_test_t
{
    checksum_t checksum;
    char buffer[1024];
};

ut_test(correct_checksum_in_checksummed_buffer)
{
    struct cksum_test_t buf;
    buf.checksum = 0;
    os_get_random_bytes(&buf.buffer, 1024);

    buf.checksum = exa_checksum(&buf, sizeof(buf));
    UT_ASSERT(buf.checksum != 0);

    UT_ASSERT(exa_checksum(&buf, sizeof(buf)) == 0);
}

ut_test(incorrect_checksum_in_checksummed_buffer)
{
    struct cksum_test_t buf;
    os_get_random_bytes(&buf.buffer, 1024);

    buf.checksum = 0xaa55; /* Wrong checksum (unless we're really unlucky) */

    UT_ASSERT(exa_checksum(&buf, sizeof(buf)) != 0);
}

/* We'll checksum the whole structure, with an odd number of bytes
 * Careful, Obviously the structure is not aligned, thus we need to pack it
 * in order to have a odd size. */
#define DATA_SIZE 1025
#ifdef WIN32
#pragma pack(push)
#pragma pack(1)
struct odd_cksum_test_t
{
    checksum_t checksum;
    char data[DATA_SIZE];
};
#pragma pack(pop)
#else
struct odd_cksum_test_t
{
    checksum_t checksum;
    char data[DATA_SIZE];
} __attribute__((packed));
#endif

ut_test(checksum_non_16bit_aligned_buffer)
{
    struct odd_cksum_test_t buf;

    buf.checksum = 0;
    os_get_random_bytes(&buf.data, sizeof(buf.data));

    /* Keep 2 bytes at the end to make sure there is no 'off by one'
     * calculation */
    buf.checksum = exa_checksum(&buf, sizeof(buf) - 2);
    UT_ASSERT(buf.checksum != 0);

    /* Invalidate the 2 last bytes that should not have been taken into
     * account */
    buf.data[sizeof(buf.data) - 2] = buf.data[sizeof(buf.data) - 1] = 0;

    UT_ASSERT(exa_checksum(&buf, sizeof(buf) - 2) == 0);
}

ut_test(incorrect_checksum_non_16bit_aligned_buffer)
{
    struct odd_cksum_test_t buf;
    os_get_random_bytes(&buf.data, sizeof(buf.data));

    buf.checksum = 0xaa55; /* Wrong checksum (unless we're really unlucky) */

    UT_ASSERT(exa_checksum(&buf, sizeof(buf)) != 0);
}

ut_test(checksum_outside_of_buffer)
{
    char buf[1024];
    checksum_t checksum;
    os_get_random_bytes(&buf, sizeof(buf));

    checksum = exa_checksum(&buf, sizeof(buf));
    UT_ASSERT(checksum != 0);

    UT_ASSERT(checksum == exa_checksum(&buf, sizeof(buf)));
}

ut_test(incorrect_checksum_outside_of_buffer)
{
    char buf[1024];
    checksum_t checksum;
    os_get_random_bytes(&buf, sizeof(buf));

    checksum = exa_checksum(&buf, sizeof(buf));
    UT_ASSERT(checksum != 0);

    /* change a byte in the buffer */
    buf[321] += 1;

    UT_ASSERT(checksum != exa_checksum(&buf, sizeof(buf)));
}

UT_SECTION(progressive_checksum)

ut_setup()
{
    os_random_init();
}

ut_cleanup()
{
    os_random_cleanup();
}

ut_test(checksum_of_nothing_is_zero)
{
    checksum_context_t ctx;

    checksum_reset(&ctx);

    UT_ASSERT_EQUAL(0, checksum_get_value(&ctx));
}

ut_test(checksum_of_null_buffers_is_zero)
{
    checksum_context_t ctx;

    checksum_reset(&ctx);
    checksum_feed(&ctx, NULL, 3);
    checksum_feed(&ctx, NULL, 16);
    checksum_feed(&ctx, NULL, 5);

    UT_ASSERT_EQUAL(0, checksum_get_value(&ctx));
}

ut_test(checksum_of_zero_sizes_is_zero)
{
    char buf1[8], buf2[11], buf3[64];
    checksum_context_t ctx;

    checksum_reset(&ctx);
    checksum_feed(&ctx, buf1, 0);
    checksum_feed(&ctx, buf2, 0);
    checksum_feed(&ctx, buf3, 0);

    UT_ASSERT_EQUAL(0, checksum_get_value(&ctx));
}

ut_test(checksum_of_buffer_of_even_size)
{
    char buf[1024];
    checksum_context_t ctx;
    checksum_t c, old_c;

    os_get_random_bytes(buf, sizeof(buf));

    checksum_reset(&ctx);
    checksum_feed(&ctx, buf, sizeof(buf));

    c = checksum_get_value(&ctx);
    /* Should pass unless we're unlucky */
    UT_ASSERT(c != 0);

    /* Check against old checksum */
    old_c = exa_checksum(buf, sizeof(buf));
    ut_printf("same as old checksum? %s", c == old_c ? "YES" : "NO");
    UT_ASSERT_EQUAL(old_c, c);
}

ut_test(checksum_of_buffer_of_odd_size)
{
    char buf[833];
    checksum_context_t ctx;

    os_get_random_bytes(buf, sizeof(buf));

    checksum_reset(&ctx);
    checksum_feed(&ctx, buf, sizeof(buf));

    /* Should pass unless we're unlucky */
    UT_ASSERT(checksum_get_value(&ctx) != 0);
}

ut_test(checksum_multiple_steps_is_equivalent_to_single_step)
{
    char buf[35];
    checksum_context_t ctx;
    size_t size1, size2;
    size_t n;
    checksum_t c1, c2;

    os_get_random_bytes(buf, sizeof(buf));

    /* Single step checksum */
    checksum_reset(&ctx);
    checksum_feed(&ctx, buf, sizeof(buf));

    c1 = checksum_get_value(&ctx);
    size1 = checksum_get_size(&ctx);

    UT_ASSERT(c1 != 0);

    /* Multiple steps checksum */
    checksum_reset(&ctx);
    n = 3;
    checksum_feed(&ctx, buf, n);
    checksum_feed(&ctx, buf + n, n);
    checksum_feed(&ctx, buf + n + n, sizeof(buf) - n - n);

    c2 = checksum_get_value(&ctx);
    size2 = checksum_get_size(&ctx);

    UT_ASSERT(c2 != 0);

    UT_ASSERT_EQUAL(c1, c2);

    UT_ASSERT_EQUAL(size1, size2);
}
