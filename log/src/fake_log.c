/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "log/include/log.h"
#include "common/include/exa_assert.h"

/* Fake logging functions to allow linking. */
void exalog_as(ExamsgID cid)
{}

void exalog_end(void)
{}

void exalog_text(exalog_level_t level, const char *file, const char *func,
                 uint32_t line, const char *fmt, ...)
{
    exalog_level_t env_level;
    char *l = getenv("EXALOG_LEVEL");
    if (!l)
	return;

    EXA_ASSERT(EXALOG_LEVEL_IS_VALID(level));

  if (!strcmp(l, "ERROR"))
      env_level = EXALOG_LEVEL_ERROR;
  else if (!strcmp(l, "WARNING"))
      env_level = EXALOG_LEVEL_WARNING;
  else if (!strcmp(l, "INFO"))
      env_level = EXALOG_LEVEL_INFO;
  else if (!strcmp(l, "DEBUG"))
      env_level = EXALOG_LEVEL_DEBUG;
  else if (!strcmp(l, "TRACE"))
      env_level = EXALOG_LEVEL_TRACE;
  else
      env_level = EXALOG_LEVEL_NONE;

  if (env_level >= level)
  {
      va_list al;

      va_start(al, fmt);
      vprintf(fmt, al);
      va_end(al);

      printf("\n");
  }
}


int exalog_set_hostname(const char *unused)
{
    return 0;
}


int exalog_quit(void)
{
    return 0;
}
