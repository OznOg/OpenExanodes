/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_SYSLOG_H
#define _OS_SYSLOG_H

#include "os_inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OS_SYSLOG_ERROR,
    OS_SYSLOG_WARNING,
    OS_SYSLOG_INFO,
    OS_SYSLOG_DEBUG
} os_syslog_level_t;

/**
 * Open the system log.
 *
 * Must be called prior to using os_syslog(). If the system log was already
 * open, it is closed prior to being opened again with the specified
 * identifier.
 *
 * @param[in] ident  Identifier to log messages with
 *
 * @os_replace{Linux, openlog}
 * @os_replace{Windows, OpenEventLog}
 */
void os_openlog(const char *ident);

/**
 * Close the system log.
 * Must be called once done with logging.
 *
 * @os_replace{Linux, closelog}
 * @os_replace{Windows, CloseEventLog}
 */
void os_closelog(void);

/**
 * Write a message to the system log.
 * The log must have been opened with os_openlog() before, otherwise messages
 * are logged with identifier "unknown".
 *
 * @param[in] level   The message's log level
 * @param[in] format  Message format
 * @param[in] ...     Optional data
 *
 * @os_replace{Linux, syslog, vsyslog}
 * @os_replace{Windows, ReportEvent}
 */
void os_syslog(os_syslog_level_t level, const char *format, ...)
#ifndef WIN32
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;

#ifdef __cplusplus
}
#endif

#endif /* _OS_SYSLOG_H */
