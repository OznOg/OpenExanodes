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
#include "os/include/os_stdio.h"

#ifdef HAVE_VALGRIND_MEMCHECK_H
# include <valgrind/memcheck.h>
#endif


#include "log/include/log.h"

#include "ringbuf.h"

/* Rate steps for statistics in percent */
unsigned int examsg_stats_fill_levels[EXAMSG_STATS_MAX_FILL_LEVELS] =
  { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };


/* --- local functions ----------------------------------------------- */

static void writewrap(exa_ringbuf_t *rng, const char *buf, size_t nbytes);
static void readwrap(exa_ringbuf_t *rng, char *buf, size_t nbytes);
static void examsgRngStatsReset(exa_ringbuf_t *rng);


/* --- examsgRngInit ------------------------------------------------- */

void
examsgRngInit(exa_ringbuf_t *rng, size_t size)
{
  memset(rng, 0, size);
#ifdef HAVE_VALGRIND_MEMCHECK_H
  VALGRIND_MAKE_MEM_UNDEFINED(rng, size);
#endif
  rng->magic = EXAMSG_RNG_MAGIC;
  rng->pRd = 0;
  rng->pWr = 0;
  rng->size = size - sizeof(exa_ringbuf_t);

  examsgRngStatsReset(rng);
}

/* --- examsgRngStatsReset ------------------------------ */

/**
 * Reset stats on a ring buffer
 *
 * \param[in] rng  Ring buffer
 *
 */
static void
examsgRngStatsReset(exa_ringbuf_t *rng)
{
  int i;

  EXA_ASSERT(rng);

  for (i = 0; i < EXAMSG_STATS_MAX_FILL_LEVELS; i++)
    rng->stats.fill_level_count[i] = 0;

  rng->stats.full_count = 0;
  rng->stats.msg_count = 0;

  rng->stats.msg_max_size = 0;
  rng->stats.msg_min_size = 0;
  rng->stats.msg_avg_size = 0;
}

/* --- examsgRngBytes ------------------------------------------------ */
/** \brief Return number of bytes available for reading or writing in a
 * ring buffer.
 *
 * \param[in] rng	Ring buffer
 *
 * \return number of available or free bytes, or 0 on error.
 */
static size_t examsgRngBytes(const exa_ringbuf_t *rng)
{
  int s;
  /* sanity checks */
  if (!rng || rng->magic != EXAMSG_RNG_MAGIC)
    return 0;

  EXA_ASSERT(rng->pRd < rng->size);
  EXA_ASSERT(rng->pWr < rng->size);

  if (rng->pWr >= rng->pRd)
    s = rng->pWr - rng->pRd;
  else
    s = (rng->size - rng->pRd + rng->pWr);

  return s;
}

/* --- examsgRngStatsUpdate -------------------------- */

/**
 * Update statistics on a ring buffer
 *
 * This function calculates the current filling of a ringbuffer
 * and increments the counter corresponding to this filling
 * in order to be able to tell the use of the ringbuffer
 *
 * \param[in] rng        Ring buffer
 * \param[in] msg_size   Size of the message that is put in box
 *
 */
static void
examsgRngStatsUpdate(exa_ringbuf_t *rng, size_t msg_size)
{
  unsigned long prev_msg_count;
  unsigned int fill_level = examsgRngBytes(rng) * 100 / rng->size;
  int i;

  prev_msg_count = rng->stats.msg_count;
  rng->stats.msg_count++;

  /* did we wrap around counter ? */
  if (rng->stats.msg_count < prev_msg_count)
    {
      /*
       * TODO maybe a better implementation would be to reset all boxes and
       * not only this one. But this is not really nice anyway because stats
       * are then lost... so the present implementation is an acceptable
       * compromise. */
      examsgRngStatsReset(rng);
      /* re-increment because we are actually handling a message */
      rng->stats.msg_count++;
    }

  for (i = 0; i < EXAMSG_STATS_MAX_FILL_LEVELS; i++)
    if (fill_level <= examsg_stats_fill_levels[i])
      {
	rng->stats.fill_level_count[i]++;
	break;
      }

  if (rng->stats.msg_min_size > msg_size || rng->stats.msg_min_size == 0)
    rng->stats.msg_min_size = msg_size;
  if (rng->stats.msg_max_size < msg_size)
    rng->stats.msg_max_size = msg_size;

  /* if no average was calculated yet the average is the new message */
  if (rng->stats.msg_avg_size == 0)
    {
      rng->stats.msg_avg_size = msg_size;
    }
  else
    {
      uint64_t tmp;
      /* calculate the average message size. The calculation for message n+1
       * from the value for n is following :
       * AVG(n+1) =  ( n * AVG(n) + size(n) )/(n+1) */
      tmp = prev_msg_count * rng->stats.msg_avg_size + msg_size;
      tmp /= rng->stats.msg_count;
      rng->stats.msg_avg_size = tmp;
    }
}

/* --- examsgRngPutVaList -------------------------------------------------- */

/** \brief Write data in a ring buffer
 *
 * \param[in] rng   Ring buffer id
 * \param[in] ap    va_list of tuple (buffer, size) The list MUST end
 *                  with NULL and buffer MUST NOT be NULL; The parame size
 *                  MUST BE EXPLICITLY CAST as a size_t.
 *                  calling this function examsgRngPut(rng, buff, 10) IS A BUG
 *                  as 10 is cast in int and sizeof(int) != sizeof(size_t) on
 *                  64bit arch.
 *
 * \return number of bytes written, or a negative error code.
 */

int
examsgRngPutVaList(exa_ringbuf_t *rng, va_list ap)
{
  examsg_blktail t;
  struct examsg_blkhead h;
  size_t msgsize = 0, total, avail;
  char *buff;
  va_list ap2;

  /* sanity checks */
  if (!rng || rng->magic != EXAMSG_RNG_MAGIC)
    return -EINVAL;

  examsg_blktail_init(&t);

  /* preserve ap as it is used later in this function */
  va_copy(ap2, ap);

  /* compute size */
  while (va_arg(ap2, void *) != NULL)
    {
      size_t s = va_arg(ap2, size_t);
      msgsize += s;
    }

  /* total size of block, including our overhead */
  total = msgsize + sizeof(h) + sizeof(t);

  /* check free space */
  if (rng->pRd > rng->pWr)
    avail = rng->pRd - rng->pWr;
  else
    avail = rng->size - rng->pWr + rng->pRd;

  if (total >= avail)
    {
      rng->stats.full_count++;
      return -ENOSPC;
    }

  /* Statistics are updated only when putting a message in the ring buffer
   * (and not when removing one) because we're interested in "worst cases" */
  examsgRngStatsUpdate(rng, total);

  /* setup message header */
  h.size = msgsize;
  h.magic = EXAMSG_HEAD_MAGIC;

  writewrap(rng, (const char*)&h, sizeof(h));

  /* store buffers in ringbuf */
  while ((buff = va_arg(ap, char *)) != NULL)
    {
      size_t s = va_arg(ap, size_t);
      writewrap(rng, buff, s);
    }

  writewrap(rng, (const char*)&t, sizeof(t));

  va_end(ap2);

  return msgsize;
}


/* --- examsgRngGetVaList -------------------------------------------------- */

/** \brief Return next message in buffer
 *
 * \param[in] rng	ring buffer id.
 * \param[out] va_list	Tuples (buffer, size) in which data is copied (split)
 *                  The parame size MUST BE EXPLICITLY CAST as a size_t.
 *                  calling this function examsgRngGet(rng, buff, 10) IS A BUG
 *                  as 10 is cast in int and sizeof(int) != sizeof(size_t) on
 *                  64bit arch.
 *
 * If the buffer is too small for the current message, -EMSGSIZE is
 * returned, and the mailbox is left unchanged. But for debugging
 * purpose, the beginning of the faulty message (up to \a maxbytes bytes)
 * is copied into \a buf. Please do *not* rely on this behaviour.
 *
 * \return number of bytes read.
 */

int
examsgRngGetVaList(exa_ringbuf_t *rng, va_list ap)
{
  va_list ap2;
  struct examsg_blkhead h;
  examsg_blktail t;
  int r, avail;
  size_t maxbytes = 0;
  char *buff;

  /* sanity checks */
  EXA_ASSERT(rng);
  EXA_ASSERT(rng->magic == EXAMSG_RNG_MAGIC);

  if (rng->pWr == rng->pRd)
    return 0; /* no message */

  /* compute max read size */
  if (rng->pRd < rng->pWr)
    avail = rng->pWr - rng->pRd;
  else
    avail = rng->size - rng->pRd + rng->pWr;

  /* read header */
  r = rng->pRd;

  EXA_ASSERT(avail > sizeof(h));

  readwrap(rng, (char *)&h, sizeof(h));

  EXA_ASSERT(h.magic == EXAMSG_HEAD_MAGIC);
  EXA_ASSERT(h.size > 0);

  /* enough place? */
  EXA_ASSERT(avail >= h.size + sizeof(h) + sizeof(t));

  /* save the va list as it is resued later in the function */
  va_copy(ap2, ap);
  /* compute total size of provided buffers */
  while (va_arg(ap2, void *) != NULL)
    maxbytes += va_arg(ap2, size_t);

  /* Check buffer size if a buffer is provided */
  if (h.size > maxbytes)
    {
      /* restore read pointer (let buffer in ring) */
      rng->pRd = r;
      return -EMSGSIZE;
    }

  /* The number of bytes to available is put in maxbytes */
  maxbytes = h.size;

  /* compute total size of provided buffers */
  while ((buff = va_arg(ap, char *)) != NULL)
    {
      size_t s = va_arg(ap, size_t);
      readwrap(rng, buff, s < maxbytes ? s: maxbytes);

      if (maxbytes <= s)
	break;

      maxbytes -= s;
    }

  readwrap(rng, (char*)&t, sizeof(t));
  EXA_ASSERT(!examsg_blktail_check(&t));

  va_end(ap2);

  return h.size;
}

/**
 * see examsgRngPutVaList description for details
 */
int
__examsgRngPut(exa_ringbuf_t *rng, ...)
{
  int ret;
  va_list ap;
  va_start(ap, rng);
  ret = examsgRngPutVaList(rng, ap);
  va_end(ap);
  return ret;
}

/**
 * see examsgRngGetVaList description for details
 */
int
__examsgRngGet(exa_ringbuf_t *rng, ...)
{
  int ret;
  va_list ap;
  va_start(ap, rng);
  ret = examsgRngGetVaList(rng, ap);
  va_end(ap);
  return ret;
}


/* --- examsgRngMemSize ---------------------------------------------- */

/** Return the actual size needed to store a ring buffer.
 *
 * \param[in] num_msg   Maximum number of messages
 * \param[in] msg_size  Maximum message size
 *
 * \return the required storage size.
 */

size_t
examsgRngMemSize(size_t num_msg, size_t msg_size)
{
  /* Enough room to store the ring buffer metadata and the given number of
     messages, each with a head and tail */
  return (sizeof(exa_ringbuf_t)
	  + num_msg * (sizeof(struct examsg_blkhead) + msg_size
		       + sizeof(examsg_blktail))
	  /* FIXME: this extra byte is NEEDED because the whole pRd, pWr
	     thing is FUCKED and instead of fixing it with the SIMPLE
	     solution (pRd, size and pWr = f(pRd,size)), mister A. b0rked
	     the whole thing even more ! */
	  + 1);
}


/* --- writewrap ----------------------------------------------------- */

static void
writewrap(exa_ringbuf_t *rng, const char *buf, size_t nbytes)
{
  char *data = rng->data;
  int n;

  EXA_ASSERT(nbytes < rng->size);

  if (rng->pWr + nbytes >= rng->size)
    {
      /* wrap around */
      n = rng->size - rng->pWr;
      memcpy(data + rng->pWr, buf, n);
      nbytes -= n;
      buf += n;
      rng->pWr = 0;
    }

  /* direct write */
  memcpy(data + rng->pWr, buf, nbytes);
  rng->pWr += nbytes;

  EXA_ASSERT(rng->pRd < rng->size);
  EXA_ASSERT(rng->pWr < rng->size);
}


/* --- readwrap ----------------------------------------------------- */

/** \brief Read data from a ring buffer
 *
 * \param[in] rng: ring buffer id.
 * \param[in] buf: buffer where to store read data.
 * \param[in] nbytes: number of bytes to read.
 *
 * If the buffer is NULL, the data is dropped.
 */

static void
readwrap(exa_ringbuf_t *rng, char *buf, size_t nbytes)
{
  char *data = rng->data;
  int n, p;

  EXA_ASSERT(nbytes < rng->size);

#ifdef HAVE_VALGRIND_MEMCHECK_H
#  ifdef VALGRIND_MAKE_MEM_DEFINED /* Valgrind >= 3.2 */
      VALGRIND_MAKE_MEM_DEFINED(rng->data, rng->size);
#  else /* Valgrind < 3.2 */
      VALGRIND_MAKE_READABLE(rng->data, rng->size);
#  endif
#endif

  p = rng->pRd;

  if (p + nbytes >= rng->size)
    {
      /* wrap around */
      n = rng->size - p;
      if (buf)
	{
	  memcpy(buf, data + p, n);
	  buf += n;
	}
      nbytes -= n;
      p = 0;
    }

  /* direct read */
  if (buf)
    memcpy(buf, data + p, nbytes);
  rng->pRd = p + nbytes;

  EXA_ASSERT(rng->pRd < rng->size);
  EXA_ASSERT(rng->pWr < rng->size);
}

void examsgRngGetInfo(const exa_ringbuf_t *rng, exa_ringinfo_t *rng_info)
{
    EXA_ASSERT(rng_info);
    rng_info->stats     = rng->stats;
    rng_info->ring_size = rng->size;
    rng_info->available = examsgRngBytes(rng);
}

#ifdef DEBUG
/* --- examsgRngDump ------------------------------------------------- */

/** \brief Dump content of a ring buffer
 *
 * \param[in] rng	   Ring buffer
 * \param[in] dump_msg_fn  Function used to dump a message
 */
void
examsgRngDump(exa_ringbuf_t *rng, ExaRingDumpMsgFn dump_msg_fn)
{
  unsigned char *data = (unsigned char *)rng->data;
  int pRd, pWr;
  struct examsg_blkhead h;
  examsg_blktail t;
  int mc;

  /* dump general stats */
  exalog_trace("ring buffer %p, read %d, write %d, size %" PRIzu,
	       rng, rng->pRd, rng->pWr, rng->size);

  if (rng->pRd == rng->pWr)
    {
      exalog_trace("EMPTY");
      return;
    }

  /* save content of ring buffer */
  pRd = rng->pRd;
  pWr = rng->pWr;

  /* iterate over messages */
  mc = 0;
  while (rng->pRd != rng->pWr)
    {
      mc++;

      /* read header */
      readwrap(rng, (char *)&h, sizeof(h));

      if (dump_msg_fn)
	dump_msg_fn(mc, data + rng->pRd, h.size);

      rng->pRd = (rng->pRd + h.size) % rng->size;

      readwrap(rng, (char*)&t, sizeof(t));
    }

  /* restore buffer */
  rng->pRd = pRd;
  rng->pWr = pWr;
}

#endif /* DEBUG */
