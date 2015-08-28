/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdarg.h>

#include "os/include/os_stdio.h"

int os_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = vsnprintf(str, size, format, ap);
    va_end(ap);

    return ret;
}

int os_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf(str, size, format, ap);
}

int os_colorful_fprintf(FILE *stream, const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = vfprintf(stream, format, ap);
    va_end(ap);

    return ret;
}

int os_colorful_vfprintf(FILE *stream, const char *format, va_list ap)
{
    return vfprintf(stream, format, ap);
}

