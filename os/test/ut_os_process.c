/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "os/include/os_process.h"

ut_test(os_process_id_returns_value_in_correct_range)
{
    os_pid_t pid = os_process_id();

    /* This is twice pid_max default value on Linux */
    UT_ASSERT(pid > 0 && pid < 65536);
}

