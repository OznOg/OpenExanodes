/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _EXA_THREAD_NAME_H
#define _EXA_THREAD_NAME_H

#include "common/include/exa_error.h"

#ifdef WIN32

#define exa_thread_name_set(name) EXA_SUCCESS

#else

/** Set the comm field of the current task struct */
int exa_thread_name_set(char *name);

#endif

#endif /* _EXA_THREAD_NAME_H */
