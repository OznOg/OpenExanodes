/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include <string>

#include "common/include/exa_constants.h"
#include "ui/common/include/config_check.h"

static exa_error_code
check_name(const std::string& name, uint size)
{
    return ConfigCheck::check_param(
                ConfigCheck::CHECK_NAME, size, name, false);
}

ut_test(name_check)
{
    UT_ASSERT_EQUAL(EXA_SUCCESS, check_name("ABC_12.3-xyz", EXA_MAXSIZE_CLUSTERNAME));

    UT_ASSERT_EQUAL(EXA_SUCCESS, check_name("123456789ABCDEF", EXA_MAXSIZE_GROUPNAME));

    UT_ASSERT(EXA_SUCCESS != check_name("0123456789ABCDEF", EXA_MAXSIZE_VOLUMENAME));

#ifdef WITH_FS
    UT_ASSERT(EXA_SUCCESS != check_name("a:b", EXA_MAXSIZE_FSTYPE));
#endif

    UT_ASSERT(EXA_SUCCESS != check_name("X+Y", EXA_MAXSIZE_VOLUMENAME));

    UT_ASSERT_EQUAL(EXA_SUCCESS, check_name(".", EXA_MAXSIZE_VOLUMENAME));

    UT_ASSERT_EQUAL(EXA_SUCCESS, check_name("..", EXA_MAXSIZE_GROUPNAME));
}
