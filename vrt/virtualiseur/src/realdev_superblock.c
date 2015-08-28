/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/virtualiseur/include/realdev_superblock.h"
#include "os/include/os_error.h"

int rdev_superblock_header_read_both(superblock_header_t headers[2], stream_t *stream)
{
    int r;

    r = stream_read(stream, headers, 2 * sizeof(superblock_header_t));
    if (r < 0)
        return r;
    else if (r != 2 * sizeof(superblock_header_t))
        return -EIO;

    return 0;
}
