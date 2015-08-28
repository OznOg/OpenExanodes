/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_USER_H
#define _OS_USER_H

#include "os/include/os_inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These constants are exposed so that one can use them in error messages */
#ifdef WIN32
#define OS_USER_HOMEDIR_VAR "APPDATA"
#else
#define OS_USER_HOMEDIR_VAR "HOME"
#endif

/**
 * Tell whether the user is administrator/root.
 *
 * @return true if admin, false otherwise
 */
bool os_user_is_admin(void);

/**
 * Get the home directory of the current user.
 *
 * On Linux this is the HOME environment variable.
 * On Windows this is the APPDATA environment variable.
 *
 * @return User's home directory
 */
const char *os_user_get_homedir(void);

#ifdef __cplusplus
}
#endif

#endif
