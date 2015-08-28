/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _CLI_LOG_H_
#define _CLI_LOG_H_

#include "os/include/os_stdio.h"

extern const char *COLOR_USED;
extern const char *COLOR_INFO;
extern const char *COLOR_NORM;
extern const char *COLOR_ERROR;
extern const char *COLOR_NOTICE;
extern const char *COLOR_WARNING;
extern const char *COLOR_FEATURE;
extern const char *COLOR_BOLD;

typedef enum {
   EXA_CLI_NOLOG = -1,
   EXA_CLI_ERROR,
   EXA_CLI_WARNING,
   EXA_CLI_NOTICE,
   EXA_CLI_INFO,
   EXA_CLI_LOG,
   EXA_CLI_TRACE
} exa_cli_log_level_t;

/*
 * Some formatting types
 */
#define FMT_TYPE_H1	  66


#define MAX_LG_STR        32768
#define MAX_DIR_LOG_NAME  64
#define MAX_FILE_LOG_NAME 64
#define MAX_ERR_MSG_LONG  128


/*
 * cli_log function must be implemented by the common
 * lib users. This allow to have specific way to display/store logs.
 */

void cli_log(const char *file, const char *func,
             int line, exa_cli_log_level_t lvl, const char *fmt, ...)
             __attribute__ ((__format__ (__printf__, 5, 6)));
/*******************************************************************************/

bool exa_verb_from_str(exa_cli_log_level_t *level, const char *str);
void set_exa_verb(exa_cli_log_level_t verb);
exa_cli_log_level_t get_exa_verb ( void);

/*! Log a trace based on the verbosity level
  \param fmt the message to print. Can include C formatting message
  \param args... the list of params for the formatting string
 */
#define exa_cli_trace(...) \
  do {									\
    cli_log ( __FILE__, __FUNCTION__, __LINE__, EXA_CLI_TRACE, __VA_ARGS__); \
  } while (0)


/*! Log an info message based on the verbosity level
  \param fmt the message to print. Can include C formatting message
  \param args... the list of params for the formatting string
 */
#define exa_cli_log(...)					\
  do {									\
    cli_log ( __FILE__, __FUNCTION__, __LINE__, EXA_CLI_LOG, __VA_ARGS__);	\
  } while (0)

/*! Log an info message based on the verbosity level
  \param fmt the message to print. Can include C formatting message
  \param args... the list of params for the formatting string
 */
#define exa_cli_info(...)					\
  do {									\
    cli_log ( __FILE__, __FUNCTION__, __LINE__, EXA_CLI_INFO, __VA_ARGS__);	\
  } while (0)

#define exa_cli_newline()                                                   \
  do {									    \
    cli_log ( __FILE__, __FUNCTION__, __LINE__, EXA_CLI_INFO, "\n"); \
  } while (0)

/*! Log a notice (not dependant of verbosity)
  \param fmt the message to print. Can include C formatting message
  \param args... the list of params for the formatting string
 */
#define exa_cli_notice(...) \
  do {									\
    cli_log ( __FILE__, __FUNCTION__, __LINE__, EXA_CLI_NOTICE, __VA_ARGS__);	\
  } while (0)

/*! Log a warning (not dependant of verbosity)
  \param fmt the message to print. Can include C formatting message
  \param args... the list of params for the formatting string
 */
#define exa_cli_warning(...) \
  do {									\
    cli_log ( __FILE__, __FUNCTION__, __LINE__, EXA_CLI_WARNING, __VA_ARGS__);	\
  } while (0)

/*! Log an error (not dependant of verbosity)
  \param fmt the message to print. Can include C formatting message
  \param args... the list of params for the formatting string
 */
#define exa_cli_error(...) \
  do {									\
    cli_log ( __FILE__, __FUNCTION__, __LINE__, EXA_CLI_ERROR, __VA_ARGS__);	\
  } while (0)


#endif  // _CLI_LOG_H_
