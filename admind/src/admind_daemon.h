/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/* XXX Bad naming. Should be admind.h, but there is already a header with
 * that name => rename old header to something else... */

#ifndef ADMIND_DAEMON_H
#define ADMIND_DAEMON_H

#include "os/include/os_inttypes.h"

#include "os/include/os_stdio.h"

/**
 * Print the version of Admind on stdout.
 */
void admind_print_version(void);

/**
 * Initialization of Admind.
 *
 * @param[in] foreground   Whether to run in the foreground
 *
 * @return 0 if successful, a negative error code otherwise
 */
int admind_init(bool foreground);

/**
 * Cleanup of Admind.
 *
 * *Must* be called before exiting admind.
 */
void admind_cleanup(void);

/**
 * Tell Admind to quit.
 */
void admind_quit(void);

/**
 * Topevel event loop
 *
 * @return 0 if successful, a (positive) exit status otherwise
 */
int admind_main(void);


#endif  /* ADMIND_DAEMON_H */
