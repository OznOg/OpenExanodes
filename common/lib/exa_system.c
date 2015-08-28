/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_error.h"

#include "log/include/log.h"
#include "os/include/os_system.h"
#include "common/include/exa_system.h"
#include "os/include/os_stdio.h"



int exa_system(char *const *command)
{
    char str[EXALOG_MSG_MAX];
    char *pos = str;
    char *const *arg;

    if (!command || !command[0])
        return -EINVAL;

    /* Get the command in one string for info purposes */
    for (arg = command; *arg != NULL; arg++)
    {
        pos += os_snprintf(pos,
                        sizeof(str) - (pos - str) - sizeof(char) /* for \0 */,
                        "%s ",
                        *arg);
        if (pos > str + sizeof(str))
            break;
    }

    exalog_info("Execute '%s'", str);

    return os_system(command);
}
