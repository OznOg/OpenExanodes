/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_EXAMSGTEST
#define H_EXAMSGTEST

/** \file examsgtest.h
 * \brief Header for examsg test programs and modules
 */

#include "os/include/os_stdio.h"
#include <log/include/log.h>

static inline void gdbreak(void);
static inline void gdbreak() { /* dummy function to insert breakpoint! */; }

#ifndef WIN32
#define COLOR_PASSED  "\e[1;32m"
#define COLOR_FAILED  "\e[1;31m"
#define COLOR_NORMAL  "\e[m"
#else
#define COLOR_PASSED  " !! "
#define COLOR_FAILED  " XX "
#define COLOR_NORMAL  ""
#endif

#define EXAMSG_TEST(condition, ...)				\
  do {								\
    char tmp[90];\
    if (!(condition)) {                                         \
      os_snprintf(tmp, sizeof(tmp), __VA_ARGS__);                  \
      printf("%-50s", tmp);				        \
      printf(": %sFAILED%s\n", COLOR_FAILED, COLOR_NORMAL);	\
      gdbreak();						\
      return examsgtest_returnstatus;				\
    } else {							\
      os_snprintf(tmp, sizeof(tmp), __VA_ARGS__);                  \
      printf("%-50s", tmp);				        \
      printf(": %sok%s\n", COLOR_PASSED, COLOR_NORMAL);	        \
    }								\
  } while(0)

extern int examsgtest_returnstatus;

int	examsgtest_doit(void);

#endif /* H_EXAMSGTEST */
