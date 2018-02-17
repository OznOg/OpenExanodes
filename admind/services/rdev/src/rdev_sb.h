/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef RDEV_SB_H
#define RDEV_SB_H

#include "common/include/uuid.h"
#include "os/include/os_assert.h"

/* FIXME Some entities use 'superblock' in full, some use the abbreviated
         'sb' form... Consistency ought to be fixed */

/** Size of an rdev superblock, in bytes */
#define RDEV_SUPERBLOCK_SIZE  4096

#if RDEV_SUPERBLOCK_SIZE & 511
#error "rdev superblock size must be a multiple of 512 due to Windows constraint"
#endif

/** Magic of an Rdev superblock */
#define EXA_RDEV_SB_MAGIC "EXANODES DISK\0\0"

/** Rdev superblock at the beginning of an Exanodes disk */
typedef struct
{
    char magic[sizeof(EXA_RDEV_SB_MAGIC)]; /**< Must contain EXA_RDEV_SB_MAGIC */
    exa_uuid_t uuid;                       /**< UUID of the disk */
} rdev_superblock_t;

/*
 * Dummy function just here to be able to use static assert
 */
static inline void exa_rdev_test_sizeof_magic(void)
{
    rdev_superblock_t dummy;
    /* For alignment purpose, the magic MUST be 16 bytes */
    COMPILE_TIME_ASSERT(sizeof(dummy.magic) == 16);
}

#endif /* RDEV_SB_H */
