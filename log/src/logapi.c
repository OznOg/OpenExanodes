/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file
 * Logging API.
 *
 * The logging uses an Examsg shared data structure to store the
 * loglevels of each component and the list of threads and modules
 * registered for logging.
 *
 * The shared data structure allows logging clients to filter out
 * messages that have a loglevel with a lower priority than the
 * currently configured loglevel.
 *
 * The handling of modules is kinda ugly: the module's pointer
 * THIS_MODULE is explicitely cast to a thread id (pid_t).
 */

#include "os/include/os_shm.h"
#include "os/include/os_time.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"

#include "examsg/src/objpoolapi.h"
#include "logd.h"
#include "logd_com.h"

#include "log/include/log.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/** Component id of the current thread
 * FIXME use reall components id not examsg..*/
static __thread ExamsgID my_cid = EXAMSG_LOGD_ID;
/* FIXME is that really the best id by default ? */

static __thread unsigned lost = 0;  /**< number of log messages lost */

static os_shm_t *loglevels_shm = NULL;

void exalog_static_init(void)
{
    /* MUST BE DONE EXACTLY ONCE */
    EXA_ASSERT(!loglevels_shm);
    loglevels_shm = os_shm_get(EXALOG_SHM_ID, EXALOG_SHM_SIZE);
}

void exalog_static_clean(void)
{
   EXA_ASSERT(loglevels_shm);
   os_shm_release(loglevels_shm);
   loglevels_shm = NULL;
}

/**
 * Register a thread/module for logging.
 *
 * \param[in] cid   Component cid
 *
 * In userspace, the parameters are ignored as it applies to the current
 * thread and the needed values are found out in kernelspace by the module.
 *
 */
void
exalog_as(ExamsgID cid)
{
    EXA_ASSERT(EXAMSG_ID_VALID(cid));

    my_cid = cid;
}

void exalog_end(void)
{
    my_cid = EXAMSG_LOGD_ID;
    lost = 0;
}

/**
 * Log a message.
 *
 * \param[in] level  Log level
 * \param[in] file   File name
 * \param[in] func   Function name
 * \param[in] line   Line number
 * \param[in] fmt    Message format, printf syntax
 * \param[in] al     Argument list
 */
static void
log_msg(exalog_level_t level, const char *file,
	const char *func, uint32_t line, const char *fmt, va_list al)
{
  exalog_data_t data;
  const exalog_level_t *loglevels;
  exalog_msg_t *msg = &data.d.log_msg;
  const char *pfile;
  size_t size;
  int r;

  /* Save errno */
  int saved_errno = errno;

  /* Check the user correctly called exalog_client_init before;
   * in case the shared memory cannot be accessed, the message is dropped */
  if (!loglevels_shm)
      return;

  loglevels = os_shm_get_data(loglevels_shm);

  EXA_ASSERT(loglevels);

  if (level > loglevels[my_cid])
    return;

  /*
   * Build message
   */
  memset(&data, 0, sizeof(data));

  data.type = LOG_MSG;

  if ((pfile = strrchr(file, '/')))
    strlcpy(msg->file, pfile + 1, sizeof(msg->file));
  else
    strlcpy(msg->file, file, sizeof(msg->file));

  strlcpy(msg->func, func, sizeof(msg->func));
  msg->line = line;

  msg->level = level;
  msg->cid = my_cid;
  msg->lost = lost;

  size = os_vsnprintf(msg->msg, sizeof(msg->msg), fmt, al) + 1;

  os_gettimeofday(&msg->rclock);

  /* Compute size */
  if (size > sizeof(msg->msg))
    size = sizeof(data);
  else
    size += sizeof(data) - sizeof(msg->msg);

  r = logd_com_send(&data, size);
  if (r)
    lost++;
  else
   lost = 0;

  /* Restore errno */
  errno = saved_errno;
}

/**
 * Log a message in userspace.
 * No need for a thread id: the thread info is accessible through TLS.
 * See __exalog_info_text() for a description of the parameters.
 */
void
exalog_text(exalog_level_t level, const char *file, const char *func,
	      uint32_t line, const char *fmt, ...)
{
  va_list al;

  va_start(al, fmt);
  log_msg(level, file, func, line, fmt, al);
  va_end(al);
}

/* --- exalog_configure ---------------------------------------------- */
/** \brief Configure logging daemon
 *
 * \param[in] component	Component ID or EXAMSG_MAX_ID for all components.
 * \param[in] level	Level of logging for the component
 *
 * \return 0 on success or a negative error code.
 */
int exalog_configure(ExamsgID component, exalog_level_t level)
{
  exalog_data_t c;

  c.type = LOG_CONFIG;

  c.d.log_config.component = component;
  c.d.log_config.level = level;

  return logd_com_send(&c, sizeof(c));
}

/**
 * Set the hostname to use in log messages.
 *
 * \param[in] hostname Local host's name
 *
 * \return 0 on success, a negative error code otherwise
 */
int exalog_set_hostname(const char *hostname)
{
  exalog_data_t h;

  if (hostname == NULL)
    return -EINVAL;

  h.type = LOG_HOST;

  strlcpy(h.d.hostname, hostname, sizeof(h.d.hostname));

  return logd_com_send(&h, sizeof(h));
}

int exalog_quit(void)
{
  exalog_data_t q;

  q.type = LOG_QUIT;

  return logd_com_send(&q, sizeof(q));
}


