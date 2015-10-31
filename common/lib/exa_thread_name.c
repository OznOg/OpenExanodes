/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common/include/exa_error.h"
#include "common/include/exa_thread_name.h"
#include "common/lib/exa_common_kernel.h"

#include <pthread.h>


/**
 * On Linux, change the comm field in the current task struct.
 * This field is seen in /proc/$pid/stat, with SysRq-T or panic
 *
 * FIXME WIN32: On Windows, do nothing.
 *
 * @param[in] name  New name of the new name. Names longer than
 *                  TASK_COMM_LEN-1 (15 chars) are truncated.
 *
 * @return 0 in case of success.
 */

int exa_thread_name_set(const char *name)
{
    return pthread_setname_np(pthread_self(), name);
}
