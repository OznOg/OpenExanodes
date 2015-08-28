/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EVENTS_H__
#define __EVENTS_H__

#include "examsg/include/examsg.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/adm_error.h"

int evmgr_init(void);
void evmgr_cleanup(void);
int evmgr_handle_msg(void);
void evmgr_process_events(void);
void evmgr_process_init(cmd_uid_t cuid, cl_error_desc_t *err_desc);
int evmgr_is_recovery_in_progress(void);
void evmgr_request_recovery(ExamsgHandle mh);
void crash_need_reboot(char *message);

#endif
