/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __ADM_ERROR_H
#define __ADM_ERROR_H

#include "common/include/exa_error.h"
#include "os/include/strlcpy.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct cl_error_desc
{
    char           msg[EXA_MAXSIZE_ERR_MESSAGE + 1];
    exa_error_code code;
} cl_error_desc_t;

#define set_success(error_desc)                 \
    set_error((error_desc), EXA_SUCCESS, NULL)

static inline void
set_error(cl_error_desc_t *error_desc, exa_error_code error_code, const char *fmt, ...)
{
    va_list ap;

    if (!error_desc)
        return;

    if (fmt != NULL)
    {
        /* Get the error string */
        va_start(ap, fmt);
        vsnprintf(error_desc->msg, sizeof(error_desc->msg), fmt, ap);
        va_end(ap);
    }
    else
    {
        /* XXX We shouldn't do that since it means code that print error
               messages may end up printing the same string twice.

               Instead we should have an accessor to build an error string
               from the combination of a generic error string (determined by
               the error code) and a "detail" string (from the formatted
               arguments, if any).

               *But* a simpler solution for now is to pass "" as detail
               string (ie as 'fmt') instead of NULL.
        */
        strlcpy(error_desc->msg, exa_error_msg(error_code),
                sizeof(error_desc->msg));
    }

    error_desc->code = error_code;
}

#endif
