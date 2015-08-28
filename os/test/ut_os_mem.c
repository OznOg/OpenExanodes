/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "os/include/os_mem.h"
#include "os/include/os_error.h"
#include <unit_testing.h>

static void *mem;

UT_SECTION(os_aligned_malloc__and__os_aligned_free)

ut_test(os_aligned_malloc)
{
    mem = os_aligned_malloc(4096, 4096, NULL);
    if (mem == NULL)
	UT_FAIL();
}

ut_test(os_aligned_free)
{
    os_aligned_free(mem);
}

ut_test(os_aligned_malloc_wrong_alignment_not_multiple_of_void_ptr)
{
    char *buf = NULL;
    int err;

    buf = os_aligned_malloc(4096, 14, &err);
    UT_ASSERT(!buf);
    UT_ASSERT_EQUAL(EINVAL, err);
}

ut_test(os_aligned_malloc_wrong_alignment_not_multiple_of_2)
{
    char *buf = NULL;
    int err;

    buf = os_aligned_malloc(4096, 13, &err);
    UT_ASSERT(!buf);
    UT_ASSERT_EQUAL(EINVAL, err);
}

UT_SECTION(os_strndup)

ut_test(ndup_null_terminated_buffer)
{
    const char *greeting = "hello";
    char *result;

    result = os_strndup(greeting, 128);
    UT_ASSERT(result != NULL);
    ut_printf("greeting = '%s', result = '%s'", greeting, result);
    UT_ASSERT_EQUAL(0, strcmp(result, greeting));
}

ut_test(ndup_non_null_terminated_smaller_than_limit_buffer)
{
    const char buf[3] = { '1', '2', '3' };
    char *result;

    result = os_strndup(buf, 3);
    UT_ASSERT(result != NULL);
    UT_ASSERT_EQUAL(0, strcmp(result, "123"));
}

ut_test(ndup_non_null_terminated_longer_than_limit_buffer)
{
    const char buf[5] = { '1', '2', '3', '4', '5' };
    char *result;

    result = os_strndup(buf, 4);
    UT_ASSERT(result != NULL);
    UT_ASSERT_EQUAL(0, strcmp(result, "1234"));
}
