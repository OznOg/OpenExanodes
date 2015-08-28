/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "common/include/threadonize.h"
#include "os/include/os_mem.h"
#include "common/include/exa_thread_name.h"
#include "os/include/strlcpy.h"
#include "os/include/os_time.h"
#include "os/include/os_mem.h"


/* --- local data ---------------------------------------------------- */

static void	exathread_routine(void *arg);
static void	exa_init_stack(size_t size);


/* --- exathread_create ---------------------------------------------- */

/** \brief Create exanodes threads
 *
 * \param[out] thread		Identifier of the newly created thread.
 * \param[in] stack		Size of the stack, in bytes.
 * \param[in] start_routine	Function executed by the thread.
 * \param[in] arg		Data passed to the \a start_routine.
 *
 * \return 0 on success
 */

struct exathread_attr {
  void (*start_routine)(void *);
  void *arg;
  size_t stack;
  char name[32];
};




bool
exathread_create_named(os_thread_t *thread, size_t stack,
		 void (*start_routine)(void *), void *arg, const char * name)
{
  struct exathread_attr *data;

  /* FIXME this hardcoded values are really stange and must be buggy... */
  stack = (stack < 32768) ? 32768 : stack;

  data = os_malloc(sizeof(*data));
  if (!data)
    return false;

  data->start_routine = start_routine;
  data->arg = arg;
  data->stack = stack;
  if (name==NULL)
    data->name[0]=0;
  else
    strlcpy(data->name, name, sizeof(data->name));

  return os_thread_create(thread, stack, exathread_routine, data);
}


bool
exathread_create(os_thread_t *thread, size_t stack,
                 void (*start_routine)(void *), void *arg)
{
    return exathread_create_named(thread, stack, start_routine, arg, NULL);
}


/* --- exathread_routine --------------------------------------------- */

/** \brief Exanodes thread startup routine
 *
 * This routine invokes user function after having initalized the stack.
 */

static void
exathread_routine(void *arg)
{
  struct exathread_attr data = *(struct exathread_attr *)arg;

  os_free(arg);
  exa_init_stack(data.stack - 16384
	  /* 16384 is completely arbitrary. It represents the amount of the
	   * stack that _MAY_ already be consumed (as the thread is already
	   * running, it uses its stack...). This may be local variables of
	   * functions, functions themselves ans also TLS ans static variables
	   * So experiencing a SIGSEGV in the above exa_init_stack function
	   * may probably mean that a thread allocated a huge structure on it
	   * stack which make 16384 too small. see bug #3877 */);
#ifndef WIN32
  /* FIXME I cannot find an equivalent function for windows, so if
   * we really need this, we should implement it */
  if (data.name[0]!=0)
    exa_thread_name_set(data.name);
#endif
  data.start_routine(data.arg);
}


/* --- exa_init_stack ------------------------------------------- */

/** \brief Initialize stack
 *
 * This makes an alloca to be sure your process or thread will have at
 * least a specific amount of stack allocated.  The allocated stack is
 * also fully written to be sure it's memory mapped by the operating
 * system.
 *
 * For a thread it is important to set the stack size to a lower value
 * (than the 8MB default).
 * Usage:
 *  pthread_attr_init(&attr);
 *  pthread_attr_setstacksize(&attr,
 * 	 <i>YOUR_MODULE</i>_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE);
 *
 * Then pass attr to your os_thread_create.
 *
 * \param[in] size the size in bytes
 */

static void
exa_init_stack(size_t size)
{
  void *stack = alloca(size);
  /* You have a segfault here ? did you read the comment in exathread_routine ? */
  memset(stack, 0, size);
}

