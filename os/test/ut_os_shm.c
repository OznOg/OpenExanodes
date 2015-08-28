/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include <string.h>
#include "os/include/os_shm.h"

#define MY_DUMMY_SHM_ID "my_dummy_shm_id"
#define MY_DUMMY_SHM_SIZE 10000

UT_SECTION(os_shm_get)

ut_test(get_an_inexistant_shm)
{
    os_shm_t *shm = os_shm_get(MY_DUMMY_SHM_ID, MY_DUMMY_SHM_SIZE);
    UT_ASSERT(!shm);
}

ut_test(get_shm_with_null_id_returns_NULL)
{
    UT_ASSERT(os_shm_get(NULL, MY_DUMMY_SHM_SIZE) == NULL);
}

ut_test(get_shm_with_size_zero_returns_NULL)
{
    UT_ASSERT(os_shm_get(MY_DUMMY_SHM_ID, 0) == NULL);
}

static const char *too_long_id(void)
{
    /* Build an id one byte too long */
    static char id[OS_SHM_ID_MAXLEN + 1 + 1];

    memset(id, 'a', sizeof(id));
    id[sizeof(id) - 1] = '\0';

    return id;
}

ut_test(get_shm_with_too_long_an_id_returns_NULL)
{
    UT_ASSERT(os_shm_get(too_long_id(), MY_DUMMY_SHM_SIZE) == NULL);
}

UT_SECTION(os_shm_create)

ut_test(create_shm_with_null_id_returns_NULL)
{
    UT_ASSERT(os_shm_create(NULL, MY_DUMMY_SHM_SIZE) == NULL);
}

ut_test(create_shm_with_size_zero_returns_NULL)
{
    UT_ASSERT(os_shm_create(MY_DUMMY_SHM_ID, 0) == NULL);
}

ut_test(create_shm_with_too_long_an_id_returns_NULL)
{
    UT_ASSERT(os_shm_create(too_long_id(), MY_DUMMY_SHM_SIZE) == NULL);
}

UT_SECTION(os_shm_get_data)

ut_test(create_write_read_and_delete_shm)
{
    unsigned char *byte;
    void *data;
    os_shm_t *shm = os_shm_create(MY_DUMMY_SHM_ID, MY_DUMMY_SHM_SIZE);

    UT_ASSERT(shm);

    data = os_shm_get_data(shm);
    UT_ASSERT(data);

    memset(data, 0xEE, MY_DUMMY_SHM_SIZE);

    for (byte = data; byte < (unsigned char *)data + MY_DUMMY_SHM_SIZE; byte++)
	UT_ASSERT(*byte == 0xEE);

    os_shm_delete(shm);
}

ut_test(try_to_rw_on_shm_owned_by_self)
{
    const unsigned char *byte;
    void *data;
    const void *data_ro;
    os_shm_t *shm = os_shm_create(MY_DUMMY_SHM_ID, MY_DUMMY_SHM_SIZE);

    data = os_shm_get_data(shm);
    UT_ASSERT(data);

    /* Fill with junk */
    memset(data, 0xEE, MY_DUMMY_SHM_SIZE);

    /* data should not be accessed anymore in rw */
    data = NULL;

    data_ro = os_shm_get_data(shm);
    UT_ASSERT(data_ro);

    /* Try to read data */
    for (byte = data_ro; byte < (unsigned char *)data_ro + MY_DUMMY_SHM_SIZE; byte++)
	UT_ASSERT(*byte == 0xEE);

    os_shm_delete(shm);
}

ut_test(try_to_rw_on_shm_owned_by_other)
{
    unsigned char *byte;
    os_shm_t *shm  = os_shm_create(MY_DUMMY_SHM_ID, MY_DUMMY_SHM_SIZE);
    os_shm_t *shmc = os_shm_get(MY_DUMMY_SHM_ID, MY_DUMMY_SHM_SIZE);

     UT_ASSERT(shm);
     UT_ASSERT(shmc);

     /* Fill with something */
     memset(os_shm_get_data(shm), 'A', MY_DUMMY_SHM_SIZE);

     for (byte = os_shm_get_data(shmc);
	     byte < (unsigned char *)os_shm_get_data(shmc) + MY_DUMMY_SHM_SIZE;
	     byte++)
	 UT_ASSERT(*byte == 'A');

    os_shm_release(shmc);
    os_shm_delete(shm);
}

