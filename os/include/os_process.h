/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**
 * API for processes
 */

#ifndef _OS_PROCESS_H
#define _OS_PROCESS_H

/* DWORD is 32 bits on Windows, and pid_t is 32 bits on Linux. */
#ifdef WIN32
    #include <windows.h>
    typedef DWORD os_pid_t;
#else
    #include <sys/types.h>
    typedef pid_t os_pid_t;
#endif

/**
 * Get current process id.
 *
 * @return PID of the process
 */
os_pid_t os_process_id(void);

#endif /* _OS_PROCESS_H */
