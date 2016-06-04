/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADMIND_RPC_H
#define __ADMIND_RPC_H

#include <string.h>

#include "common/include/exa_nodeset.h"
#include "examsg/include/examsg.h"

struct adm_service;

/** Structure to handle RPC replies */
typedef struct admwrk_request_t
{
  ExamsgType type;            /**< identifier of the expected reply type */
  exa_nodeset_t waiting_for;  /**< bitmap of the nodes we are waiting for */
  ExamsgHandle mh;            /**< Examsg handle to use */
  bool (*is_node_down)(exa_nodeid_t nid);
} admwrk_request_t;

typedef struct barrier {
  int           rank;
  exa_nodeset_t nodes;
} barrier_t;

typedef struct admwrk_ctx_t admwrk_ctx_t;

admwrk_ctx_t *admwrk_ctx_alloc();
void admwrk_ctx_free(admwrk_ctx_t *ctx);

void admwrk_handle_localcmd_msg(admwrk_ctx_t *ctx, const Examsg *msg, ExamsgMID *from);

void admwrk_run_command(admwrk_ctx_t *ctx, const struct adm_service *service,
                        admwrk_request_t *handle, int command,
			const void *request, size_t size);
int admwrk_get_ack(admwrk_request_t *handle, exa_nodeid_t *nodeid, int *err);

void admwrk_reply(admwrk_ctx_t *ctx, void *__reply, size_t size);

int admwrk_barrier_msg(admwrk_ctx_t *ctx, int err, const char *step, const char *fmt, ...)
    __attribute__ ((format (printf, 4, 5)));
int  admwrk_exec_command(admwrk_ctx_t *ctx, const struct adm_service *service,
                         int command, const void *request, size_t size);

static inline void admwrk_ack(admwrk_ctx_t *ctx, int err)
{
  admwrk_reply(ctx, &err, sizeof(err));
}

void admwrk_bcast  (admwrk_ctx_t *ctx, admwrk_request_t *handle,
		    int type, const void *out, size_t size);
int admwrk_get_reply(admwrk_request_t *handle, exa_nodeid_t *nodeid,
		     void *reply, size_t size, int *err);
int admwrk_get_bcast(admwrk_request_t *handle, exa_nodeid_t *nodeid,
	             void *reply, size_t size, int *err);

static inline int admwrk_barrier(admwrk_ctx_t *ctx, int err, const char *step)
{
  return admwrk_barrier_msg(ctx, err, step, "%s", "");
}

#endif /* __ADMIND_RPC_H */
