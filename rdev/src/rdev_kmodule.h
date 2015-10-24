/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef RDEV_KMODULE_H
#define RDEV_KMODULE_H

#define EXA_RDEV_INIT                   0x52
#define EXA_RDEV_FLUSH                  0x53
#define EXA_RDEV_MAKE_REQUEST_NEW       0x54
#define EXA_RDEV_WAIT_ONE_REQUEST       0x55
#define EXA_RDEV_GET_LAST_ERROR         0x56
#define EXA_RDEV_ACTIVATE               0x5b
#define EXA_RDEV_DEACTIVATE             0x5c

#include "rdev/include/exa_rdev.h"

typedef struct {
    void *nbd_private;
} user_land_io_handle_t;

struct exa_rdev_request_kernel
{
    rdev_op_t op;
    long sector;
    long sector_nb;
    void *buffer;
    /** Contains information belonging to the upper component
     *  It MUST NOT be used or modified by the module */
    user_land_io_handle_t h;
}  __attribute__((__packed__)) ;

struct exa_rdev_major_minor
{
    long major;
    long minor;
};

#endif

