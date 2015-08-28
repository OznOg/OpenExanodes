/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXA_CHECKSUM_H
#define EXA_CHECKSUM_H

#include "os/include/os_inttypes.h"

#ifdef __cplusplus
#include <iostream>

extern "C" {
#endif

/** A checksum */
typedef uint16_t checksum_t;

/** Format to use for printing a checksum */
#define CHECKSUM_FMT  "0x%04x"

/**
 * Compute checksum of data.
 *
 * Size must be even (that is, the buffer to checksum must have an
 * integer number of 16-bit words).
 *
 * The checksum can be stored inside the checksummed buffer itself.
 * In this case, the buffer contains a checksum_t, which must be set
 * to 0 when computing the checksum, and when verifying the checksum,
 * the function returns 0.
 *
 * NOTE: Be sure your buffer is either packed, or it has been memset to
 * a non-random value to prevent the compiler-generated padding to add
 * random bytes between structure fields.
 *
 * @param[in] buffer  Pointer to the data to checksum.
 * @param[in] size    Size of the superblock
 *
 * @return        Computed checksum.
 */
checksum_t exa_checksum(const void *buffer, size_t size);

/**
 * Checksumming context.
 * All fields are private; do not use them directly.
 */
typedef struct
{
    size_t total_size;     /**< Total size checksummed, in bytes */
    uint8_t latched_byte;  /**< Latched byte (for odd-sized buffers */
    bool latched;          /**< Whether a byte is latched */
    uint32_t sum;          /**< Checksum value (only valid at end) */
} checksum_context_t;

/**
 * Begin a new checksum computation.
 *
 * @param ctx  Checksumming context to initialize
 */
void checksum_reset(checksum_context_t *ctx);

/**
 * Feed a buffer to a checksum computation.
 *
 * - Cannot be called after the checksum computation is ended with
 *   checksum_end().
 * - Does not do anything if the buffer is NULL or the size is zero.
 *
 * @param     ctx     Checksumming context
 * @param[in] buffer  Buffer to process
 * @param[in] size    Size of buffer, in bytes
 */
void checksum_feed(checksum_context_t *ctx, const void *buffer, size_t size);

/**
 * Result of a checksum computation.
 *
 * @param[in] ctx  Checksumming context
 *
 * @return computed checksum
 */
checksum_t checksum_get_value(const checksum_context_t *ctx);

/**
 * Size of the data that was checksumed.
 *
 * @param[in] ctx  Checksumming context
 *
 * @return the size
 */
size_t checksum_get_size(const checksum_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* EXA_CHECKSUM_H */
