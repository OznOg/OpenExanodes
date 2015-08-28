/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef REALDEV_SUPERBLOCK_H
#define REALDEV_SUPERBLOCK_H

/* XXX Shouldn't need a separate module for rdev superblock, but otherwise
   we couldn't link exa_vrt_sbtool properly... */

#include "vrt/common/include/vrt_stream.h"

#include "common/include/checksum.h"

#include "os/include/os_inttypes.h"

typedef enum { SUPERBLOCK_HEADER_MAGIC = 0x99033055 } superblock_header_magic_t;

#define SUPERBLOCK_HEADER_FORMAT  1

typedef struct
{
    /* IMPORTANT - Fields 'magic' and 'format' *MUST* always be 1st and 2nd */
    superblock_header_magic_t magic;
    uint32_t format;       /**< Format of this header */
    uint32_t position;
    uint32_t reserved1;    /**< For future use. Must be zero */
    uint64_t sb_version;
    uint64_t data_max_size;
    uint64_t data_offset;  /**< Relative to beginning of superblock headers */
    uint64_t data_size;    /**< Data size, in bytes */
    checksum_t checksum;   /**< Checksum of the data stored in the superblock */
    char     reserved2[6]; /**< Padding; may be used in future. */
} superblock_header_t;

int rdev_superblock_header_read_both(superblock_header_t headers[2], stream_t *stream);

#endif /* REALDEV_SUPERBLOCK_H */
