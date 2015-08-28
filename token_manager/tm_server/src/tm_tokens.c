/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "token_manager/tm_server/include/tm_server.h"
#include "token_manager/tm_server/include/tm_file.h"

#include "token_manager/tm_server/src/tm_tokens.h"
#include "token_manager/tm_server/src/tm_token.h"
#include "token_manager/tm_server/src/tm_err.h"

#include "common/include/exa_constants.h"

#include "os/include/os_assert.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_file.h"
#include "os/include/os_stdio.h"
#include "os/include/os_dir.h"
#include "os/include/os_string.h"
#include "os/include/os_network.h"

#include <stdlib.h>
#include <string.h>  /* for memcpy() */

static token_t tokens[TM_TOKENS_MAX];
static uint64_t num_tokens = 0;

static void __reset_token(token_t *token)
{
    uuid_zero(&token->uuid);
    token->holder = EXA_NODEID_NONE;
    os_strlcpy(token->holder_addr, "", sizeof(token->holder_addr));
}

static void __reset_all(void)
{
    int i;

    for (i = 0; i < TM_TOKENS_MAX; i++)
        __reset_token(&tokens[i]);

    num_tokens = 0;
}

void tm_tokens_init(void)
{
    __reset_all();
}

void tm_tokens_cleanup(void)
{
    __reset_all();
}

static token_t *__add_token(const exa_uuid_t *uuid)
{
    token_t *t;
    int i;

    for (i = 0; i < TM_TOKENS_MAX; i++)
    {
        OS_ASSERT(!uuid_is_equal(&tokens[i].uuid, uuid));
        if (uuid_is_zero(&tokens[i].uuid))
            break;
    }

    if (i >= TM_TOKENS_MAX)
        return NULL;

    t = &tokens[i];
    uuid_copy(&t->uuid, uuid);
    t->holder = EXA_NODEID_NONE;
    os_strlcpy(t->holder_addr, "", sizeof(t->holder_addr));

    num_tokens++;

    return t;
}

static token_t *__find_token(const exa_uuid_t *uuid)
{
    int i;

    for (i = 0; i < TM_TOKENS_MAX; i++)
        if (uuid_is_equal(&tokens[i].uuid, uuid))
            return &tokens[i];

    return NULL;
}

int tm_tokens_set_holder(const exa_uuid_t *uuid, exa_nodeid_t node_id,
                         const os_net_addr_str_t node_addr)
{
    token_t *t;

    OS_ASSERT(uuid != NULL && !uuid_is_zero(uuid));
    OS_ASSERT(node_id != EXA_NODEID_NONE);
    OS_ASSERT(node_addr != NULL && os_net_ip_is_valid(node_addr));

    t = __find_token(uuid);
    if (t == NULL)
    {
        t = __add_token(uuid);
        if (t == NULL)
            return -TM_ERR_TOO_MANY_TOKENS;
    }

    if (t->holder != EXA_NODEID_NONE && t->holder != node_id)
        return -TM_ERR_ANOTHER_HOLDER;

    t->holder = node_id;
    os_strlcpy(t->holder_addr, node_addr, sizeof(t->holder_addr));

    return 0;
}

int tm_tokens_get_holder(const exa_uuid_t *uuid, exa_nodeid_t *node_id)
{
    token_t *t;

    OS_ASSERT(uuid != NULL && !uuid_is_zero(uuid));

    t = __find_token(uuid);
    if (t == NULL)
        return -TM_ERR_NO_SUCH_TOKEN;

    *node_id = t->holder;
    return 0;
}

int tm_tokens_release(const exa_uuid_t *uuid, exa_nodeid_t node_id)
{
    token_t *t;

    OS_ASSERT(uuid != NULL && !uuid_is_zero(uuid));
    OS_ASSERT(node_id != EXA_NODEID_NONE);

    t = __find_token(uuid);
    if (t == NULL)
        return -TM_ERR_NO_SUCH_TOKEN;

    if (t->holder != node_id)
        return -TM_ERR_NOT_HOLDER;

    __reset_token(t);
    num_tokens--;

    return 0;
}

void tm_tokens_force_release(const exa_uuid_t *uuid)
{
    token_t *t;

    OS_ASSERT(uuid != NULL && !uuid_is_zero(uuid));

    t = __find_token(uuid);
    if (t != NULL)
    {
        __reset_token(t);
        num_tokens--;
    }
}

uint64_t tm_tokens_count(void)
{
    return num_tokens;
}

int tm_tokens_load(const char *filename)
{
    uint64_t n = TM_TOKENS_MAX;
    int err;

    err = tm_file_load(filename, tokens, &n);
    if (err != 0)
        return err;

    num_tokens = n;
    return 0;
}

int tm_tokens_save(const char *filename)
{
    return tm_file_save(filename, tokens, TM_TOKENS_MAX);
}
