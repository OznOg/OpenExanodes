/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef H_LOGD
#define H_LOGD

/** \file
 * \brief Logging daemon.
 *
 * Logging routines implemented in exa_msgd daemon
 * \sa logd.c
 */

#include "log/include/log.h"

#define EXALOG_SHM_SIZE (sizeof(exalog_level_t) * (EXAMSG_LAST_ID + 1))
#define EXALOG_SHM_ID   "exanodes-log"

/* Default loglevel */
#if defined(WITH_TRACE)
#define EXALOG_DEFAULT_LEVEL  EXALOG_LEVEL_TRACE
#elif defined(DEBUG)
#define EXALOG_DEFAULT_LEVEL  EXALOG_LEVEL_DEBUG
#else
#define EXALOG_DEFAULT_LEVEL  EXALOG_LEVEL_INFO
#endif

int log_init(const char *logfile_name);

/* here because of unit test */
void exalog_loop(void *_loglevels_shm);

#endif /* H_LOGD */
