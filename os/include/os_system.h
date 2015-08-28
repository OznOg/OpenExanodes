/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_SYSTEM_H
#define _OS_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Terminate the calling process
 *
 * @param status Exit status
 *
 * @os_replace{Linux, exit}
 * @os_replace{Windows, exit}
 */
void os_exit(int status);

/**
 * Run a program. The caller waits for the termination of the program.
 *
 * @param command  The program to start and the arguments to pass to it
 *
 * @return Program's exit status (zero if ok, non-zero otherwise).
 *
 * @os_replace{Linux, fork, execvp}
 * @os_replace{Windows, CreateProcess}
 */
int os_system(char *const *command);

#ifdef __cplusplus
}
#endif

#endif /* _OS_SYSTEM_H */

