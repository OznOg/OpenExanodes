/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef OS_DAEMON_CHILD_H
#define OS_DAEMON_CHILD_H

#include "os/include/os_inttypes.h"

/**
 * Daemon initialization.
 *
 * The arguments are passed just like for a regular main().
 *
 * @param[in] argc  Number of arguments
 * @param[in] argv  Commandline arguments.
 *
 * @return 0 if successfull, a negative error code otherwise.
 */
extern int daemon_init(int argc, char *argv[]);

/**
 * Daemon toplevel loop.
 *
 * @return 0 if successfull, an exit code otherwise (see exit()).
 */
extern int daemon_main(void);

/**
 * Tells whether the parent of the daemon has requested it to quit.
 *
 * @note Meant to be used *only* in the *toplevel* loop of a daemon.
 *       Do *not* call it in secondary threads: it would impede the
 *       reusability of the threads' module since this library defines
 *       main().
 *
 * @return true if quit was requested, false otherwise
 */
extern bool daemon_must_quit(void);

#endif /* OS_DAEMON_CHILD_H */
