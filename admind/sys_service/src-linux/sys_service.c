/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/admind_daemon.h"
#include "common/include/exa_constants.h" /* for ADMIND_PROCESS_NICE */
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"

#include "os/include/os_error.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_file.h"
#include "os/include/os_stdio.h"
#include "os/include/os_time.h"
#include "os/include/os_syslog.h"

#include <alloca.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>

static const char *program;

static int pipe_fds[2] = {0, 0};

/**
 * Daemonize the current process.
 *
 * Fork and wait for the child to signal parent via exa_detach_parent().
 * Close all file descriptors.
 * Lock all pages in memory.
 *
 * @param[in] stack	Estimated stack size for the process.
 * @param[in] name	Name of the daemon.
 *
 * @return 0 on success or an error code.
 */
static int exa_daemonize(size_t stack, const char *name)
{
  pid_t pid;
  int ret;
  int i;

  os_openlog(name);

  if (pipe(pipe_fds) == -1) {
    ret = -errno;
    fprintf(stderr, "cannot create pipe: %m\n");
    os_syslog(OS_SYSLOG_ERROR, "cannot create pipe: %m");
    exit(1);
  }

  pid = fork();
  switch(pid) {
    case -1:
      ret = -errno;
      os_syslog(OS_SYSLOG_ERROR, "cannot fork: %m");
      exit(1);

    case 0: { /* child */
      struct rlimit limit = {RLIM_INFINITY, RLIM_INFINITY};
      void *dummy;

      ret = chdir(RUNNING_DIR);
      if (ret == -1)
      {
	ret = -errno;
        os_syslog(OS_SYSLOG_ERROR, "chdir(" RUNNING_DIR ") failed: %s",
                  exa_error_msg(ret));
	return ret;
      }
      setrlimit(RLIMIT_CORE, &limit);
      os_closelog();

      /* close file descriptors (except the pipe with our parent) */
      for (i = getdtablesize(); i >= 0; i--)
	if (i != pipe_fds[1])
	  close(i);

      i = open("/dev/null", O_RDWR);
      ret = dup(i);
      if (ret == -1)
      {
	ret = -errno;
        os_syslog(OS_SYSLOG_ERROR, "dup() failed: %s", exa_error_msg(ret));
	return ret;
      }
      ret = dup(i);
      if (ret == -1)
      {
	ret = -errno;
        os_syslog(OS_SYSLOG_ERROR, "dup() failed: %s", exa_error_msg(ret));
	return ret;
      }

      /* make sure we have enough room for the stack */
      dummy = alloca(stack);
      memset(dummy, 0, stack);

      /* lock everything... */
      if (mlockall(MCL_CURRENT|MCL_FUTURE))
	{
	  ret = -errno;
          os_syslog(OS_SYSLOG_ERROR, "cannot lock: %m");
	  return ret;
	}

      return 0;
    }

    default: { /* parent */
      fd_set rfds;
      ssize_t count;

      /* Close unused end */
      close (pipe_fds[1]);

      /* Monitor pipe */
      FD_ZERO (&rfds);
      FD_SET (pipe_fds[0], &rfds);

      ret = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
      if (ret <= 0) {
	fprintf(stderr, "select() failed for `%s': %m\n", name);
        os_syslog(OS_SYSLOG_ERROR, "select() failed: %m");
	exit(1);
      }

      count = read(pipe_fds[0], &ret, sizeof(ret));
      if (count < 0) {
	fprintf(stderr, "read() failed: %m\n");
        os_syslog(OS_SYSLOG_ERROR, "read() failed: %m");
	exit(1);
      }
      if (count != sizeof(ret)) {
	fprintf(stderr, "read(): bad count %" PRIzd " (broken pipe ?)\n", count);
        os_syslog(OS_SYSLOG_ERROR, "read(): bad count %" PRIzd " (broken pipe ?)", count);
	exit(1);
      }
      if (ret) {
	fprintf(stderr, "failed to daemonize `%s': %s\n",
		name, exa_error_msg(ret));
        os_syslog(OS_SYSLOG_ERROR, "failed to daemonize: %s",
                  exa_error_msg(ret));
	waitpid(pid, NULL, 0);
	exit(1);
      } else {
        os_syslog(OS_SYSLOG_INFO, "started");
	exit(0);
      }

    }
  }

  return EXA_SUCCESS;
}


/**
 * Return the error/success code from the child
 *
 * it can be used only once
 *
 */
static void exa_detach_parent(enum exa_error_code error_code)
{
  ssize_t count;

  if (!error_code)
    setsid ();

  if(pipe_fds[1] == 0)
  {
    os_syslog(OS_SYSLOG_ERROR, "trying to detach before deamonizing");
    exit(1);
  }

  count = write (pipe_fds[1], &error_code, sizeof (error_code));
  if (count != sizeof (error_code))
    os_syslog(OS_SYSLOG_ERROR, "write() failed: %m");
  close (pipe_fds[1]);
}

static int run_standalone(void)
{
    int err;

    err = admind_init(true);
    if (err)
    {
        /* XXX There is no mean to flush exalog before exiting, thus this
         * sleep is a hack to give some time to logd to write its messages.
         * This is for sure ugly, but only happens in case of error, so this
         * is really not harmful. */
        os_sleep(1);
        return err;
    }

    err = admind_main();
    admind_cleanup();

    return err;
}

static int run_as_service(void)
{
    int err;

    err = exa_daemonize(20480, exa_daemon_name(EXA_DAEMON_ADMIND));
    if (err)
    {
        os_syslog(OS_SYSLOG_ERROR, "Failed to daemonize");
        exa_detach_parent(err);
        return err;
    }

    /* FIXME Is this renice really mandatory here? why not just renicing
     * the process inside the init scrip? This would ease portability,
     * but there are maybe some unpredictable side effects...  (If you
     * remove the nice, do not forget to remove the unistd.h header from
     * includes) */
    err = nice(ADMIND_PROCESS_NICE);
    if (err == -1)
    {
        os_syslog(OS_SYSLOG_ERROR, "Failed to renice");
        exa_detach_parent(err);
        return 1;
    }

    err = admind_init(false);

    exa_detach_parent(err);

    if (err)
    {
        /* Sleep: see comment in run_standalone() */
        os_sleep(1);
        return err;
    }

    err = admind_main();
    admind_cleanup();

    return err;
}

/* Keep in sync with Windows version! */
static void usage(void)
{
    printf("Usage: %s [options]\n", program);
    printf("Options:\n"
           "  -f, --foreground  Run in foreground\n"
           "  -h, --help        Display this help and exit\n"
           "  -v, --version     Output version information and exit\n");
}

static bool opt_is(const char *opt, const char *short_val, const char *long_val)
{
    if (strcmp(opt, short_val) == 0)
        return true;

    if (long_val != NULL && strcmp(opt, long_val) == 0)
        return true;

    return false;
}

int main(int argc, char *argv[])
{
    const char *opt;
    int err = 0;

    program = argv[0];

    switch (argc)
    {
    case 1:
        err = run_as_service();
        break;

    case 2:
        opt = argv[1];
        /* '-fg' for backward compatibility */
        if (opt_is(opt, "-f", "--foreground") || opt_is(opt, "-fg", NULL))
            err = run_standalone();
        else if (opt_is(opt, "-h", "--help"))
            usage();
        else if (opt_is(opt, "-v", "--version"))
            admind_print_version();
        else
        {
            fprintf(stderr, "Invalid option: '%s'\n", opt);
            err = 1;
        }
        break;

    default:
        fprintf(stderr, "Too many arguments\n");
        err = 1;
    }

    return err ? 1 : 0;
}
