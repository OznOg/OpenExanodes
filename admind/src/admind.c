/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/admind.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "admind/src/admind_daemon.h"
#include "admind/src/adm_cache.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_deserialize.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_license.h"
#include "admind/src/adm_monitor.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_serialize.h"
#include "admind/src/adm_hostname.h"
#include "admind/src/admindstate.h"
#include "admind/src/cli_server.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/xml_proto/xml_protocol_version.h"
#include "common/include/exa_config.h"
#include "os/include/os_mem.h"
#include "common/include/exa_env.h"
#include "common/include/exa_error.h"
#include "config/exa_version.h"
#include "log/include/log.h"
#include "examsg/include/examsg.h"

#include "os/include/os_dir.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_kmod.h"
#include "os/include/os_random.h"
#include "os/include/os_time.h"
#include "os/include/strlcpy.h"
#include "os/include/os_syslog.h"
#include "os/include/os_daemon_lock.h"

#include "git.h"

#ifdef WIN32
#include "os/include/os_windows.h"
#endif

static const char version[] =
    "Exanodes " EXA_EDITION_STR " " EXA_VERSION " for " EXA_PLATFORM_STR
    " (r" GIT_REVISION ")";


static bool quit = false;  /**< Whether admind should quit */

os_daemon_lock_t *admind_daemon_lock;

static void
signal_handler(int sig)
{
  switch (sig)
    {
    case SIGABRT:
      adm_monitor_unsafe_terminate_all(ADM_MONITOR_METHOD_GENTLE);
      abort();
      break;

    case SIGSEGV:
      adm_monitor_unsafe_terminate_all(ADM_MONITOR_METHOD_GENTLE);
      raise(SIGSEGV);
      break;

#ifndef WIN32
    case SIGINT:
      exalog_debug("SIGINT received.");
      admind_quit();
      break;

    case SIGQUIT:
      exalog_debug("SIGQUIT received.");
      admind_quit();
      break;
#endif

    case SIGTERM:
      exalog_debug("SIGTERM received.");
      admind_quit();
      break;

#if defined(WITH_MEMTRACE) && !defined(WIN32)
    case SIGUSR1:
      os_meminfo("Admind", OS_MEMINFO_DETAILED);
      break;
#endif

    default:
#ifdef WIN32
      exalog_debug("got unexpected signal %d", sig);
#else
      /* XXX Should use strsignal() */
      exalog_debug("got unexpected signal %s", strsignal(sig));
#endif
    }
}

/* ---------------------------------------- */

void admind_print_version(void)
{
  printf("%s\n", version);
  printf("Seanodes Exanodes management daemon\n");
  printf("%s\n", EXA_COPYRIGHT);
  printf("XML Protocol version = %s\n", XML_PROTOCOL_VERSION);
}

/**
 * Load the configuration file.
 *
 * If successful, sets the goal to the persistent goal read from the config
 * file. Otherwise, leaves the goal unchanged.
 */
static int
load_config(void)
{
    cl_error_desc_t error_desc;
    char error_msg[EXA_MAXSIZE_LINE + 1] = "";
    int error_val;
    struct stat sb;
    char conf_path[OS_PATH_MAX];
    char license_path[OS_PATH_MAX];

    exa_env_make_path(conf_path, sizeof(conf_path), exa_env_cachedir(),
                      ADMIND_CONF_EXANODES_FILE);
    exa_env_make_path(license_path, sizeof(license_path), exa_env_cachedir(),
                      ADM_LICENSE_FILE);

    exalog_debug("Loading config file '%s'", conf_path);

    /* check if file exist */
    if (stat(conf_path, &sb) == -1)
    {
	if (errno == ENOENT)
	{
	    exalog_info("No config file");
	    adm_set_state(ADMIND_NOCONFIG);
	    return EXA_SUCCESS;
	}
	else
	{
            exalog_error("Failed to read Exanodes config file '%s': %s (%d)",
                         conf_path, strerror(errno), -errno);
	    return -errno;
	}
    }

    /* Only a regular file is accepted */
    if ((sb.st_mode & S_IFMT) != S_IFREG)
    {
        exalog_error("Failed to read Exanodes config file '%s':"
                     "Not a regular file", conf_path);
	return -EINVAL;
    }

    error_val = adm_deserialize_from_file(conf_path,
	                                  error_msg, false /* create */);

    if (error_val != EXA_SUCCESS)
    {
        exalog_error("Failed to read Exanodes config file '%s': %s",
                     conf_path, error_msg);
	return error_val;
    }

    exalog_debug("Loading license file '%s'", license_path);

    /* Ignore errors. A valid license is not required at start*/
    exanodes_license = adm_deserialize_license(license_path, &error_desc);

    /* If the license file is correct but the license is not for the current
     * product, it is refused. */
    if (exanodes_license != NULL
        && !adm_license_matches_self(exanodes_license, &error_desc))
    {
        adm_license_delete(exanodes_license);
        exanodes_license = NULL;
    }

    if (exanodes_license == NULL)
    {
        exalog_error("Failed to load license: %s", error_desc.msg);
	return error_desc.code;
    }
    else
    {
        exalog_info("%s license file loaded (status '%s', UUID '"UUID_FMT"')",
                    adm_license_is_eval(exanodes_license) ? "Evaluation":"Full",
                    adm_license_status_str(
                        adm_license_get_status(exanodes_license)),
                    UUID_VAL(adm_license_get_uuid(exanodes_license)));
    }

    /* Set adm_config_size and adm_config_buffer that are needed during the
     * recovery of admind. */
    /* FIXME why it this stuff done here and not when actually reading the
     * config file ? */
    adm_config_size = adm_serialize_to_null(false /* create */);
    adm_config_buffer = os_malloc(adm_config_size + 1);
    adm_serialize_to_memory(adm_config_buffer, adm_config_size + 1, false);

    exalog_info("Config file loaded (cluster '%s', UUID '" UUID_FMT "')",
	         adm_cluster.name, UUID_VAL(&adm_cluster.uuid));

    /* Valid config and goal files were found, so we are STOPPED; if the goal
     * is to be started, the state STARTED will be set at the end of recovery */
    adm_set_state(ADMIND_STOPPED);

    return EXA_SUCCESS;
}

static void
datastruct_init(void)
{
  char hostname[EXA_MAXSIZE_HOSTNAME + 1];

  if (adm_hostname_load(hostname))
    exalog_info("no hostname file, will use real hostname");
  else
    adm_hostname_override(hostname);

  adm_cluster_init();

  adm_leader_set = false;
  adm_leader_id  = EXA_NODEID_NONE;

  /* Init the random generator (UUIDs, tmp files) */
  os_random_init();

  /* Initialize the config library */
  xml_conf_init();

  /* Initialize the public key to check license */
  adm_license_static_init();

  adm_state_static_init(ADMIND_NOCONFIG);

  rpc_command_resetall();

  inst_static_init();

  adm_command_init_processing();
  adm_service_init_command_processing();
}

static t_work *clinfo_thr;
static t_work *clicommand_thr;
static t_work *recovery_thr;

static int
worker_threads_start(void)
{
  int retval;

  retval = launch_worker_thread(&clinfo_thr,
                                CLINFO_THR_ID, EXAMSG_ADMIND_INFO_ID,
			        EXAMSG_ADMIND_INFO_LOCAL,
			        EXAMSG_ADMIND_INFO_BARRIER_ODD,
			        EXAMSG_ADMIND_INFO_BARRIER_EVEN);
  if (retval)
    return retval;

  retval = launch_worker_thread(&clicommand_thr,
                                CLICOMMAND_THR_ID, EXAMSG_ADMIND_CMD_ID,
				EXAMSG_ADMIND_CMD_LOCAL,
				EXAMSG_ADMIND_CMD_BARRIER_ODD,
				EXAMSG_ADMIND_CMD_BARRIER_EVEN);
  if (retval)
    return retval;

  return launch_worker_thread(&recovery_thr,
                              RECOVERY_THR_ID, EXAMSG_ADMIND_RECOVERY_ID,
	                      EXAMSG_ADMIND_RECOVERY_LOCAL,
			      EXAMSG_ADMIND_RECOVERY_BARRIER_ODD,
			      EXAMSG_ADMIND_RECOVERY_BARRIER_EVEN);
}

static void worker_threads_stop(void)
{
  stop_worker_thread(clinfo_thr);
  stop_worker_thread(clicommand_thr);
  stop_worker_thread(recovery_thr);
}

static void setup_signal_handlers(void)
{
#ifdef WIN32
    signal(SIGTERM, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGSEGV, signal_handler);
#else
    struct sigaction action;

    /*
     * FIXME WIN32 signal framework issue
     * The whole signal framework is hardly portable on windows.
     * For some functions it may be possible to use some other mechanisme
     * (examsg ?) to perform the action, for some others some equivalent
     * should be necessary...
     */
    /* block all signals inside handler */
    sigfillset(&action.sa_mask);
    action.sa_handler = signal_handler;

    /* restore handler when called so that the core file and frinds can be
     * automaticaly performed by the system */
    action.sa_flags = SA_RESETHAND;

    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGINT,  &action, NULL);
#ifdef WITH_MEMTRACE
    action.sa_flags = 0;
    sigaction(SIGUSR1, &action, NULL);
#endif
    action.sa_handler = SIG_IGN;
    action.sa_flags = 0;
    sigaction(SIGTTOU, &action, NULL);
    sigaction(SIGTTIN, &action, NULL);
    sigaction(SIGPIPE, &action, NULL);
    /* Do NOT ignore SIGCHLD: we need it in order to get waitpid work
     * Do NOT catch SIGHUP, the loging thread needs it */
#endif /* WIN32 */
}

int admind_init(bool foreground)
{
    adm_cluster_goal_t cluster_goal;
    int err;

    /* XXX When foreground is true, don't open the syslog and print to
     * stdout/stderr instead */
    os_openlog("Exanodes");

    if (!exa_env_properly_set())
    {
        os_syslog(OS_SYSLOG_ERROR, "Environment not properly set");
        return -EXA_ERR_BAD_INSTALL;
    }

    err = os_daemon_lock(exa_daemon_name(EXA_DAEMON_ADMIND), &admind_daemon_lock);
    if (err == -EEXIST)
    {
	os_syslog(OS_SYSLOG_ERROR, "Admind is already running");
	return -ADMIND_ERR_ALREADY_RUNNING;
    }

    if (err)
    {
	os_syslog(OS_SYSLOG_ERROR, "Failed creating Admind lock");
	return -EXA_ERR_LOCK_CREATE;
    }

    setup_signal_handlers();

#if USE_EXA_COMMON_KMODULE
    /* FIXME Add module unloading in some admind_cleanup() to be added (and
     * that would be mandatory to call before exiting). */
    err = os_kmod_load("exa_common");
    if (err)
    {
        os_syslog(OS_SYSLOG_ERROR, "Failed to load kernel module 'exa_common'");
        return -ADMIND_ERR_MODULESTART;
    }
#endif

    err = examsg_static_init(EXAMSG_STATIC_CREATE);
    if (err)
    {
        exalog_error("Failed to initialize messaging subsystem: %s",
                     exa_error_msg(err));
        return err;
    }

    err = exalog_thread_start();
    if (err)
    {
        fprintf(stderr, "Failed to start logging daemon. %s (%d)\n"
                "Maybe exanodes modules are not loaded\n",
                exa_error_msg(err), err);
        return err;
    }

    /* MUST be called BEFORE spawning threads */
    exalog_static_init();

    /* Now log thread is spawned we can use exalog_ */
    exalog_as(EXAMSG_ADMIND_ID);

    exalog_info("------------------------------------------------------------");
    exalog_info("%s", version);
    exalog_info("------------------------------------------------------------");

    err = os_net_init();
    if (err)
    {
        exalog_error("Failed to initialize network subsystem: %s (%d)",
		     exa_error_msg(err), err);
        return -err;
    }

    datastruct_init();

    /* Create the directory (if needed) to store our configuration file */
    err = adm_cache_create();
    if (err != 0)
    {
        exalog_error("Failed to create %s: %s (%d)",
		     exa_env_cachedir(), exa_error_msg(err), err);
        return -ADMIND_ERR_CREATE_DIR;
    }

    err = load_config();
    if (err)
        return err;

    err = adm_cluster_load_goal(&cluster_goal);
    if (err)
        return err;

    adm_cluster.persistent_goal = cluster_goal;
    adm_cluster.goal = cluster_goal;

    exalog_info("The goal of this node is '%s'",
                adm_cluster.goal == ADM_CLUSTER_GOAL_STARTED ? "STARTED" : "STOPPED");

    /* Consistency check: a cluster that is not created cannot have a goal other
     * than UNDEFINED... falling in this error means that something went wrong
     * during a cldelete or (usually) that a manual cleanup of config files
     * was badly performed... */
    if (adm_cluster.goal != ADM_CLUSTER_GOAL_UNDEFINED && !adm_cluster.created)
    {
        exalog_error("Inconsistent state of configuration found;"
                     " Probably a cleanup failed.");
        return -EINVAL;
    }

    err = worker_threads_start();
    if (err)
    {
        exalog_error("Failed to start worker threads: %s", exa_error_msg(err));
        return err;
    }

    err = evmgr_init();
    if (err)
    {
        exalog_error("Failed to initialize event manager: %s", exa_error_msg(err));
        return err;
    }

    /* Config file says that the node should restart so we trigger the init
     * of processes and expect the cluster to start */
    if (adm_cluster.goal == ADM_CLUSTER_GOAL_STARTED)
    {
        cl_error_desc_t err_desc;

        /* no need to save the command uid, as no cli is attached... */
        evmgr_process_init(get_new_cmd_uid(), &err_desc);
        if (err_desc.code)
	{
            exalog_error("Failed to start processes for automatic restart: %s (%d)",
                         err_desc.msg, err_desc.code);
            return err_desc.code;
	}
    }

    /* WARNING cli server MUST be started AFTER the worker threads and evmgr
     * otherwise there might be incoming connections whit no WT nor evmgr to
     * handle them */
    err = cli_server_start();
    if (err)
    {
        exalog_error("Failed to start CLI server: %s", exa_error_msg(err));
        return err;
    }

    exalog_debug("initialization ok");
    return 0;
}

void admind_cleanup(void)
{
    adm_monitor_unsafe_terminate_all(ADM_MONITOR_METHOD_GENTLE);

    cli_server_stop();

    evmgr_cleanup();

    worker_threads_stop();
    adm_cluster_cleanup();

    exalog_static_clean();
    exalog_thread_stop();

    examsg_static_clean(EXAMSG_STATIC_DELETE);

#ifdef USE_EXA_COMMON_KMODULE
    os_kmod_unload("exa_common");
#endif

    os_daemon_unlock(admind_daemon_lock);

    conf_cleanup();
    xmlCleanupParser();

    os_meminfo("Admind", OS_MEMINFO_DETAILED);
}

void admind_quit(void)
{
    exalog_debug("quit requested");
    quit = true;
}

int admind_main(void)
{
    while (!quit)
    {
        int err = evmgr_handle_msg();
        EXA_ASSERT_VERBOSE(err <= 0, "Invalid return value: %d", err);
        evmgr_process_events();
    }
    exalog_debug("quit");

    return 0;
}
