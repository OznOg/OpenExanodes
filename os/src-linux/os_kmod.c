/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "os/include/os_kmod.h"
#include "os/include/os_stdio.h"

int os_kmod_load(const char *module_name)
{
    char sys_cmd[512];
    struct stat sstat;
    int ret;

    /* check if module is already loaded */
    os_snprintf(sys_cmd, sizeof(sys_cmd), "lsmod | grep -q %s", module_name);
    ret = WEXITSTATUS(system(sys_cmd));
    if (ret != 0)
    {
        /* module not loaded ; load it ! */
        os_snprintf(sys_cmd, sizeof(sys_cmd), "modprobe %s", module_name);

        ret = WEXITSTATUS(system(sys_cmd));
        if (ret != 0)
            return ret;
    }

    /* delete the char if the device file exists (the major may change) */
    os_snprintf(sys_cmd, sizeof(sys_cmd), "/dev/%s", module_name);
    ret = stat(sys_cmd, &sstat);
    if (ret == 0)
    {
        ret = unlink(sys_cmd);
        if (ret != 0)
            return -errno;
    }

    os_snprintf(sys_cmd, sizeof(sys_cmd), "mknod /dev/%s c `cat /proc/devices "
            "| grep %s | cut -d ' ' -f 1` 0", module_name, module_name);
    ret = WEXITSTATUS(system(sys_cmd));

    return ret;
}

int os_kmod_unload(const char *module_name)
{
    char sys_cmd[512];
    int ret;

    /* check if module is loaded */
    sprintf(sys_cmd, "lsmod | grep -q %s", module_name);
    ret = WEXITSTATUS(system(sys_cmd));

    if (ret == 0)
    {
        sprintf(sys_cmd, "modprobe -r %s", module_name);

        ret = WEXITSTATUS(system(sys_cmd));
        if (ret != 0)
            return ret;
    }

    return 0;
}
