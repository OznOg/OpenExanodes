 /*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef TM_CLIENT_H
#define TM_CLIENT_H

#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

/* Needed for the -errno returned by the functions hereafter. */
#include "os/include/os_error.h"

/** A token manager */
typedef struct token_manager token_manager_t;

/**
 * Is the given token manager vonnected.
 *
 * @param[in] tm   Token manager
 *
 * return true if connected to the token manager false if not.
 */
bool tm_is_connected(const token_manager_t *tm);

/**
 * Init a token manager handle.
 *
 * NOTE: The caller *must* initialize networking by calling os_net_init()
 * prior to calling this function.
 *
 * @param[out] tm       Token manager connected to
 * @param[in]  ip_addr  IP address of token manager
 * @param[in]  port     Port of token manager
 *
 * @return 0 if successful, a negative error code otherwise
 */
int tm_init(token_manager_t **tm, const char *ip_addr, unsigned short port);

/**
 * Connect to a token manager
 *
 * @param tm    the token manager handle
 *
 * @return  0 if successful, a negative error code otherwise
 */
int tm_connect (token_manager_t *tm);

/**
 * Disconnect from a token manager
 *
 * @param tm    the token manager handle
 */
void tm_disconnect (token_manager_t *tm);

/**
 * Free a token manager.
 *
 * NOTE: This does *not* release the token the caller may hold.
 *
 * @param[in,out] tm  Token manager to free; it is freed
 *                    and reset to NULL.
 */
void tm_free(token_manager_t **tm);

/**
 * Verify the connection's liveliness.
 *
 * @param[in] tm    The token manager
 * @param[in] uuid  UUID of token
 * @param[in]     node_id  Id of caller
 *
 * @return 0 if the connection is alive, a negative error code otherwise.
 */
int tm_check_connection(token_manager_t *tm, const exa_uuid_t *uuid,
                     exa_nodeid_t node_id);

/**
 * Request (acquire) a token.
 *
 * The caller was attributed the token *iff* the function succeeds.
 *
 * @param[in,out] tm       Token manager to request a token from
 * @param[in]     uuid     UUID of token to request
 * @param[in]     node_id  Id of caller
 *
 * @return 0 if successful, a negative error code otherwise
 */
int tm_request_token(token_manager_t *tm, const exa_uuid_t *uuid,
                     exa_nodeid_t node_id);

/**
 * Release a token.
 *
 * The token was released *iff* the function succeeds.
 * (Not actually true if the function fails when receiving the token
 * manager's reply!)
 *
 * @param[in,out] tm       Token manager to release the token to
 * @param[in]     uuid     UUID of token to release
 * @param[in]     node_id  Id of caller
 *
 * @return 0 if successful, a negative error code otherwise
 */
int tm_release_token(token_manager_t *tm, const exa_uuid_t *uuid,
                     exa_nodeid_t node_id);

/**
 * Force the release of a token.
 *
 * @param[in,out] tm    Token manager to release the token to
 * @param[in]     uuid  UUID of token to release
 *
 * @return 0 if successful, a negative error code otherwise
 */
int tm_force_token_release(token_manager_t *tm, const exa_uuid_t *uuid);

#endif /* TM_CLIENT_H */
