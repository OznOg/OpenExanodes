/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>

#include "os/include/os_assert.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_system.h"

int main(int argc, char *argv[])
{
    int status;

    OS_ASSERT(argc == 2);

    status = atoi(argv[1]);
    os_exit(status);

    /* Should never go there */
    OS_ASSERT(false);

    /* Prevent a warning */
    return 0;
}
