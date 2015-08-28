/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <asm/uaccess.h>
#include <linux/sched.h>


int exa_thread_name_set_kernel(const char *name)
{
    char __name[TASK_COMM_LEN];

    if (copy_from_user(__name, name, TASK_COMM_LEN) != 0)
        return -EFAULT;

    strlcpy(current->comm, __name, TASK_COMM_LEN);

    return 0;
}
