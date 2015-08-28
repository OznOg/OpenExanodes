/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/*
 * Monitoring of daemons.
 *
 * NOTE: Only children may be monitored because of the way the liveness
 * check is implemented.
 */

#include "adm_monitor.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>

#include "log/include/log.h"
#include "common/include/exa_names.h"
#include "common/include/exa_error.h"
#include "os/include/os_thread.h"
#include "os/include/os_assert.h"

/** Monitored daemons */
static os_daemon_t monitored[EXA_DAEMON__LAST + 1];

/** Whether a daemon is expected to die */
static bool death_expected[EXA_DAEMON__LAST + 1];

/** Lock protecting the above array */
static os_thread_mutex_t monitor_lock;

#define LOCK()  os_thread_mutex_lock(&monitor_lock)
#define UNLOCK()  os_thread_mutex_unlock(&monitor_lock)

/* Delays between attempts of terminating the daemons. Evaluate to false
 * so as to be chainable with boolean actions and not break the chain. */
#define RETRY_DELAY_1()  (os_millisleep(100), false)
#define RETRY_DELAY_2()  (os_sleep(1), false)
#define RETRY_DELAY_3()  (os_sleep(3), false)

/* Try a given action four times with increasing delays in-between. Stops as
 * soon as the action returns true. Returns true if the action succeeded,
 * false otherwise */
#define TRY_4_TIMES(action)                     \
    ((action)                                   \
     || RETRY_DELAY_1() || (action)             \
     || RETRY_DELAY_2() || (action)             \
     || RETRY_DELAY_3() || (action))

/**
 * Initialize daemon monitoring.
 */
void adm_monitor_init(void)
{
  exa_daemon_id_t id;

  os_thread_mutex_init(&monitor_lock);

  LOCK();

  for (id = EXA_DAEMON__FIRST; id <= EXA_DAEMON__LAST; id++)
  {
      monitored[id] = OS_DAEMON_INVALID;
      death_expected[id] = false;
  }

  UNLOCK();
}

/**
 * Register a daemon for monitoring.
 *
 * \param[in] id  Daemon id
 *
 * return 0 if successfull, -ESRCH if no process found
 */
int adm_monitor_register(exa_daemon_id_t id, os_daemon_t daemon)
{
  const char *name;

  name = exa_daemon_name(id);
  EXA_ASSERT(name);

  LOCK();

  EXA_ASSERT_VERBOSE(monitored[id] == OS_DAEMON_INVALID,
		     "daemon '%s' already registered", name);
  monitored[id] = daemon;
  death_expected[id] = false;

  UNLOCK();

  exalog_trace("registered '%s' ", name);

  return 0;
}

/**
 * Unregister a daemon from monitoring.
 *
 * \param[in] id  Daemon id
 *
 * \return 0 if successfull, -ESRCH if no process found
 */
int adm_monitor_unregister(exa_daemon_id_t id)
{
  const char *name;

  name = exa_daemon_name(id);
  EXA_ASSERT(name);

  LOCK();

  monitored[id] = OS_DAEMON_INVALID;
  death_expected[id] = false;

  UNLOCK();

  exalog_trace("unregistered '%s' ", name);

  return 0;
}

/**
 * Check liveness of all monitored daemons.
 */
void adm_monitor_check(void)
{
  exa_daemon_id_t id;
  bool alive;
  int status;

  LOCK();

  for (id = EXA_DAEMON__FIRST; id <= EXA_DAEMON__LAST; id++)
  {
      if (monitored[id] == OS_DAEMON_INVALID)
          continue;

      /* If daemon is supposed to be dying, no need to check for liveness */
      if (death_expected[id])
	  continue;

      if (os_daemon_status(monitored[id], &alive, &status) == 0
          && !alive)
      {
	const char *name = exa_daemon_name(id);
	EXA_ASSERT_VERBOSE(false, "daemon %s is dead (exit status %d)"
			   " => ABORTING", name, status);
      }
  }

  UNLOCK();
}

/**
 * Check liveness of one daemon, if it should be alive, given
 * its daemon_id (used in clinfo).
 * return 0 if alive or if it is not registered, -ESRCH otherwise.
 */
int
adm_monitor_check_one_daemon(exa_daemon_id_t id)
{
  bool alive;
  int ok;

  LOCK();

  /* Do not take 'death_expected' into account: all the caller wants to know
   * is whether the daemon is alive. */
  ok = (monitored[id] == OS_DAEMON_INVALID
        || (os_daemon_status(monitored[id], &alive, NULL) == 0 && alive));

  UNLOCK();

  return ok ? 0 : -ESRCH;
}

/**
 * Try to terminate a monitored daemon with the specified method.
 * Resets the monitored daemon to OS_DAEMON_INVALID if it's
 * successfully terminated.
 *
 * @note This function does *not* take the lock.
 *
 * @param[in]  id      Id of the daemon to terminate
 * @param[in]  method  Termination method to use
 * @param[out] status  Exit status of the daemon
 *
 * @return true if daemon terminated (and reaped), false otherwise
 */
static bool __unsafe_terminate(exa_daemon_id_t id, adm_monitor_method_t method,
                               int *status)
{
    bool alive;

    if (monitored[id] == OS_DAEMON_INVALID)
        return true;

    death_expected[id] = true;

    switch (method)
    {
    case ADM_MONITOR_METHOD_GENTLE:
        os_daemon_tell_quit(monitored[id]);
        break;

    case ADM_MONITOR_METHOD_BRUTAL:
        os_daemon_terminate(monitored[id]);
        break;
    }

    if (os_daemon_status(monitored[id], &alive, status) == 0 && !alive)
    {
        monitored[id] = OS_DAEMON_INVALID;
        death_expected[id] = false;
        return true;
    }

    return false;
}

/**
 * Same as __unsafe_terminate() but taking the lock.
 */
static bool __terminate(exa_daemon_id_t id, adm_monitor_method_t method,
                        int *status)
{
    bool ok;

    LOCK();
    ok = __unsafe_terminate(id, method, status);
    UNLOCK();

    return ok;
}

/**
 * Try to terminate all monitored daemons with the specified method.
 * Resets the monitored daemons successfully terminated to OS_DAEMON_INVALID.
 *
 * @param[in] method  Termination method to use
 *
 * @return true if all daemons have been terminated (and reaped), false otherwise
 */
static bool __unsafe_terminate_all(adm_monitor_method_t method)
{
    bool all_terminated = true;
    exa_daemon_id_t id;

    for (id = EXA_DAEMON__FIRST; id <= EXA_DAEMON__LAST; id++)
        if (!__unsafe_terminate(id, method, NULL))
            all_terminated = false;

    return all_terminated;
}

/**
 * Terminate a daemon.
 *
 * First, kindly ask the daemon to quit and do that several times. Then, if it
 * insists on running despite the repeated requests to stop, kill it brutally.
 *
 * @note Has an effect *iff* done after the daemon is registered and before it
 *       is unregistered
 *
 * @param[in] id      Daemon id
 *
 * @return 0 or a negative error code.
 */
int adm_monitor_terminate(exa_daemon_id_t id)
{
    const char *name;
    os_daemon_t daemon;
    int status;

    name = exa_daemon_name(id);
    EXA_ASSERT(name);

    exalog_debug("Terminating daemon %s", name);

    LOCK();
    daemon = monitored[id];
    UNLOCK();

    if (daemon == OS_DAEMON_INVALID)
        return 0;

    if (TRY_4_TIMES(__terminate(id, ADM_MONITOR_METHOD_GENTLE, &status)))
        goto success;

    exalog_warning("Failed to gently terminate daemon %s. Trying to kill it.",
                   name);

    if (TRY_4_TIMES(__terminate(id, ADM_MONITOR_METHOD_BRUTAL, &status)))
        goto success;

    /* At this point, status is a negative error code */
    exalog_error("Failed terminating daemon %s: %s", name, exa_error_msg(status));
    return status;

success:
    exalog_debug("Terminated daemon %s", name);

    return 0;
}

/**
 * Terminate all daemons without taking the lock.
 *
 * Several attempts are made to terminate the daemons using the specified
 * method. If that fails, terminate the daemons with ADM_MONITOR_METHOD_BRUTAL.
 *
 * @note Calling this function is only useful when trying to perform some
 * emergency action before crashing. DO *NOT* USE IT IN ANY OTHER CASE.
 *
 * @param[in] method  Termination method
 */
void adm_monitor_unsafe_terminate_all(adm_monitor_method_t method)
{
    OS_ASSERT(ADM_MONITOR_METHOD_VALID(method));


#ifdef WIN32
    /* The only method implemented on Windows is BRUTAL */
    method = ADM_MONITOR_METHOD_BRUTAL;
#endif

    if (TRY_4_TIMES(__unsafe_terminate_all(method)))
        return;

    if (method != ADM_MONITOR_METHOD_BRUTAL)
    {
        exalog_warning("Failed to gently terminate some daemons."
                       " Trying to kill them.");

        if (!TRY_4_TIMES(__unsafe_terminate_all(ADM_MONITOR_METHOD_BRUTAL)))
            exalog_warning("Failed killing some daemons.");
    }
}
