/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "os/include/os_user.h"


bool os_user_is_admin(void)
{
    return geteuid() == 0;
}


const char *os_user_get_homedir()
{
    return getenv(OS_USER_HOMEDIR_VAR);
}
