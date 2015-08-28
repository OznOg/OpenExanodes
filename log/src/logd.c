/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file
 * Logging daemon.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>

#include "os/include/os_file.h"
#include "os/include/os_shm.h"
#include "os/include/strlcpy.h"
#include "os/include/os_network.h"
#include "os/include/os_dir.h"
#include "os/include/os_time.h"
#include "os/include/os_syslog.h"
#include "os/include/os_error.h"
#include "os/include/os_stdio.h"

#include "common/include/exa_constants.h"
#include "common/include/threadonize.h"
#include "common/include/exa_error.h"

#include "log/include/log.h"

#include "examsg/src/mailbox.h"


#include "logd.h"
#include "logd_com.h"

/** Log file name */
static char exalog_file[OS_PATH_MAX];
static char exalog_old_file[OS_PATH_MAX];

static char local_node[EXA_MAXSIZE_HOSTNAME + 1];  /**< Local hostname */
static FILE *logfile;                              /**< Log file */
static os_shm_t *p_loglevels_shm = NULL; /* Private pointer on shm */

static bool need_rotate;  /**< Need to rotate the log file ? */
static bool need_reopen;  /**< Need to reopen the log file ? */

static os_thread_t thr_log;

/** Log level names */
static const char *loglevel_tab[] =
  {
    [EXALOG_LEVEL_NONE]    = "",
    [EXALOG_LEVEL_ERROR]   = "[ERROR]",
    [EXALOG_LEVEL_WARNING] = "[WARNING]",
    [EXALOG_LEVEL_INFO]    = "[Info]",
    [EXALOG_LEVEL_TRACE]   = "[trace]",
    [EXALOG_LEVEL_DEBUG]   = "[debug]"
  };

/**
 * SIGHUP handler.
 */
static void
log_reopen_handler(int unused)
{
  need_reopen = true;
}

/**
 * SIGXFSZ handler, in case the file size limit is reached.
 */
static void
log_rotate_handler(int unused)
{
  need_rotate = true;
}

/**
 * Set the hostname to use in logged messages.
 *
 * \param[in] hostname  Local host's name
 */
static void
log_set_hostname(const char *hostname)
{
  strlcpy(local_node, hostname, sizeof(local_node));
}


#ifdef WIN32
/**
 * Match Exalog levels to os_syslog levels.
 *
 * @param[in] level     the exalog_level_t to match
 *
 * @return the corresponding os_syslog_level_t
 */
static os_syslog_level_t os_syslog_level_from_exalog_level(exalog_level_t level)
{
    EXA_ASSERT(level != EXALOG_LEVEL_NONE);

    switch(level)
    {
    case EXALOG_LEVEL_ERROR:
        return OS_SYSLOG_ERROR;

    case EXALOG_LEVEL_WARNING:
        return OS_SYSLOG_WARNING;

    case EXALOG_LEVEL_INFO:
        return OS_SYSLOG_INFO;

    case EXALOG_LEVEL_DEBUG:
    case EXALOG_LEVEL_TRACE:
    case EXALOG_LEVEL_NONE:
        return OS_SYSLOG_DEBUG;
    }

    return OS_SYSLOG_DEBUG;
}
#endif

/**
 * Initialize logging facility.
 *
 * \return 0 on success, a negative error code otherwise
 */
int log_init(const char *logfile_name)
{
    int ret;
    ExamsgID id;
    exalog_level_t *ll;
    char name[EXA_MAXSIZE_HOSTNAME + 1] = "localhost";

    /* make sure init is done only once */
    EXA_ASSERT(!p_loglevels_shm);

    /* Create shared data one log level per component
     * CAREFUL Map shm before doing anything else as all other function may call
     * log subsystem
     * FIXME here we use examsgIDas component, thus the size is really much
     * bigger than needed */
    p_loglevels_shm = os_shm_create(EXALOG_SHM_ID, EXALOG_SHM_SIZE);
    if (!p_loglevels_shm)
	return -ENOMEM;

    ll = os_shm_get_data(p_loglevels_shm);
    /* Reset log levels to default when starting */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
	ll[id] = EXALOG_DEFAULT_LEVEL;

    os_host_canonical_name("localhost", name, sizeof(name));

    /* Initialization is done before spawning the thread so that the mailbox
     * is available when returning from this function and thus log messages
     * are stored in the mailbox waiting for the thread to handle them. */
    log_set_hostname(name);

    /* Create log file */
    logfile = fopen(logfile_name, "a");
    if (!logfile)
    {
	ret = -errno;
	os_syslog(OS_SYSLOG_ERROR, "cannot create logfile: %s", strerror(ret));
	return ret;
    }

#ifndef WIN32
    /* FIXME WIN32 I do not know how to implement this on windows...
     * What make problem here in not the signal function itself which exists
     * but the signal itself (SIGHUP etc..) which is not defined on windows.
     * Actually, I think we could add the definition and keep the code like
     * this, but I really do not know if this is the best was to do it (how
     * can we later send a SIGHUP to the process ?)
     * For now I comment this, considering this is not prioriary code...
     */
    signal(SIGHUP, log_reopen_handler);
    signal(SIGXFSZ, log_rotate_handler);
#endif

    return logd_com_init();
}

/**
 * Shutdown logging facility.
 */
static void log_exit(void)
{
    logd_com_exit();

    os_shm_delete(p_loglevels_shm);

    if (logfile)
	fclose(logfile);
}

int exalog_thread_start(void)
{
    int err, n;
    const char *log_dir = getenv("EXANODES_LOG_DIR");

    if (log_dir == NULL)
    {
	os_syslog(OS_SYSLOG_ERROR,
                  "Cannot get environment variable EXANODES_LOG_DIR.");
	return -EXA_ERR_BAD_INSTALL;
    }

    n = os_snprintf(exalog_file, sizeof(exalog_file), "%s" OS_FILE_SEP "%s",
                    log_dir, "exanodes.log");
    if (n < 0)
        return n;

    n = os_snprintf(exalog_old_file, sizeof(exalog_old_file), "%s" OS_FILE_SEP "%s",
                    log_dir, "exanodes.log.old");
    if (n < 0)
        return n;

    /* Create log directory */
    err = os_dir_create_recursive(log_dir);
    if (err != 0)
    {
	os_syslog(OS_SYSLOG_ERROR,
                  "Cannot create log directory '%s': %s (%d)",
		  log_dir, os_strerror(err), err);
        return err;
    }

    err = log_init(exalog_file);
    if (err != 0)
	return err;

    /* Initialize exalog (with a reasonable stack; Linux default is 8 MB) */
    exathread_create_named(&thr_log,
	    ADMIND_THREAD_STACK_SIZE + MIN_THREAD_STACK_SIZE,
	    exalog_loop, NULL, "exa_adm_log");

    return 0;
}

void exalog_thread_stop()
{
    exalog_quit();
    os_thread_join(thr_log);
    log_exit();
}

/**
 * Reopen a log file in a new file and close the old one.
 *
 * In case it's not possible to open a new file, the old one is kept.
 *
 * If fclose() on the old one fails (because the file is bigger than the
 * fs limit, for example), the close is forced by calling close().
 *
 * When successfully reopening a log file, logging is resumed.
 *
 * \return 0 on success, a negative error otherwise
 */
static int
log_reopen(void)
{
  FILE *old = logfile;
  FILE *new;

  /* create log file */
  new = fopen(exalog_file, "w");
  if (!new)
    {
      int ret = errno;
      os_syslog(OS_SYSLOG_ERROR, "cannot create log %s\n", strerror(ret));
      return -ret;
    }

  logfile = new;

  /* close old one */
  if (old)
  {
      if (fclose(old))
	  os_syslog(OS_SYSLOG_WARNING, "failed to close old log file %s (%d),",
		    strerror(errno), errno);
  }

  return 0;
}

/**
 * Perform a log rotate (exanodes.log to exanodes.log.old).
 *
 * If the rotation is sucessfully performed, log_reopen() is called.
 * Otherwise, a negative error code is returned.
 */
static int
log_rotate(void)
{
  int retval;

  retval = os_file_rename(exalog_file, exalog_old_file);
  if (retval)
    {
      os_syslog(OS_SYSLOG_ERROR, "cannot rename log %s\n", strerror(-retval));
      /* If there was an error... well, try to reopen the log file anyway
       * because an error doesn't mean that we do not want to rotate anymore */
    }

  return log_reopen();
}

/**
 * Configure the logging level of specified component.
 *
 * \param[in] c	 Configuration message
 */
static void
log_configure(const exalog_config_t *cfg)
{
  exalog_level_t *loglevels = os_shm_get_data(p_loglevels_shm);
  int c, cmin, cmax;

  EXA_ASSERT(EXAMSG_ID_VALID(cfg->component)
             || cfg->component == EXAMSG_ALL_COMPONENTS);

  if (cfg->component == EXAMSG_ALL_COMPONENTS)
    {
      cmin = EXAMSG_FIRST_ID;
      cmax = EXAMSG_LAST_ID;
    }
  else
    cmin = cmax = cfg->component;

  for (c = cmin; c <= cmax; c++)
      loglevels[c] = cfg->level;
}

/**
 * Append a message to the log.
 *
 * \param[in] msg  Log message
 *
 * \return 0 on success, a negative error code otherwise
 */
static int
log_append(const exalog_msg_t *msg)
{
  int ret;
  struct tm date;
  const char *date_str;
  int msec;
  unsigned lost;
  time_t date_in_sec;
#ifdef WIN32
  os_syslog_level_t os_level;
#endif

  /* Check flags raised by signals */
  if (need_rotate)
    {
      need_rotate = false;
      if ((ret = log_rotate()) != 0)
        return ret;
    }

  if (need_reopen)
    {
      need_reopen = false;
      if ((ret = log_reopen()) != 0)
        return ret;
    }

  EXA_ASSERT(EXAMSG_ID_VALID(msg->cid));

  /* NONE level is not sent, thus cannot be received... */
  EXA_ASSERT(msg->level != EXALOG_LEVEL_NONE);

  if (!logfile)
    return -EINVAL;

  date_in_sec = (time_t)msg->rclock.tv_sec;
  os_localtime(&date_in_sec, &date);
  msec = msg->rclock.tv_usec / 1000;

  date_str = os_date_msec_to_str(&date, msec);

#ifdef WIN32
  os_level = os_syslog_level_from_exalog_level(msg->level);
#endif

  lost = msg->lost;
  if (lost > 0)
    {
#ifdef DEBUG
      ret = fprintf(logfile, "%s %s %s %s:%d %s() %s %u log messages lost\n",
		    date_str,
		    local_node,
		    examsgIdToName(msg->cid),
		    msg->file, msg->line,
		    msg->func,
		    loglevel_tab[EXALOG_LEVEL_WARNING],
		    lost);
#ifdef WIN32
      os_syslog(OS_SYSLOG_WARNING, "%s %s %s:%d %s() %u log messages lost\n",
		    local_node,
		    examsgIdToName(msg->cid),
		    msg->file, msg->line,
		    msg->func,
		    lost);
#endif
#else
      ret = fprintf(logfile, "%s %s %u log messages lost\n",
		    date_str,
		    loglevel_tab[EXALOG_LEVEL_WARNING],
		    lost);
#ifdef WIN32
      os_syslog(OS_SYSLOG_WARNING, "%u log messages lost\n",
		    lost);
#endif
#endif
      if (ret<0)
	return ret;

      if (fflush(logfile))
	return -errno;
    }

#ifdef DEBUG
  ret = fprintf(logfile, "%s %s %s %s:%d %s() %s %s\n",
		date_str,
		local_node,
		examsgIdToName(msg->cid),
		msg->file, msg->line,
		msg->func,
		loglevel_tab[msg->level],
		msg->msg);
#ifdef WIN32
  if (os_level < OS_SYSLOG_DEBUG)
      os_syslog(os_level, "%s %s %s:%d %s() %s\n",
		local_node,
		examsgIdToName(msg->cid),
		msg->file, msg->line,
		msg->func,
		msg->msg);
#endif
#else
  if (EXALOG_LEVEL_VISIBLE_BY_USER(msg->level))
  {
    ret = fprintf(logfile, "%s %s %s\n",
		  date_str,
		  loglevel_tab[msg->level],
		  msg->msg);
#ifdef WIN32
    if (os_level < OS_SYSLOG_DEBUG)
        os_syslog(os_level, "%s\n",
		  msg->msg);
#endif
  }
  else
  {
    ret = fprintf(logfile, "%s %s <%s:%d>  %s\n",
		  date_str,
		  loglevel_tab[msg->level],
		  msg->file, msg->line,
		  msg->msg);
#ifdef WIN32
    if (os_level < OS_SYSLOG_DEBUG)
        os_syslog(os_level, "<%s:%d>  %s\n",
		  msg->file, msg->line,
		  msg->msg);
#endif
  }
#endif
  if (ret < 0)
    return ret;

  if (fflush(logfile))
    return -errno;

  return 0;
}

/**
 * Suspend logging.
 *
 * \return 0 on success, a negative error code otherwise
 */
static int
log_suspend(void)
{
  /* wait for resume */
  /* while we wait, we drop any log message that would be in the mailbox.
   * This behaviour is to prevent the logd to be waked up permanently by
   * messages that would be left in the box, but that we don't process
   * to sum up, WHEN SUSPENDED, EVERY LOG MESSAGES ARE DROPPED */
  while (true)
    {
      exalog_data_t logmsg;

      if (need_rotate || need_reopen)
	break;

      logd_com_recv(&logmsg);
    }

  return 0;
}

/**
 * Logging thread
 *
 * \param[in] arg not used
 *
 * \return NULL
 */
void exalog_loop(void *dummy)
{
  bool quit = false;

  do {
      exalog_data_t logmsg;
      int err = logd_com_recv(&logmsg);

      if (err)
      {
	  log_suspend();
	  continue;
      }

      switch (logmsg.type)
      {
	  case LOG_HOST:
	      log_set_hostname(logmsg.d.hostname);
	      break;

	  case LOG_CONFIG:
	      log_configure(&logmsg.d.log_config);
	      break;

	  case LOG_MSG:
	      err = log_append(&logmsg.d.log_msg);
	      if (err)
	      {
		  os_syslog(OS_SYSLOG_DEBUG,
			    "failed to log message, error %d\n", err);
		  if (err == -EFBIG)
		      err = log_rotate();

		  if (err)
		      log_suspend();
	      }
	      break;

	  case LOG_QUIT:
	      quit = true;
	      break;
      }
    } while (!quit);
}
