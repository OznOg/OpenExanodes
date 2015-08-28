/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** @file
 *  @brief Date and time formatting routines
 */

#include "os/include/os_assert.h"
#include "os/include/os_time.h"
#include "os/include/os_stdio.h"

const char *os_date_msec_to_str(const struct tm *date, int msec)
{
    static char str[sizeof("YYYY-MM-DD hh:mm:ss.iii")];
    int r;

    /* Careful: tm_mon is in the range 0..11, hence tm_mon + 1 */
    r = os_snprintf(str, sizeof(str), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
		    1900 + date->tm_year, date->tm_mon + 1, date->tm_mday,
		    date->tm_hour, date->tm_min, date->tm_sec, msec);

#ifdef DEBUG
    OS_ASSERT_VERBOSE(r < sizeof(str), "date string truncated");
#else
    (void)r;
#endif

    return str;
}
