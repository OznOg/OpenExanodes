/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef OS_DAEMON_LOCK_H
#define OS_DAEMON_LOCK_H

struct os_daemon_lock;
typedef struct os_daemon_lock os_daemon_lock_t;


/**
 * Lock a daemon.
 * - Under Linux, it creates a pid file if it does not exist and stores in it
 *   the pid of the daemon.
 * - Under Windows, it creates an event which will be deleted by the system
 *   upon daemon termination.
 *
 * @param[in] daemon_name  Name of calling daemon
 * @param[in] daemon_lock  Identifier of the daemon lock
 *
 * @return 0 if successful, a negative error code otherwise
 */
int os_daemon_lock(const char *daemon_name, os_daemon_lock_t **daemon_lock);

/**
 * Unlock a daemon.
 *
 * @param[in] daemon_lock  Identifier of the daemon lock
 */
void os_daemon_unlock(os_daemon_lock_t *daemon_lock);


#endif /* OS_DAEMON_LOCK_H */
