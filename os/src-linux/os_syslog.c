/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdarg.h>
#include <syslog.h>

#include "os/include/os_syslog.h"

/* Don't use OS_ASSERT() in this file as OS_ASSERT() uses os_syslog() */

static bool opened = false;

void os_openlog(const char *ident)
{
    openlog(ident, LOG_NDELAY, LOG_USER);
    opened = true;
}

void os_closelog()
{
    closelog();
    opened = false;
}

void os_syslog(os_syslog_level_t level, const char *format, ...)
{
    int priority = 0;
    va_list ap;

    if (!opened)
        os_openlog("unknown");

    switch (level)
    {
    case OS_SYSLOG_ERROR:
        priority = LOG_ERR;
        break;
    case OS_SYSLOG_WARNING:
        priority = LOG_WARNING;
        break;
    case OS_SYSLOG_INFO:
        priority = LOG_INFO;
        break;
    case OS_SYSLOG_DEBUG:
        priority = LOG_DEBUG;
        break;
    }

    va_start(ap, format);
    vsyslog(priority, format, ap);
    va_end(ap);
}
