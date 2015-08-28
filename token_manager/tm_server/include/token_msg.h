/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef TOKEN_MSG_H
#define TOKEN_MSG_H

#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"

/** Network default ports */
#define TOKEN_MANAGER_DEFAULT_PORT       30263
#define TOKEN_MANAGER_DEFAULT_PRIV_PORT  30264

/** Token operation codes */
typedef enum {
    TOKEN_OP_ACQUIRE,       /**< Acquire the token */
    TOKEN_OP_RELEASE,       /**< Release the token */
    TOKEN_OP_FORCE_RELEASE, /**< Forcefully release the token */
    TOKEN_OP_HEARTBEAT      /**< Heartbeat */
} token_op_t;

#define TOKEN_OP_IS_VALID(op)                                   \
    ((op) >= TOKEN_OP_ACQUIRE && (op) <= TOKEN_OP_HEARTBEAT)

/* XXX Ideally, the sender should be a node UUID instead of a node id */
/** Request sent by client to Token Manager */
typedef struct
{
    token_op_t op;            /**< Operation */
    exa_uuid_t cluster_uuid;  /**< Client UUID */
    exa_nodeid_t sender_id;   /**< Requester id */
} token_request_msg_t;

/** Result of a token operation */
typedef enum
{
    TOKEN_RESULT_DENIED,    /**< The operation was denied */
    TOKEN_RESULT_ACCEPTED   /**< The operation was successful */
} token_result_t;

#define TOKEN_RESULT_IS_VALID(tr) \
    ((tr) == TOKEN_RESULT_DENIED || (tr) == TOKEN_RESULT_ACCEPTED)

/** Reply sent by Token Manager to client */
typedef struct
{
    token_result_t result;
} token_reply_msg_t;

#endif /* TOKEN_MSG_H */
