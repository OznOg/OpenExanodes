/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifdef WITH_SUPSIM

#include "sup_debug.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/**
 * Fake logging function with the same signature as the real one
 * from Exanodes' logging API.
 */
int
__exalog_text(int unused_error_level, const char *unused_file,
	      const char *unused_fun, unsigned unused_line, const char *fmt, ...)
{
  time_t t;
  char time_str[64];
  va_list al;

  time(&t);
  strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&t));
  printf("%s: ", time_str);

  va_start(al, fmt);
  vprintf(fmt, al);
  va_end(al);

  printf("\n");

  return 0;
}

#endif /* WITH_SUPSIM */
