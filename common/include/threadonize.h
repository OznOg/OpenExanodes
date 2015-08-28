/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef THREADONIZE_H
#define THREADONIZE_H

#include "os/include/os_inttypes.h"
#include "os/include/os_thread.h"

/*
 * The minimal 'security' stack size to allocate. Use it in your
 * thread stack initialisation like:
 *
 * pthread_attr_setstacksize(&thread_attr,
 * <YOUR_MODULE>_THREAD_STACK_SIZE+MIN_THREAD_STACK_SIZE);
 * 16384 stands for the PTHREAD_STACK_MIN defined thru limit.h
 * I do not make the include because it would also need a define __USE_POSIX
 * which I am not sure we want to do here...
 */
#define MIN_THREAD_STACK_SIZE 16384

bool	exathread_create(os_thread_t *thread, size_t stack,
		void (*start_routine)(void *), void *arg);
bool    exathread_create_named(os_thread_t *thread, size_t stack,
                void (*start_routine)(void *), void *arg, const char * name);

#endif /* THREADONIZE_H */
