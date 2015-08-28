/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADM_MONITOR_H__
#define __ADM_MONITOR_H__

#include "common/include/exa_names.h"
#include "os/include/os_daemon_parent.h"

void adm_monitor_init(void);

int adm_monitor_register(exa_daemon_id_t id, os_daemon_t daemon);
int adm_monitor_unregister(exa_daemon_id_t id);

void adm_monitor_check(void);
int adm_monitor_check_one_daemon(exa_daemon_id_t id);

int adm_monitor_terminate(exa_daemon_id_t id);

/* Daemon termination methods */
typedef enum
{
    ADM_MONITOR_METHOD_GENTLE,  /* Give the daemon a chance to exit cleanly */
    ADM_MONITOR_METHOD_BRUTAL   /* Vaporize the daemon */
} adm_monitor_method_t;

#define ADM_MONITOR_METHOD_VALID(t) \
    ((t) == ADM_MONITOR_METHOD_GENTLE || (t) == ADM_MONITOR_METHOD_BRUTAL)

void adm_monitor_unsafe_terminate_all(adm_monitor_method_t method);

#endif
