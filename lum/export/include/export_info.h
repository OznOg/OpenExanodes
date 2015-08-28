/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXPORT_INFO_H
#define __EXPORT_INFO_H
#include "os/include/os_inttypes.h"

typedef struct
{
    /** No writes on this volumes */
    bool readonly;

    /** Whether the export is in use */
    /* FIXME Use IN_USE/NOT_IN_USE/UNKNOWN_USE instead of bool?
             (see export_stuff.h) */
    bool in_use;
} lum_export_info_reply_t;

#endif
