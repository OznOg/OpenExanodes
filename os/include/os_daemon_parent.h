/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef OS_DAEMON_PARENT_H
#define OS_DAEMON_PARENT_H

#include "os/include/os_inttypes.h"

#ifdef WIN32

#include <stdlib.h>

#include "os/include/os_windows.h"

typedef HANDLE os_daemon_t;
#define OS_DAEMON_INVALID ((os_daemon_t)NULL)

#else  /* WIN32 */

#include <unistd.h>

typedef pid_t os_daemon_t;
/* We'll never have a daemon with pid 0 */
#define OS_DAEMON_INVALID ((os_daemon_t)0)

#endif

/**
 * Spawn a daemon.
 *
 * The function waits for the spawned daemon to finish its initialization
 * (successfully or not).
 *
 *   - If the daemon exited, the function returns the daemon's exit code.
 *
 *   - If the daemon's initialization failed, the function returns the error
 *     code returned by the daemon's daemon_init().
 *
 * @note The resulting handle must be freed when done with.
 *
 * @param[in]  argv    NULL-terminated commandline
 * @param[out] daemon  Handle on the daemon spawned (if successful)
 *
 * @return 0 if successful, a negative error code otherwise.
 *
 * @os_replace{Linux, fork, exec}
 * @os_replace{Windows, CreateProcess}
 */
int os_daemon_spawn(char *const argv[], os_daemon_t *daemon);

/**
 * Get the PID of the calling daemon.
 *
 * @return PID
 *
 * @os_replace{Linux, getpid}
 * @os_replace{Windows, GetCurrentProcessId, _getpid}
 */
uint32_t os_daemon_current_pid(void);


/**
 * Get a daemon handle from a process ID.
 *
 * @note The handle must be freed when done with.
 *
 * @param[out] daemon  Handle on the daemon (if successful)
 * @param[in] pid     Process ID
 *
 * @return 0 if successful, a negative error code otherwise.
 *
 * @os_replace{Windows, OpenProcess}
 */
int os_daemon_from_pid(os_daemon_t *daemon, uint32_t pid);

/**
 * Get the process ID of a daemon.
 *
 * @param[in]  daemon  Daemon to get the PID of
 * @param[out] pid     Daemon's PID (if successful)
 *
 * @return 0 if successful, a negative error code otherwise.
 *
 * @os_replace{Windows, GetProcessId}
 */
int os_daemon_to_pid(os_daemon_t daemon, uint32_t *pid);

/**
 * Free a daemon handle.
 *
 * @note Don't use this function directly. Use macro os_daemon_free()
 * instead as it is safer.
 *
 * @param[in] daemon  Pointer to daemon to free
 *
 * @os_replace{Windows, CloseHandle}
 */
void __os_daemon_free(os_daemon_t *daemon);

/** Free a daemon handle and set it to OS_DAEMON_INVALID */
#define os_daemon_free(daemon)  __os_daemon_free(&daemon)

/**
 * Check the status of a daemon.
 *
 * @param[in]  daemon  Daemon to check
 * @param[out] alive   Whether the daemon is alive (optional)
 * @param[out] status  Exit status of the daemon (optional)
 *
 * @return 0 if successful, a negative error code otherwise
 *
 * @note The status returned is only valid when the daemon is not alive.
 *
 * @os_replace{Linux, waitpid}
 * @os_replace{Windows, GetExitCodeProcess}
 */
int os_daemon_status(os_daemon_t daemon, bool *alive, int *status);

/**
 * Check wether a daemon exists as a process.
 *
 * @note Limitations:
 *     - This function doesn't look at the state of the daemon and thus may
 *       return true even if the daemon is not "alive" (zombie on Linux).
 *     - The daemon isn't reaped if it's dead.
 *   Because of these limitations, os_daemon_status() should be used when
 *   checking a process from is parent.
 *
 * @param[in] daemon  Daemon to check
 *
 * @return true if the process exists, false otherwise
 *
 * @os_replace{Linux, kill}
 * @os_replace{Windows, GetExitCodeProcess}
 */
bool os_daemon_exists(os_daemon_t daemon);

/**
 * Forcefully terminate a daemon.
 *
 * The daemon is terminated in the most brutal way available on the platform.
 *
 * @note There is no guarantee that the daemon does not exist anymore
 * when the function returns. Use os_daemon_wait() to ensure that and
 * reap the process.
 *
 * @note There is no guarantee that the exit code of the process is the
 * same on all platforms. The only guarantee is that it is non-zero.
 *
 * @note The daemon handle is *not* freed.
 *
 * @param[in] daemon  Daemon to terminate
 *
 * @os_replace{Linux, kill}
 * @os_replace{Windows, TerminateProcess}
 */
void os_daemon_terminate(os_daemon_t daemon);

/**
 * Ask a daemon to quit.
 *
 * This function is the 'gentle' way of terminating a daemon.  There is no
 * guarantee the daemon has quit when the function returns nor that it will
 * quit later on.
 *
 * @note The daemon handle is *not* freed.
 *
 * @param[in] daemon  Daemon that ought to quit
 *
 * @os_replace{Linux, kill}
 */
void os_daemon_tell_quit(os_daemon_t daemon);

/**
 * Wait for a daemon to terminate.
 *
 * @note Calling this function is mandatory to ensure that the daemon is
 * properly reaped.
 *
 * @note The daemon handle is *not* freed.
 *
 * @param[in] daemon  Daemon to wait for
 *
 * @return The (positive) exit status of the daemon, or a negative error
 *         code.
 *
 * @os_replace{Linux, waitpid}
 * @os_replace{Windows, WaitForSingleObject}
 */
int os_daemon_wait(os_daemon_t daemon);

#endif /* OS_DAEMON_PARENT_H */
