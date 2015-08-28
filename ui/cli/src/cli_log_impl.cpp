/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/common/include/cli_log.h"
#include <stdarg.h>

void cli_log(const char *file, const char *func,
             const int line, exa_cli_log_level_t lvl, const char *fmt, ...)
{
    va_list ap;
    FILE *output;

    if (lvl > get_exa_verb())
        return;

    if (lvl <= EXA_CLI_WARNING)
        output = stderr;
    else
        output = stdout;

    va_start(ap, fmt);
    os_colorful_vfprintf(output, fmt, ap);
    va_end(ap);

    fflush(output);
}


