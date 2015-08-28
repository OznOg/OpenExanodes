/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "blockdevice/include/blockdevice.h"

/* Memory block device: we need some actual implementation to test the
   API... */

static const char *dummy_get_name(const void *context)
{
    return NULL;
}

static uint64_t dummy_get_sector_count(const void *context)
{
    return 0;
}

static int dummy_set_sector_count(void *context, uint64_t count)
{
    return 0;
}

static int dummy_submit_io(void *context, blockdevice_io_t *io)
{
    return 0;
}

static int dummy_close(void *context)
{
    return 0;
}

static blockdevice_ops_t all_ops =
{
    .get_name_op = dummy_get_name,

    .get_sector_count_op = dummy_get_sector_count,
    .set_sector_count_op = dummy_set_sector_count,

    .submit_io_op = dummy_submit_io,

    .close_op = dummy_close
};

/* No need to have a valid context for what we'll do here. */
static void *dummy_ctx = (void *)1;
static blockdevice_t *bdev;

UT_SECTION(blockdevice_open)

ut_test(opening_blockdevice_with_null_context_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, blockdevice_open(&bdev, NULL, &all_ops,
                                              BLOCKDEVICE_ACCESS_READ));
}

ut_test(opening_blockdevice_with_invalid_access_returns_EINVAL)
{
    UT_ASSERT_EQUAL(-EINVAL, blockdevice_open(&bdev, dummy_ctx, &all_ops,
                                              BLOCKDEVICE_ACCESS__FIRST - 1));
    UT_ASSERT_EQUAL(-EINVAL, blockdevice_open(&bdev, dummy_ctx, &all_ops,
                                              BLOCKDEVICE_ACCESS__LAST + 1));
}

ut_test(opening_blockdevice_with_basic_accessors_undefined_returns_EINVAL)
{
    blockdevice_ops_t no_base_accessors_ops = all_ops;

    no_base_accessors_ops.get_name_op = NULL;
    no_base_accessors_ops.get_sector_count_op = NULL;

    UT_ASSERT_EQUAL(-EINVAL, blockdevice_open(&bdev, dummy_ctx, &no_base_accessors_ops,
                                              BLOCKDEVICE_ACCESS_WRITE));
}

ut_test(opening_blockdevice_with_no_submit_io_op_defined_fails_with_EINVAL)
{
    blockdevice_ops_t no_async_ops = all_ops;

    no_async_ops.submit_io_op = NULL;

    UT_ASSERT_EQUAL(-EINVAL, blockdevice_open(&bdev, dummy_ctx, &no_async_ops,
                                        BLOCKDEVICE_ACCESS_RW));
}

ut_test(opening_blockdevice_with_all_params_valid_succeeds)
{
    blockdevice_access_t a;

    for (a = BLOCKDEVICE_ACCESS__FIRST; a <= BLOCKDEVICE_ACCESS__LAST; a++)
    {
        UT_ASSERT_EQUAL(0, blockdevice_open(&bdev, dummy_ctx, &all_ops, a));
        blockdevice_close(bdev);
    }
}

UT_SECTION(blockdevice_submit_io)

ut_setup()
{
    UT_ASSERT_EQUAL(0, blockdevice_open(&bdev, dummy_ctx, &all_ops,
                                        BLOCKDEVICE_ACCESS_READ));
}

ut_cleanup()
{
    blockdevice_close(bdev);
}

void dummy_io_cb(blockdevice_io_t *io, int err)
{}

ut_test(submitting_null_io_returns_EINVAL)
{
    char buf[10];

    UT_ASSERT_EQUAL(-EINVAL,
                    blockdevice_submit_io(bdev, NULL, BLOCKDEVICE_IO_READ, 0,
                                          buf, sizeof(buf), false, 0, dummy_io_cb));
}

ut_test(submitting_io_with_invalid_type_returns_EINVAL)
{
    blockdevice_io_t io;
    char buf[10];

    UT_ASSERT_EQUAL(-EINVAL,
                    blockdevice_submit_io(bdev, &io, BLOCKDEVICE_IO_TYPE__FIRST - 1,
                                          0, buf, sizeof(buf), false, 0, dummy_io_cb));

    UT_ASSERT_EQUAL(-EINVAL,
                    blockdevice_submit_io(bdev, &io, BLOCKDEVICE_IO_TYPE__LAST + 1,
                                          0, buf, sizeof(buf), false, 0, dummy_io_cb));
}

ut_test(submitting_io_with_null_buf_and_non_zero_size_returns_EINVAL)
{
    blockdevice_io_t io;

    UT_ASSERT_EQUAL(-EINVAL,
                    blockdevice_submit_io(bdev, &io, BLOCKDEVICE_IO_READ,
                                          0, NULL, 16, false, 0, dummy_io_cb));
}

ut_test(submitting_io_with_all_params_valid_succeeds)
{
    blockdevice_io_t io;
    char buf[16];

    UT_ASSERT_EQUAL(0, blockdevice_submit_io(bdev, &io, BLOCKDEVICE_IO_READ,
                                             0, buf, sizeof(buf), false, 0, dummy_io_cb));
}
