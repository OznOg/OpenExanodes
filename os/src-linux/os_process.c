/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_process.h"
#include "os/include/os_assert.h"

#include <unistd.h>

os_pid_t os_process_id(void)
{
    COMPILE_TIME_ASSERT(sizeof(os_pid_t) == 4);

    return getpid();
}
