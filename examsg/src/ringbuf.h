/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_EXAMSG_RINGBUF
#define H_EXAMSG_RINGBUF

#include "common/include/exa_constants.h"
#include "os/include/os_inttypes.h"

#include <string.h>

#include <stdarg.h>

/* --- Ring buffer object -------------------------------------------- */

/** Ring buffer handle */
typedef struct exa_ringbuf exa_ringbuf_t;

/** Number of fill levels of interest */
#define EXAMSG_STATS_MAX_FILL_LEVELS  11

/** Fill level values */
extern unsigned int examsg_stats_fill_levels[EXAMSG_STATS_MAX_FILL_LEVELS];

/** Ring buffer statistics */
typedef struct exa_ringbufStat
  {
    /** Per-fill level event counts: entry [i] == c means that c times
     * the ringbuffer fill level was in the range
     * ] examsg_stats_fill_levels[i-1] ... examsg_stats_fill_levels[i] ] */
    unsigned long fill_level_count[EXAMSG_STATS_MAX_FILL_LEVELS];

    /** Number of times a message couldn't be put in the ring buffer
     * because the buffer was full */
    unsigned long full_count;

    unsigned long msg_count; /**< total number of messages */
    size_t msg_min_size; /**< minimum size of messages */
    size_t msg_max_size; /**< maximum size of messages */
    size_t msg_avg_size; /**< average size of messages */

  } ExaRingBufStat;

typedef struct exa_ringinfo {
    size_t ring_size;  /* Size of ring buffer */
    size_t available;  /* Amount of bytes available for read */
    ExaRingBufStat stats; /* Stats for this ring buffer */
} exa_ringinfo_t;

/** Ring buffer header */
struct exa_ringbuf {
#define  EXAMSG_RNG_MAGIC       0x32145678	/* magic flag */
  int magic;		/* magic flag (means: initialized) */
  int pRd;		/* read position */
  int pWr;		/* write position */
  size_t size;		/* size of buffer */
  ExaRingBufStat stats; /**< Statistics */
  char data[];          /**< Data */
};

/** Start-of-message marker */
struct examsg_blkhead {
#define  EXAMSG_HEAD_MAGIC       0x11EADF00/* magic flag */
  uint32_t magic;       /* magic number for check */
  size_t size;		/* message size */
};

/** End-of-message marker, contains EXAMSG_TAIL_PATTERN */
typedef struct
{
#ifdef DEBUG
#  define EXAMSG_TAIL_SIZE     20
#else
#  define EXAMSG_TAIL_SIZE     1
#endif

#define EXAMSG_TAIL_PATTERN  '$'
  char tail[EXAMSG_TAIL_SIZE];
} examsg_blktail;

static inline int examsg_blktail_check(examsg_blktail * tail)
{
  int i;

  for (i = 0; i < EXAMSG_TAIL_SIZE; i++)
    {
      if (tail->tail[i] != EXAMSG_TAIL_PATTERN)
        return -1;
    }

  return 0;
}

static inline void examsg_blktail_init(examsg_blktail * tail)
{
  memset(tail->tail, EXAMSG_TAIL_PATTERN, sizeof(tail->tail));
}

/** Flags for ringbuffers spying */
typedef enum ExaRingOp {
  EXARNG_AVAIL,
  EXARNG_FREE
} ExaRingOp;

void examsgRngInit(exa_ringbuf_t *rng, size_t size);

int examsgRngPutVaList(exa_ringbuf_t *rng, va_list ap);
int examsgRngGetVaList(exa_ringbuf_t *rng, va_list ap);

int __examsgRngPut(exa_ringbuf_t *rng, ...);
int __examsgRngGet(exa_ringbuf_t *rng, ...);

/* This macro is here to prevent misuse of the API and add the final NULL in
 * the list of params. There are 2 NULL to prevent as the function itself waits
 * for tuple (buffer size) and thus if the caller gives an odd number of param
 * (which is wrong), the function won't crash and return -EINVAL. */
#define examsgRngPut(rng, ...) __examsgRngPut((rng), __VA_ARGS__, NULL, 0)
#define examsgRngGet(rng, ...) __examsgRngGet((rng), __VA_ARGS__, NULL, 0)

size_t examsgRngMemSize(size_t num_msg, size_t msg_size);

/** Function used to display a message within a ring buffer */
typedef void (*ExaRingDumpMsgFn)(int idx, const void *data, size_t size);

/*
 * Returns information about a ringbuffer
 * \param[in] rng  the ring buffer
 * \param[in:out] rng_info informations about the ring buffer
 */
void examsgRngGetInfo(const exa_ringbuf_t *rng, exa_ringinfo_t *rng_info);

#ifdef DEBUG
void examsgRngDump(exa_ringbuf_t *rng, ExaRingDumpMsgFn dump_msg_fn);
#endif

#endif /* H_EXAMSG_RINGBUF */
