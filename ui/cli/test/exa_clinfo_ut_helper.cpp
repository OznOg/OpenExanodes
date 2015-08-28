/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/common/include/cli_log.h"
#include <sstream>
#include <string.h>
#include <stdarg.h>


std::stringstream __stdout;


/**
 * Another implementation of cli_log allows to redirect output of
 * exa_clinfo::exa_display_volumes_status to stringstream
 */
void cli_log(const char * file, const char * func,
             const int line, exa_cli_log_level_t lvl, const char * fmt, ...)
{
    va_list ap;
    char buf[1024];
    memset(buf, 0, sizeof(buf));

    COLOR_USED = "";
    COLOR_INFO = "";
    COLOR_NORM = "";
    COLOR_ERROR = "";
    COLOR_WARNING = "";
    COLOR_BOLD = "";

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end (ap);

    __stdout << buf;
}

