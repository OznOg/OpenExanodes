/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>
#include <string.h>
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "common/include/exa_assert.h"

#include "os/include/os_shm.h"
#include "os/include/os_ipc_sem.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"

#include "objpoolapi.h"
#include "mailbox.h"
#include "ringbuf.h"

/* Maximum number of mailboxes handled by a mboxset; 2 because for now,
 * only examsgd uses this facility, and it needs only 2 */
#define MBOXSET_SIZE 2

#define MBOX_EMPTY_ID ((ExamsgID)(EXAMSG_LAST_ID + 1))

/** Ids contained in the mboxes set MUST be sorted because they
 * are used in the array order to take locks. As locks MUST always
 * be taken and released in the same order, the array MUST remain
 * sorted */
struct mbox_set {
    ExamsgID mboxes[MBOXSET_SIZE];
};

struct exa_mbox {
    ExamsgID owner;   /* Componet that owns the mbox */
    uint32_t read_count;
    uint32_t received_count;
    bool watchers[EXAMSG_LAST_ID + 1]; /**< Who is watching this mbox */
    exa_ringbuf_t rng[];
};
typedef struct exa_mbox exa_mbox_t;

/* sleeping semaphores */
static os_ipc_semset_t *wait_sems;
#define EXAMSG_IPC_EVENT_WAIT_KEY 0xE7A51237

/* Mutexes on mboxes */
static os_ipc_semset_t *mboxes_locks;
#define EXAMSG_IPC_OBJ_LOCKS_KEY 0xE7A51236

/** Pool and shm_pool are unique for each process (ie share between all threads
 * of a given process). There is NO LOCKING on this data, this means that
 * the caller is responsible for calling examsgMboxCreateAll (or examsgMboxMapAll)
 * once (and only once) in the process that needs it. */
#define EXAMSG_SHMPOOL_ID "exanodes-msg"
static os_shm_t *shm_pool = NULL;
static ExaMsgReservedObj *pool = NULL;

static void mbox_lock(int id)
{
    EXA_ASSERT_VERBOSE(OBJ_ID_VALID(id), "Invalid id: %d", id);
    os_ipc_sem_down(mboxes_locks, id);
}

static void mbox_unlock(int id)
{
    EXA_ASSERT_VERBOSE(OBJ_ID_VALID(id), "Invalid id: %d", id);
    os_ipc_sem_up(mboxes_locks, id);
}

static void mbox_unlock_all(void)
{
    ExamsgID id;

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
	mbox_unlock(id);
}

static void mbox_lock_all(void)
{
    ExamsgID id;

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
	mbox_lock(id);
}

int examsgMboxCreateAll(void)
{
    ExamsgID id;
    void *memory;

    EXA_ASSERT(!shm_pool);

    shm_pool = os_shm_create(EXAMSG_SHMPOOL_ID, EXAMSG_RAWSIZE);
    if (!shm_pool)
	return -ENOMEM;

    memory = os_shm_get_data(shm_pool);
    EXA_ASSERT(memory);

    /* Initialize the pool Metadata */
    pool = examsgPoolInit(memory, EXAMSG_RAWSIZE);

    mboxes_locks = os_ipc_semset_create(EXAMSG_IPC_OBJ_LOCKS_KEY, EXAMSG_LAST_ID + 1);
    EXA_ASSERT(mboxes_locks);

    wait_sems = os_ipc_semset_create(EXAMSG_IPC_EVENT_WAIT_KEY, EXAMSG_LAST_ID + 1);
    EXA_ASSERT(wait_sems);

    /* Unlock Mboxes */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
	os_ipc_sem_up(mboxes_locks, id);

    return 0;
}

void examsgMboxDeleteAll(void)
{
    os_shm_delete(shm_pool);
    os_ipc_semset_delete(mboxes_locks);
    os_ipc_semset_delete(wait_sems);
    shm_pool = NULL;
    pool = NULL;
    mboxes_locks = NULL;
    wait_sems = NULL;
}

int examsgMboxMapAll(void)
{
    EXA_ASSERT(!shm_pool);

    shm_pool = os_shm_get(EXAMSG_SHMPOOL_ID, EXAMSG_RAWSIZE);
    if (!shm_pool)
	return -ENOMEM;

    pool = os_shm_get_data(shm_pool);
    EXA_ASSERT(pool);

    mboxes_locks = os_ipc_semset_get(EXAMSG_IPC_OBJ_LOCKS_KEY, EXAMSG_LAST_ID + 1);
    EXA_ASSERT(mboxes_locks);

    wait_sems = os_ipc_semset_get(EXAMSG_IPC_EVENT_WAIT_KEY, EXAMSG_LAST_ID + 1);
    EXA_ASSERT(wait_sems);

    return 0;
}

void examsgMboxUnmapAll(void)
{
    EXA_ASSERT(shm_pool);

    os_shm_release(shm_pool);
    os_ipc_semset_release(mboxes_locks);
    os_ipc_semset_release(wait_sems);
    shm_pool = NULL;
    pool = NULL;
    mboxes_locks = NULL;
    wait_sems = NULL;
}

/* examsgMboxMemSize
 * \brief Return the actual size needed to store a mailbox of size \a
 * size
 *
 * \param[in] num_msg   Maximum number of messages
 * \param[in] msg_size  Maximum message size
 *
 * \return the required storage size.
 */
static inline size_t
examsgMboxMemSize(size_t num_msg, size_t size)
{
    return sizeof(exa_mbox_t) + examsgRngMemSize(num_msg, size);
}

int examsgMboxCreate(ExamsgID owner, ExamsgID id, size_t num_msg, size_t msg_size)
{
  ExamsgID i;
  exa_mbox_t *mb;
  int s;

  exalog_trace("num_msg = %" PRIzu ", msg_size = %" PRIzu ","
	       " blkhead = %" PRIzu ", blktail = %" PRIzu
	       " => MboxMemSize = %" PRIzu ", RngMemSize = %" PRIzu, num_msg, msg_size,
	       sizeof(struct examsg_blkhead), sizeof(examsg_blktail),
	       examsgMboxMemSize(num_msg, msg_size),
	       examsgRngMemSize(num_msg, msg_size));

  /* Lock all object while we change pool configuration
   * Please lock and unlock alway in the same order to prevent races */
  mbox_lock_all();

  /* create object */
  s = examsgObjCreate(pool, id, examsgMboxMemSize(num_msg, msg_size));
  if (s)
  {
      mbox_unlock_all();
      return s;
  }

  /* initialize structure */
  mb = examsgObjAddr(pool, id);
  EXA_ASSERT(mb);

  examsgRngInit(mb->rng, examsgRngMemSize(num_msg, msg_size));

  for (i = EXAMSG_FIRST_ID; i <= EXAMSG_LAST_ID; i++)
      mb->watchers[i] = false;

  mb->read_count = 0;
  mb->received_count = 0;
  mb->owner = owner;

  mbox_unlock_all();

  return 0;
}

int examsgMboxDelete(ExamsgID id)
{
    int ret;

    /* Lock all while changing the whole pool configuration */
    mbox_lock_all();

    /* destroy storage */
    ret = examsgObjDelete(pool, id);

    mbox_unlock_all();

    return ret;
}

int __examsgMboxSend(ExamsgID from, ExamsgID to, ExamsgFlags flags, ...)
{
  uint32_t wakeup_events = 0;
  ExamsgID dest_owner;
  va_list _ap;
  exa_mbox_t *mbox;
  int n;

  mbox_lock(to);

  /* get mailbox (this makes sure it was not deleted while we were waiting) */
  mbox = examsgObjAddr(pool, to);
  if (!mbox)
  {
      mbox_unlock(to);
      return -ENXIO;
  }

  va_start(_ap, flags);

  do {
      va_list ap;
      va_copy(ap, _ap);

      /* copy message */
      n = examsgRngPutVaList(mbox->rng, ap);

      va_end(ap);

      if (n == -ENOSPC && !(flags & EXAMSGF_NOBLOCK))
      {
	  uint32_t read_count;

	  /* Tell we want to be waked up on recv */
	  mbox->watchers[from] = true;

	  /* Get the current counter on this very box */
	  read_count = mbox->read_count;

	  do {
	      mbox_unlock(to);

	      /* Go to sleep while the mailbox we are waiting on did not do
	       * a recv */
	      os_ipc_sem_down(wait_sems, from);

	      mbox_lock(to);

	      mbox = examsgObjAddr(pool, to);
	      if (!mbox)
	      {
		  mbox_unlock(to);
		  return -ENXIO;
	      }

	      /* In case we get woke up and nothing changed here, this
	       * means that we were notified for a message reception in our
	       * mailbox, thus we have to remember it */
	      if (read_count == mbox->read_count)
		  wakeup_events++;

            /* Continue the loop until the recipent read something */
	  } while (read_count == mbox->read_count);

	  mbox->watchers[from] = false;
      }
  } while (n == -ENOSPC && !(flags & EXAMSGF_NOBLOCK));

  va_end(_ap);

  /* Notify the message in the destination box only if no erro was reported */
  if (n >= 0)
  {
      /* The received_count is used to allow MboxWait to return IIF there is
       * really something to read. This is a kind of optimization, this is
       * not known to be necessary. */
      mbox->received_count++;
      dest_owner = mbox->owner;

      mbox_unlock(to);

      /* Wake up the owner of the destination mbox */
      os_ipc_sem_up(wait_sems, dest_owner);
  }
  else /* simply unlock mbox on error */
      mbox_unlock(to);

  while (wakeup_events)
  {
      wakeup_events--;
      /* restore events that were swallowed while waiting in
       * order not to forget to read our mailbox */
      os_ipc_sem_up(wait_sems, from);
  }

  return n;
}

int examsgMboxRecv(ExamsgID id, void *mid, size_t mid_size,
	           void *buffer, size_t maxbytes)
{
  exa_mbox_t *mbox;
  ExamsgID i;
  int s;

  EXA_ASSERT(mid);

  mbox_lock(id);

  mbox = examsgObjAddr(pool, id);
  if (!mbox)
    {
      mbox_unlock(id);
      return -ENXIO;
    }

  /* read message */
  s = examsgRngGet(mbox->rng, mid, mid_size, buffer, maxbytes);

  /* signal something was actually received */
  mbox->read_count++;

  /* Wake up watchers */
  for (i = EXAMSG_FIRST_ID; i <= EXAMSG_LAST_ID; i++)
    if (mbox->watchers[i])
	os_ipc_sem_up(wait_sems, i);

  mbox_unlock(id);

  /* An error occurred or nothing to read */
  if (s <= 0)
    return s;

  /* Stored data MUST be at least a mid */
  EXA_ASSERT(s > sizeof(ExamsgMID));

  return s;
}

int examsgMboxWait(ExamsgID waiter, const mbox_set_t *mbox_set,
	           struct timeval *timeout)
{
    bool changed = false;
    int err = 0;

    /* Go to bed */
    do {
	int i;

	err = os_ipc_sem_down_timeout(wait_sems, waiter, timeout);
	EXA_ASSERT(err == 0 || err == -EINTR || (timeout && err == -ETIME));
	if (err)
	    break;

	/* Lock all mboxes we are watching */
	/* FIXME There is no guaranty that the mbox is not deleted meanwhile...
	 * Actually this cannot happen because the caller is the owner, but
	 * this could fail one day... */
	for (i = 0; i < MBOXSET_SIZE; i++)
	    if (mbox_set->mboxes[i] != MBOX_EMPTY_ID)
		mbox_lock(mbox_set->mboxes[i]);

	/* Check if one of them changed */
	for (i = 0; i < MBOXSET_SIZE; i++)
	    if (mbox_set->mboxes[i] != MBOX_EMPTY_ID)
	    {
		ExamsgID id = mbox_set->mboxes[i];
		const exa_mbox_t *mb = examsgObjAddr(pool, id);
		/* make sure owner is waiter */
		EXA_ASSERT_VERBOSE(mb && mb->owner == waiter,
			"%d not allowed to wait on %d (should be %d)",
			waiter, id, mb->owner);

		if (mb->read_count != mb->received_count)
		    changed = true;
	    }

	/* Unlock boxes */
	for (i = 0; i < MBOXSET_SIZE; i++)
	    if (mbox_set->mboxes[i] != MBOX_EMPTY_ID)
		mbox_unlock(mbox_set->mboxes[i]);

	/* A new message was received meanwhile */
    } while (!changed);

    return err;
}

void examsgMboxShowStats(void)
{
    ExamsgID id;
    printf("Event wait key = 0x%X\n", EXAMSG_IPC_EVENT_WAIT_KEY);
    printf("Objects locks key = 0x%X\n", EXAMSG_IPC_OBJ_LOCKS_KEY);
    printf("Shared memory id = '%s'\n", EXAMSG_SHMPOOL_ID);

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	unsigned long pct;
	int i;
	const ExaRingBufStat *stats;
	exa_ringinfo_t rng_info;
	const exa_mbox_t *mb;

	mbox_lock(id);

	mb = examsgObjAddr(pool, id);

	if (!mb)
	{
	    mbox_unlock(id);
	    printf("Mailbox '%s' not created\n", examsgIdToName(id));
	    continue;
	}

	examsgRngGetInfo(mb->rng, &rng_info);

	mbox_unlock(id);
	stats = &rng_info.stats;

	printf("Mailbox '%s'\n", examsgIdToName(id));
	printf("\tsize: %" PRIzu "\n", rng_info.ring_size);
	printf("\tavailable: %" PRIzu "\n", rng_info.available);

	printf("\ttotal number of messages: %lu\n", stats->msg_count);
	printf("\taverage message size: %" PRIzu "\n", stats->msg_avg_size);
	printf("\tminimum message size: %" PRIzu "\n", stats->msg_min_size);
	printf("\tmaximum message size: %" PRIzu "\n", stats->msg_max_size);

	for (i = 0; i < EXAMSG_STATS_MAX_FILL_LEVELS; i++)
	{
	    unsigned long count = stats->fill_level_count[i];

	    pct = stats->msg_count > 0 ? count * 100 / stats->msg_count : 0;

	    printf("\tfill level %u%% .. %u%% : %lu times ~ %lu%%\n",
		    i > 0 ? examsg_stats_fill_levels[i - 1] : 0,
		    examsg_stats_fill_levels[i], count, pct);
	}

	pct = stats->msg_count > 0 ?
	    stats->full_count * 100 / stats->msg_count : 0;

	printf("\tno room: %lu times ~ %lu%%\n", stats->full_count, pct);
    }
}


static int id_cmp(const void *_id1, const void *_id2)
{
    ExamsgID id1 = *(ExamsgID *)_id1;
    ExamsgID id2 = *(ExamsgID *)_id2;

    return id1 - id2;
}

void mboxset_add(mbox_set_t *box_set, ExamsgID id)
{
    int i = 0;

    EXA_ASSERT(EXAMSG_ID_VALID(id));

    while (i < MBOXSET_SIZE && box_set->mboxes[i] != MBOX_EMPTY_ID)
	i++;

    EXA_ASSERT_VERBOSE(i < MBOXSET_SIZE,
	    "Mailbox %d cannot be added: there are more than %d mailboxes.",
	    id, MBOXSET_SIZE);

    box_set->mboxes[i] = id;

    /* mbox set MUST remain sorted, see comment on struct mbox_set */
    qsort(box_set->mboxes, MBOXSET_SIZE, sizeof(*box_set->mboxes), id_cmp);
}

void mboxset_del(mbox_set_t *box_set, ExamsgID id)
{
    int i = 0;

    EXA_ASSERT(EXAMSG_ID_VALID(id));

    while (i < MBOXSET_SIZE && (box_set)->mboxes[i] != id)
	i++;

    EXA_ASSERT_VERBOSE(i < MBOXSET_SIZE,
	    "Mailbox of id %d is not managed here.", id);

    box_set->mboxes[i] = MBOX_EMPTY_ID;

    /* mbox set MUST remain sorted, see comment on struct mbox_set */
    qsort(box_set->mboxes, MBOXSET_SIZE, sizeof(*box_set->mboxes), id_cmp);
}

mbox_set_t *mboxset_alloc(const char *file, unsigned int line)
{
    mbox_set_t *box_set = os_malloc_trace(sizeof(mbox_set_t), file, line);
    int i;

    if (!box_set)
	return NULL;

    for (i = 0; i < MBOXSET_SIZE; i++)
	box_set->mboxes[i] = MBOX_EMPTY_ID;

    return box_set;
}

void mboxset_free(mbox_set_t *box_set)
{
   os_free(box_set);
}

