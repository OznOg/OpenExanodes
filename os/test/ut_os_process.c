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

    /* pid_t is said to be a signed integer, thus take the maximum positive integer from pid_t */
    UT_ASSERT(pid > 0 && pid < (pid_t)((1 << ((sizeof(pid_t) - 1) * 8)) -1));
}

