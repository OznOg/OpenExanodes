/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "nbd/clientd/src/bd_user_user.h"
#include "nbd/clientd/src/nbd_clientd_private.h"
#include "nbd/clientd/src/nbd_stats.h"

#include "nbd/service/include/nbd_msg.h"

#include "nbd/common/nbd_common.h"

#include "common/include/daemon_api_server.h"
#include "common/include/exa_error.h"

#include "log/include/log.h"

#include "os/include/os_time.h"
#include "os/include/os_thread.h"
#include "os/include/strlcpy.h"

#include <errno.h>
#include <string.h>

#define NBD_REQ_TYPE_INVALID (NBD_REQ_TYPE_WRITE + 15)

static void nbd_stat_reset(struct device_stats *stats)
{
  memset(&stats->begin, 0, sizeof(stats->begin));
  memset(&stats->done, 0, sizeof(stats->done));

  /* Ensure a value different from READ, WRITE and WRITE_BARRIER so
   * that the first request is not counted as a seek. */
  stats->begin.prev_request_type = NBD_REQ_TYPE_INVALID;

  stats->last_reset = os_gettimeofday_msec();
}


void nbd_stat_init(struct device_stats *stats)
{
  os_thread_mutex_init(&stats->begin_mutex);
  os_thread_mutex_init(&stats->done_mutex);
}

void nbd_stat_clean(struct device_stats *stats)
{
    os_thread_mutex_destroy(&stats->begin_mutex);
    os_thread_mutex_destroy(&stats->done_mutex);
}

void nbd_stat_restart(struct device_stats *stats)
{
  memset(&stats->begin, 0, sizeof(stats->begin));
  memset(&stats->done, 0, sizeof(stats->done));

  /* Ensure a value different from READ, WRITE and WRITE_BARRIER so
   * that the first request is not counted as a seek. */
  stats->begin.prev_request_type = NBD_REQ_TYPE_INVALID;

  stats->last_reset = 0;
}


void nbd_get_stats(struct device_stats *stats, struct nbd_stats_reply *reply,
                   bool reset)
{
    os_thread_mutex_lock(&stats->begin_mutex);
    os_thread_mutex_lock(&stats->done_mutex);

    reply->last_reset = stats->last_reset;

    if (stats->last_reset == 0)
        reply->now = 0;
    else
        reply->now = os_gettimeofday_msec();

    reply->begin = stats->begin.info;
    reply->done = stats->done.info;

    if (reset)
        nbd_stat_reset(stats);

    os_thread_mutex_unlock(&stats->begin_mutex);
    os_thread_mutex_unlock(&stats->done_mutex);
}

static uint64_t distance(uint64_t offset1, uint64_t offset2)
{
  return offset1 > offset2 ? offset1 - offset2 : offset2 - offset1;
}


/*
 * This function is called when the NBD client receives a request from
 * the kernel, just before submitting it to the appropriate NBD
 * server.
 */
void nbd_stat_request_begin(struct device_stats *stats, const nbd_io_desc_t *io)
{
  bool is_seq;

  os_thread_mutex_lock(&stats->begin_mutex);

  is_seq = (io->sector == stats->begin.next_sector &&
     io->request_type == stats->begin.prev_request_type)
    || stats->begin.prev_request_type == NBD_REQ_TYPE_INVALID;

  EXA_ASSERT(NBD_REQ_TYPE_IS_VALID(io->request_type));
  switch (io->request_type)
    {
    case NBD_REQ_TYPE_READ:
      stats->begin.info.nb_sect_read += io->sector_nb;
      ++stats->begin.info.nb_req_read;

      if (!is_seq)
	{
	  stats->begin.info.nb_seek_dist_read +=
	    distance(io->sector, stats->begin.next_sector);
	  ++stats->begin.info.nb_seeks_read;
	}

      break;

    case NBD_REQ_TYPE_WRITE:
      stats->begin.info.nb_sect_write += io->sector_nb;
      ++stats->begin.info.nb_req_write;

      if (!is_seq)
	{
	  stats->begin.info.nb_seek_dist_write +=
	    distance(io->sector, stats->begin.next_sector);
	  ++stats->begin.info.nb_seeks_write;
	}
      break;
    }

  stats->begin.prev_request_type = io->request_type;
  stats->begin.next_sector = io->sector + io->sector_nb;

  os_thread_mutex_unlock(&stats->begin_mutex);
}


/*
 * This function is called when we receive a response from the NBD
 * server to a request the NBD client issued, just before handing it
 * back to the kernel.
 */
void nbd_stat_request_done(struct device_stats *stats, const nbd_io_desc_t *io)
{
  os_thread_mutex_lock(&stats->done_mutex);

  if (io->result >= 0)
  {
      EXA_ASSERT(NBD_REQ_TYPE_IS_VALID(io->request_type));
      switch (io->request_type)
      {
      case NBD_REQ_TYPE_READ:
          stats->done.info.nb_sect_read += io->sector_nb;
          ++stats->done.info.nb_req_read;
          break;
      case NBD_REQ_TYPE_WRITE:
          stats->done.info.nb_sect_write += io->sector_nb;
          ++stats->done.info.nb_req_write;
          break;
      }
  }
  else
  {
    /* The EAGAIN requests will be replayed. */
    if (io->result != -EAGAIN)
      ++stats->done.info.nb_req_err;
  }

  os_thread_mutex_unlock(&stats->done_mutex);
}

