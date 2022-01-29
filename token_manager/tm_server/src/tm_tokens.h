/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef TM_TOKENS_H
#define TM_TOKENS_H

#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

#include "os/include/os_network.h"

/**
 * Initialize token management.
 */
void tm_tokens_init(void);

/**
 * Cleanup token management.
 */
void tm_tokens_cleanup(void);

/**
 * Set the holder of a token.
 *
 * NOTE: Tokens are *not* persisted by this function.
 *
 * @param[in] uuid       UUID of token
 * @param[in] node_id    Id of node willing to get the token
 * @param[in] node_addr  IP address of the node
 *
 * @return 0 if successful,
 *         -TM_ERR_TOO_MANY_TOKENS if no more token can be defined,
 *         -TM_ERR_ANOTHER_HOLDER if another node holds the token
 */
int tm_tokens_set_holder(const exa_uuid_t *uuid, exa_nodeid_t node_id,
                         const char *node_addr);

/**
 * Get the holder of a token.
 *
 * @param[in]  uuid       UUID of token
 * @param[out] holder_id  Node id of holder
 *
 * @return 0 if successful, -TM_ERR_NO_SUCH_TOKEN if the token doesn't exist
 */
int tm_tokens_get_holder(const exa_uuid_t *uuid, exa_nodeid_t *holder_id);

/**
 * Release a token.
 *
 * NOTE: Tokens are *not* persisted by this function.
 *
 * @param[in] uuid     UUID of token
 * @param[in] node_id  Id of node willing to release the token
 *
 * @return 0 if successful,
 *         -TM_ERR_NO_SUCH_TOKEN if the token doesn't exist (is not held by
 *                               anyone),
 *         -TM_ERR_NOT_HOLDER if the node doesn't hold the token and thus
 *                            is not allowed to release it
 */
int tm_tokens_release(const exa_uuid_t *uuid, exa_nodeid_t node_id);

/**
 * Forcefully release a token.
 *
 * NOTE: Tokens are *not* persisted by this function.
 *
 * @param[in] uuid  UUID of token
 */
void tm_tokens_force_release(const exa_uuid_t *uuid);

/**
 * Get the number of tokens managed.
 *
 * @return Number of tokens
 */
uint64_t tm_tokens_count(void);

/**
 * Load all tokens.
 *
 * NOTE: If the file isn't existing, it is considered normal
 * and zero tokens are loaded.
 *
 * @param[in] filename  File to load the tokens from
 *
 * @return 0 if successful, a negative error code otherwise
 */
int tm_tokens_load(const char *filename);

/**
 * Save all tokens.
 *
 * NOTE: If there are zero tokens to save, the file is unlinked
 * instead of being written empty.
 *
 * NOTE: The directory where the file is saved is created if it
 * doesn't exist
 *
 * @param[in] filename  File to save the tokens to
 *
 * @return 0 if successful, a negative error code otherwise
 */
int tm_tokens_save(const char *filename);

#endif /* TM_TOKENS_H */
