/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __OBJPOOLAPI_H
#define __OBJPOOLAPI_H

#include "examsg/include/examsg.h"
#include "common/include/exa_constants.h"

#include <sys/types.h>

/** Maximum memory pool size per node in bytes (1.5MB) */
#define EXAMSG_MPOOL_MAX  ((size_t)(1550 * 1024))

/** Raw size of memory: object pool header + memory pool size. */
#define EXAMSG_RAWSIZE (EXAMSG_MPOOL_MAX)

/** Check validity of an object id */
#define OBJ_ID_VALID(id) \
  ((id) >= EXAMSG_FIRST_ID && (id) <= EXAMSG_LAST_ID)

typedef struct ExaMsgReservedObj ExaMsgReservedObj;

/** Special 'no event' value for event counters */
#define EXAMSG_EVNOVALUE ((uint32_t)-1)

ExaMsgReservedObj *examsgPoolInit(void *memory, size_t size);
int examsgObjCreate(ExaMsgReservedObj *pool, int id, size_t size);
int examsgObjDelete(ExaMsgReservedObj *pool, int id);
void *examsgObjAddr(ExaMsgReservedObj *pool, int id);
void objpool_show_info(ExaMsgReservedObj *pool);
#endif
