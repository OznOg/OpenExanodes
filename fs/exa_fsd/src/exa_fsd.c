/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "common/include/exa_env.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"

#include "log/include/log.h"
#include "examsg/include/examsg.h"

#include "common/include/daemon_api_server.h"
#include "os/include/strlcpy.h"
#include "os/include/os_daemon_child.h"
#include "os/include/os_file.h" /* for OS_PATH_MAX */
#include "os/include/os_syslog.h"
#include "os/include/os_thread.h"
#include "os/include/os_daemon_lock.h"

#include "fs/include/exactrl-fs.h"
#include "fs/exa_fsd/src/exa_fsd.h"
#include "fs/exa_fsd/src/fscommands.h"
#include "fs/exa_fsd/src/work_thread.h"

ExamsgHandle mh;

char hostname[EXA_MAXSIZE_HOSTNAME+1];

/** PID of Admind */
static uint32_t admind_pid = 0;

/** Whether the daemon should quit */
static bool stop;

/* request queue */
struct daemon_request_queue *requests[EXA_FS_REQUEST_LAST];

os_daemon_lock_t *exa_fsd_daemon_lock;

/* Signal handler */
static void
signal_handler(int sig)
{
  stop = true;
}

static const char *program;    /**< Daemon name */

/* XXX Should be moved to some sort of plugin. */
static os_thread_mutex_t gfs_lock;
static uint32_t gfs_gulm_pid = 0;

/**
 * Set whether Gulm is running.
 *
 * @param[in] running  Whether Gulm is running
 *
 * This function must be called with 'true' after Gulm has been spawned and
 * with 'false' before Gulm is terminated.
 */
void gfs_set_gulm_running(bool running)
{
    char path[OS_PATH_MAX];
    FILE *file;
    uint32_t pid = 0;

    if (running)
    {
	exa_env_make_path(path, sizeof(path), exa_env_piddir(), "%s.pid",
                          exa_daemon_name(EXA_DAEMON_LOCK_GULMD));

	file = fopen(path, "r");
	if (!file)
	    pid = 0;

	if (fscanf(file, "%"PRIu32, &pid) != 1)
	    pid = 0;

	fclose(file);
	/* FIXME Handle pid file read error */
    }

    os_thread_mutex_lock(&gfs_lock);

    gfs_gulm_pid = pid;

    os_thread_mutex_unlock(&gfs_lock);
}

/**
 * Get Gulm's PID.
 *
 * @return Gulm's PID if Gulm has been spawned, 0 otherwise.
 */
static uint32_t gfs_get_gulm_pid(void)
{
    uint32_t pid;

    os_thread_mutex_lock(&gfs_lock);
    pid = gfs_gulm_pid;
    os_thread_mutex_unlock(&gfs_lock);

    return pid;
}

/* --- fsd_get_queue ---------------------------------------------- */
/**
 * Get the right queue from the message
 *
 * \return void
 */
static struct daemon_request_queue *
fsd_get_queue(ExamsgID from)
{
  switch (from)
    {
    case EXAMSG_ADMIND_INFO_LOCAL:
      return requests[EXA_FS_REQUEST_INFO];

    case EXAMSG_ADMIND_CMD_LOCAL:
      return requests[EXA_FS_REQUEST_CMD];

    case EXAMSG_ADMIND_RECOVERY_LOCAL:
      return requests[EXA_FS_REQUEST_RECOVERY];

    case EXAMSG_TEST_ID:
      return requests[EXA_FS_REQUEST_TEST];

    default:
      EXA_ASSERT(false);
    }
}

/**
 * Check whether a daemon is alive. Exit if the daemon is dead.
 *
 * @param[in] daemon_id  Id of daemon to check
 * @param[in] pid        PID of the daemon
 */
static void __check_daemon(exa_daemon_id_t daemon_id, uint32_t pid)
{
    if (kill(pid, 0) != 0)
    {
        const char *name = exa_daemon_name(daemon_id);
        os_syslog(OS_SYSLOG_ERROR, "daemon %s (pid %"PRIu32") is dead => EXITING",
                  name, pid);
        exit(1);
    }
}

static void check_admind(void)
{
    __check_daemon(EXA_DAEMON_ADMIND, admind_pid);
}

/* XXX Should be moved to some sort of plugin. */
static void check_gfs(void)
{
    uint32_t pid = gfs_get_gulm_pid();

    if (pid != 0)
        __check_daemon(EXA_DAEMON_LOCK_GULMD, pid);
}

/* --- fsd_process_loop ------------------------------------------- */
/* The event loop of the exa_fsd daemon.
 *
 * \param[in] req	Examsg of the request.
 * \return 0 on success or an error code.
 */
static int
fsd_process_loop(void)
{
  struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
  Examsg msg;
  ExamsgMID from;
  int retval;
  struct daemon_request_queue * queue;

  check_admind();
  check_gfs();

  /* wait for a message or a signal */
  retval = examsgWaitInterruptible(mh, &timeout);
  if (retval == -EINTR || retval == -ETIME)
      return 0;

  if (retval != 0)
    return retval;

  memset(&msg, 0, sizeof(msg));
  memset(&from, 0, sizeof(from));

  retval = examsgRecv(mh, &from, &msg, sizeof(msg));

  /* Error while message reception */
  if (retval < 0)
    return retval;

  /* No message */
  if (retval == 0)
    return 0;

  queue = fsd_get_queue(from.id);

  if (msg.any.type == EXAMSG_DAEMON_RQST)
    {
      EXA_ASSERT(retval > sizeof(msg.any));
      daemon_request_queue_add_request(queue, msg.payload,
	      retval - sizeof(msg.any), from.id);
    }
  else if (msg.any.type == EXAMSG_DAEMON_INTERRUPT)
    {
      daemon_request_queue_add_interrupt(queue, mh, from.id);
    }
  else
    {
      EXA_ASSERT_VERBOSE(false, "Cannot handle this type of message: %d",
			 msg.any.type);
    }
  return 0;
}

/**
 * Display usage help and exit.
 *
 * \param[in] status  Exit status
 */
static void
usage(int status)
{
  fprintf(stderr, "Exanodes File System Daemon\n");
  fprintf(stderr, "Usage: %s [options]\n", program);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -h, --hostname <c>       Set hostname to run as.\n");
  fprintf(stderr, "  -a, --admind <pid>       The exa_admin process id.\n");
  exit(status);
}

int daemon_init(int argc, char *argv[])
{
    static struct option long_opts[] =
	{
	    { "hostname", required_argument, NULL, 'h' },
	    { "admind",   required_argument, NULL, 'a' },
	    { NULL,       0,                 NULL, 0   }
	};

    int long_idx;
    int ret;

    program = argv[0];

    while (true)
    {
	int c = getopt_long(argc, argv, "h:a:", long_opts, &long_idx);

	if (c == -1)
	    break;

	switch (c)
	{
	case 'h':
	    strlcpy(hostname, optarg, EXA_MAXSIZE_HOSTNAME+1);
	    break;

	case 'a':
	  if (sscanf(optarg, "%"PRIu32, &admind_pid) != 1 || admind_pid <= 1)
	      return -EINVAL;  /* FIXME Use better value */
	  break;

	default:
	    usage(1);
	}
    }

    ret = examsg_static_init(EXAMSG_STATIC_GET);
    if (ret != EXA_SUCCESS)
    	return ret;

  /* MUST be call BEFORE spawning threads */
  exalog_static_init();

  /* Set the logs here after the daemonize */
  exalog_as(EXAMSG_FSD_ID);

  /* Init examsg */
  mh = examsgInit (EXAMSG_FSD_ID);
  if (!mh)
      return -ENOMEM;

  ret = examsgAddMbox(mh, EXAMSG_FSD_ID, 3, EXAMSG_MSG_MAX);
  EXA_ASSERT_VERBOSE(ret==0, "ret=%d", ret);

  os_thread_mutex_init(&gfs_lock);

  /* Initialize working threads */
  fsd_workthread_init();

  /* register the signal handler in order to be able to stop properly */
  signal(SIGTERM, signal_handler);

  return EXA_SUCCESS;
}

int daemon_main(void)
{
    int retval;

    os_openlog(exa_daemon_name(EXA_DAEMON_FSD));

    if (os_daemon_lock(exa_daemon_name(EXA_DAEMON_FSD), &exa_fsd_daemon_lock) != 0)
    {
        os_syslog(OS_SYSLOG_ERROR, "Failed creating fsd lock");
        return EXA_ERR_LOCK_CREATE;
    }

    stop = false;
    /* process events until SIGTERM is received (see signal handler) */
    while (!stop)
    {
	retval = fsd_process_loop();

	EXA_ASSERT_VERBOSE(retval==0, "error %d during event loop", retval);
    }

    examsgDelMbox(mh, EXAMSG_FSD_ID);

    /* Exit examsg */
    examsgExit(mh);

    exalog_static_clean();

    examsg_static_clean(EXAMSG_STATIC_RELEASE);

    os_daemon_unlock(exa_fsd_daemon_lock);

    os_closelog();

    return EXA_SUCCESS;
}

