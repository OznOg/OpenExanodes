/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"
#include "common/include/uuid.h"
#include "os/include/os_random.h"
#include "os/include/os_stdio.h"
#include <string.h>

UT_SECTION(uuid_equality)

ut_test(uuid_is_equal_with_diff_uuids_returns_false)
{
    exa_uuid_t uuid1, uuid2;

    memset(&uuid1, 0, sizeof(uuid1));
    memset(&uuid2, 1, sizeof(uuid2));

    UT_ASSERT(!uuid_is_equal(&uuid1, &uuid2));
}

ut_test(uuid_is_equal_with_same_uuids_returns_true)
{
    exa_uuid_t uuid1, uuid2;

    memset(&uuid1, 3, sizeof(uuid1));
    memset(&uuid2, 3, sizeof(uuid2));

    UT_ASSERT(uuid_is_equal(&uuid1, &uuid2));
}

UT_SECTION(uuid_compare)

ut_test(comparison_of_first_lower_than_second_returns_minus_one)
{
    exa_uuid_t uuid1 = { .id = { 3, 5, 0, 9 } };
    exa_uuid_t uuid2 = { .id = { 3, 5, 4, 9 } };

    UT_ASSERT(uuid_compare(&uuid1, &uuid2) == -1);
}

ut_test(comparison_of_first_higher_than_second_returns_plus_one)
{
    exa_uuid_t uuid1 = { .id = { 3, 5, 4, 9 } };
    exa_uuid_t uuid2 = { .id = { 3, 5, 0, 9 } };

    UT_ASSERT(uuid_compare(&uuid1, &uuid2) == +1);
}

ut_test(comparison_of_equal_uuids_returns_zero)
{
    exa_uuid_t uuid1 = { .id = { 7, 9, 6, 2 } };
    exa_uuid_t uuid2 = { .id = { 7, 9, 6, 2 } };

    UT_ASSERT(uuid_compare(&uuid1, &uuid2) == 0);
}

UT_SECTION(uuid_is_zero)

ut_test(uuid_is_zero_with_nonzero_returns_false)
{
    exa_uuid_t uuid = { .id = { 0, 0, 8, 0 } };
    UT_ASSERT(!uuid_is_zero(&uuid));
}

ut_test(uuid_is_zero_with_zero_returns_true)
{
    exa_uuid_t uuid = { .id = { 0, 0, 0, 0 } };
    UT_ASSERT(uuid_is_zero(&uuid));
}

UT_SECTION(uuid_copy)

ut_test(uuid_copy_does_copy)
{
    exa_uuid_t uuid1 = { .id = { 9, 4, 1, 0 } };
    exa_uuid_t uuid2;

    uuid_copy(&uuid2, &uuid1);
    UT_ASSERT(uuid_is_equal(&uuid2, &uuid1));
}

UT_SECTION(uuid_zero)

ut_test(uuid_zero_does_zero)
{
    exa_uuid_t uuid = { .id = { 5, 5, 5, 6 } };
    uuid_zero(&uuid);
    UT_ASSERT(uuid_is_zero(&uuid));
}

UT_SECTION(uuid2str)

/* XXX uuid2str actually never returns NULL (unless its parameter
 * str_uuid is NULL) */
ut_test(uuid2str_returns_string_repr)
{
    exa_uuid_t uuid = { .id = { 3, 1, 4, 1 } };
    exa_uuid_str_t str;
    char *r;

    r = uuid2str(&uuid, str);
    UT_ASSERT_EQUAL_STR(r, "00000003:00000001:00000004:00000001");
}

UT_SECTION(uuid_scan)

/*
 * TODO XXX All unit tests here should be split into tests performing only
 * *one* thing.
 */

ut_test(scan_invalid_string_returns_ERR_UUID)
{
  char toobig[UUID_STR_LEN * 80];
  exa_uuid_t uuid;
  int ret;

  ret = uuid_scan("Very bad uuid", &uuid);
  UT_ASSERT(ret == -EXA_ERR_UUID);

  ret = uuid_scan("12345678_12345678/12345678012345678", &uuid);
  UT_ASSERT(ret == -EXA_ERR_UUID);

  ret = uuid_scan("SSSSSSSS:SSSSSSSS:SSSSSSSS:SSSSSSSS", &uuid);
  UT_ASSERT(ret == -EXA_ERR_UUID);

  ret = uuid_scan("11111111:22222222:22222222:SSSSSSSS", &uuid);
  UT_ASSERT(ret == -EXA_ERR_UUID);

  /* Note, no \0 at the end, on purpose */
  memset(toobig, 'G', sizeof(toobig));

  ret = uuid_scan(toobig, &uuid);
  UT_ASSERT(ret == -EXA_ERR_UUID);

  /* The A is off by one */
  ret = uuid_scan("11111111:22222222:22222222:33333333A", &uuid);
  UT_ASSERT(ret == -EXA_ERR_UUID);
}

ut_test(scan_invalid_params_returns_ERR_UUID)
{
  exa_uuid_t uuid;
  int ret;

  ret = uuid_scan("", NULL);
  UT_ASSERT(ret == -EXA_ERR_UUID);

  ret = uuid_scan(NULL, &uuid);
  UT_ASSERT(ret == -EXA_ERR_UUID);

  ret = uuid_scan(NULL, NULL);
  UT_ASSERT(ret == -EXA_ERR_UUID);
}

ut_test(scan_valid_strings_returns_SUCCESS)
{
  exa_uuid_t uuid, uuid2;
  char str[UUID_STR_LEN + 1];
  int ret;

  os_random_init();

  uuid_generate(&uuid);

  /* write the UUID into a string */
  os_snprintf(str, UUID_STR_LEN + 1, UUID_FMT, UUID_VAL(&uuid));

  /* parse it back */
  ret = uuid_scan(str, &uuid2);
  UT_ASSERT(ret == EXA_SUCCESS);

  /* check it is actually still the same */
  ret = memcmp(&uuid, &uuid2, sizeof(uuid));
  UT_ASSERT(ret == 0);

  /* Check uuid_is_equal agrees */
  UT_ASSERT(uuid_is_equal(&uuid, &uuid2));
}
