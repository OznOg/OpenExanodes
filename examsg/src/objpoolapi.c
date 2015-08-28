/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "common/include/exa_assert.h"
#include "examsg/src/objpoolapi.h"
#include "os/include/os_thread.h"

#ifdef HAVE_VALGRIND_MEMCHECK_H
# include <valgrind/memcheck.h>
#endif

#define POOL_MAGIC 0x100d1262
/* --- Objects pool -------------------------------------------------- */

/** Examsg object header */
typedef struct ExaMsgObj {
  bool free;        /**< is object free or not */
  size_t size;      /**< object size */
  size_t offset;    /**< object offset address */
} ExaMsgObj;

/** Examsg pool header */
struct ExaMsgReservedObj {
  uint32_t magic;
  size_t size;		              /**< pool size */
  ExaMsgObj objs[EXAMSG_LAST_ID + 1]; /**< Mailboxes informations */
  char data[];                        /**< Data area */
};

ExaMsgReservedObj *examsgPoolInit(void *memory, size_t size)
{
    ExamsgID id;
    ExaMsgReservedObj *pool = memory;

    if (size < sizeof(ExaMsgReservedObj))
	return NULL;

    /* Kill memory content */
    memset(pool, 0xEE, size);

#ifdef HAVE_VALGRIND_MEMCHECK_H
    VALGRIND_MAKE_MEM_UNDEFINED(pool, size);
#endif

    pool->magic = POOL_MAGIC;
    pool->size  = size - sizeof(ExaMsgReservedObj);

    /* initialize objects to empty state */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	pool->objs[id].free = true;
	/* The size 0 means that the mbox was never ever created */
	pool->objs[id].size = 0;
	pool->objs[id].offset = 0;
    }

    return pool;
}

/* --- examsgObjCreate ----------------------------------------------- */

/** \brief Create a new object.
 *
 * In case of success, the new object is left _locked_ so that the caller
 * can perform additional initializations.
 *
 * \param[in] id	Object id.
 * \param[in] size	Object size.
 *
 * \return 0 on success, or a negative error code.
 */

int examsgObjCreate(ExaMsgReservedObj *pool, int id, size_t size)
{
  ExamsgID i;
  size_t offset = 0;

  EXA_ASSERT(pool);
  EXA_ASSERT(OBJ_ID_VALID(id));
  EXA_ASSERT(pool->magic == POOL_MAGIC);

  /* align size on 64bits to be on the safe side */
  if (size & 0x7)
    size = (size | 0x7) + 1;

  /* check this id is free */
  if (!pool->objs[id].free)
      return -EEXIST;

  /* Calculate offset of new object. Object lock is not necessary because object
   * size cannot change dynamically (can just happen in create or delete which
   * are lock held functions */
  /* FIXME this calculation doesn't take into account the fragmentation of the
   * pool which means that if one of the first obj is destroyed but not the
   * others, the complete pool will be corrupted... */
  for (i = EXAMSG_FIRST_ID; i <= EXAMSG_LAST_ID; i++)
  {
      if (i == id)
	  continue;

      /* Offset is set once for all for a given id.
       * Any mailbox that existed once has its own slot and thus has
       * the offset already set; this must be taken into account for new objects */
      offset += pool->objs[i].size;
  }

  if (offset + size > pool->size)
      return -ENOMEM;

  /* initialize new object */
  pool->objs[id].size = size;
  pool->objs[id].offset = offset;

  pool->objs[id].free = false;

  return 0;
}


/* --- examsgObjDelete ----------------------------------------------- */

/** \brief Delete an examsg object.
 *
 * Release storage associated to object \a id.
 *
 * WARNING: Initially, the memory "freed" by the deletion of the object
 * was "garbage collected", i.e. the objects were moved around so that
 * no they were kept contiguous. It is no longer the case.
 *
 * \param[in] id	Object id.
 *
 * \return 0 on success or a negative error code.
 */

int examsgObjDelete(ExaMsgReservedObj *pool, int id)
{
  int elt;
  EXA_ASSERT(OBJ_ID_VALID(id));
  EXA_ASSERT(pool);
  EXA_ASSERT(pool->magic == POOL_MAGIC);

  /* sanity checks */
  if (pool->objs[id].free)
      return -EINVAL;

  /* kill content of memory held by this object */
  memset(pool->data + pool->objs[id].offset, 0xEE, pool->objs[id].size);

  /* destroy object */
  pool->objs[id].free = true;

  /* Defragment memory */

  /* move the whole stuff */
  memmove(pool->data + pool->objs[id].offset,
	  pool->data + pool->objs[id].offset + pool->objs[id].size,
	  pool->size - pool->objs[id].offset - pool->objs[id].size);

  /* Recompute offsets */
  for (elt = EXAMSG_FIRST_ID; elt <= EXAMSG_LAST_ID; elt++)
  {
      if (pool->objs[elt].free)
	  continue;

      /* If elt was after id in the pool, it was shifted of pool->objs[id].size */
      if (pool->objs[elt].offset > pool->objs[id].offset)
	  pool->objs[elt].offset -= pool->objs[id].size;
  }

  pool->objs[id].size = 0;

  return 0;
}

/* --- examsgObjAddr ------------------------------------------------- */

/** \brief Return the address of an object.
 *
 * \param[in] id: object id.
 * \return object address.
 */

void *examsgObjAddr(ExaMsgReservedObj *pool, int id)
{
  EXA_ASSERT(OBJ_ID_VALID(id));

  if (pool->objs[id].free)
    return NULL;

  return pool->data + pool->objs[id].offset;
}

void objpool_show_info(ExaMsgReservedObj *pool)
{
    if (!pool)
	printf("pool not mapped\n\n");
    else
	printf("maximum pool size:\t%" PRIzu "\n"
		"allocated pool size:\t%" PRIzu "\n\n",
		EXAMSG_MPOOL_MAX, pool->size);
}
