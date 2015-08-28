/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef TM_FILE_H
#define TM_FILE_H

/* FIXME No need for uint64_t, use unsigned*/

#include "token_manager/tm_server/src/tm_token.h"
#include "os/include/os_inttypes.h"

typedef enum
{
    TM_TOKENS_FILE_FORMAT_VERSION_1 = 1
} tm_tokens_file_version_t;

#define TM_TOKENS_FILE_MAGIC_NUMBER     0xabdd4456bdef5764

#define TM_TOKENS_FILE_FORMAT_VERSION   TM_TOKENS_FILE_FORMAT_VERSION_1

#define TM_TOKENS_FILE_FORMAT_VERSION_IS_VALID(version) \
        ((version) > 0 && (version) <= TM_TOKENS_FILE_FORMAT_VERSION)

/* FIXME Made mandatory that count contains as input the size of the tokens array*/
/**
 * Load all tokens from a token file.
 *
 * NOTE: If the file isn't existing, it is considered normal
 * and zero tokens are loaded.
 *
 * @param[in]     filename  File to load the tokens from
 * @param[out]    tokens    Tokens loaded
 * @param[in,out] count     Input: max number of tokens storable in 'tokens',
 *                          output: tokens loaded
 *
 * @return 0 if successful, a negative error code otherwise
 */
int tm_file_load(const char *filename, token_t *tokens, uint64_t *count);

/**
 * Save tokens to a token file.
 *
 * NOTE: If there are zero tokens to save, the file is unlinked
 * instead of being written empty.
 *
 * NOTE: The directory where the file is saved is created if it
 * doesn't exist.
 *
 * @param[in] filename  File to save the tokens to
 * @param[in] tokens    Tokens to save
 * @param[in] count     Number of tokens
 *
 * @return 0 if successful, a negative error code otherwise
 */
int tm_file_save(const char *filename, const token_t *tokens,
                 uint64_t count);

#endif /* TM_FILE_H */
