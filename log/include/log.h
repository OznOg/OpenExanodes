/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_EXA_LOG
#define H_EXA_LOG

/** \file
 * Message logging routines
 */

#include "os/include/os_inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/* FIXME EXAMSG_ALL_COMPONENTS is a hack needed for loglevels... */
#define EXAMSG_ALL_COMPONENTS (EXAMSG_LAST_ID + 2)

/* === Logger control ================================================ */

/** Maximum length of log messages */
#define EXALOG_MSG_MAX		((size_t)256)

/** Maximum length of function and file names */
#define EXALOG_NAME_MAX		((size_t)64)
/** Log levels, used to control verbosity */
typedef enum exalog_level_t {
#define EXALOG_LEVEL_FIRST EXALOG_LEVEL_NONE
  EXALOG_LEVEL_NONE = 400,	/**< don't log anything */
  EXALOG_LEVEL_ERROR,		/**< log errors only */
  EXALOG_LEVEL_WARNING,		/**< log warnings and errors */
  EXALOG_LEVEL_INFO,		/**< log info, warnings and errors */
  EXALOG_LEVEL_DEBUG,		/**< log debug, info, warnings and errors */
  EXALOG_LEVEL_TRACE		/**< log everything */
#define EXALOG_LEVEL_LAST EXALOG_LEVEL_TRACE
} exalog_level_t;
#define EXALOG_LEVEL_IS_VALID(l) ((l) >= EXALOG_LEVEL_FIRST && (l) <= EXALOG_LEVEL_LAST)

#define EXALOG_LEVEL_VISIBLE_BY_USER(loglevel) \
  ((loglevel) >= EXALOG_LEVEL_ERROR && (loglevel) <= EXALOG_LEVEL_INFO)

/* === API =========================================================== */
/* This include is for components id.... should definitly be removed */
#include "examsg/include/examsg.h"

/* --- exalog_error -------------------------------------------------- */

/** \brief Log an error message
 *
 * \param[in] fmt	Message format, printf syntax.
 * \param[in] ...	arguments for \a fmt.
 */
#define exalog_error(...)					\
  exalog_text(EXALOG_LEVEL_ERROR,__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)


/* --- exalog_warning ------------------------------------------------ */

/** \brief Log a warning message
 *
 * \param[in] fmt	Message format, printf syntax.
 * \param[in] ...	arguments for \a fmt.
 */
#define exalog_warning(...)					\
  exalog_text(EXALOG_LEVEL_WARNING, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)


/* --- exalog_info --------------------------------------------------- */

/** \brief Log an informational message
 *
 * \param[in] fmt	Message format, printf syntax.
 * \param[in] ...	arguments for \a fmt.
 */
#define exalog_info(...)					\
  exalog_text(EXALOG_LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)


/* --- exalog_trace -------------------------------------------------- */

/** \brief Log an execution trace message
 *
 * \param[in] fmt	Message format, printf syntax.
 * \param[in] ...	arguments for \a fmt.
 */
#define exalog_trace(...)					\
  exalog_text(EXALOG_LEVEL_TRACE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

/* --- exalog_debug -------------------------------------------------- */

/** \brief Log a debugging message
 *
 * \param[in] fmt	Message format, printf syntax.
 * \param[in] ...	arguments for \a fmt.
 */
#define exalog_debug(...)					\
  exalog_text(EXALOG_LEVEL_DEBUG,  __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)


/* --- Low level API -------------------------------------------------- */

void exalog_as(ExamsgID cid);

void exalog_end(void);

void exalog_text(exalog_level_t level, const char *file, const char *func,
		   uint32_t line, const char *fmt, ...)
                   __attribute__ ((__format__ (__printf__, 5, 6)));

int exalog_thread_start(void);
void exalog_thread_stop(void);
int exalog_configure(ExamsgID component, exalog_level_t level);

int exalog_set_hostname(const char *hostname);
int exalog_quit(void);

void exalog_static_init(void);
void exalog_static_clean(void);

#ifdef __cplusplus
}
#endif

#endif /* H_EXA_LOG */
