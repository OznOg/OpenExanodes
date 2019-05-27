/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>

#include <unit_testing.h>

#include "admind/src/adm_nic.h"
#include "common/include/exa_error.h"

#include "os/include/os_error.h"
#include "os/include/os_network.h"

UT_SECTION(adm_nic)

ut_test(create_and_delete_valid_nic_ip)
{
    struct adm_nic *nic = NULL;
    int err;

    err = adm_nic_new("123.123.123.123", &nic);
    UT_ASSERT_EQUAL(0, err);
    UT_ASSERT(nic != NULL);
    UT_ASSERT_EQUAL_STR("123.123.123.123", adm_nic_ip_str(nic));
    UT_ASSERT_EQUAL_STR("123.123.123.123", adm_nic_get_hostname(nic));

    adm_nic_free(nic);
}

ut_test(create_and_delete_invalid_ip)
{
    struct adm_nic *nic = NULL;
    int err;

    err = adm_nic_new("123.1123.123.123", &nic);
    UT_ASSERT(err != 0);
    UT_ASSERT(nic == NULL);
}

ut_test(create_and_delete_invalid_hostname)
{
    struct adm_nic *nic = NULL;
    int err;

    err = adm_nic_new("toto.prout", &nic);
    UT_ASSERT(err != 0);
    UT_ASSERT(nic == NULL);
}

ut_test(create_and_delete_valid_hostname)
{
    struct adm_nic *nic = NULL;
    int err;

    err = adm_nic_new("isima.fr", &nic);
    UT_ASSERT(err == 0);
    UT_ASSERT(nic != NULL);

    UT_ASSERT_EQUAL_STR("193.55.95.39", adm_nic_ip_str(nic));
    UT_ASSERT_EQUAL_STR("isima.fr", adm_nic_get_hostname(nic));

    adm_nic_free(nic);
}
