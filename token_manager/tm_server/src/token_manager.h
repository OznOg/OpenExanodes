/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __TOKEN_MANAGER_H__
#define __TOKEN_MANAGER_H__

#include "os/include/os_inttypes.h"

/* Environment variables */
#define TOKEN_MANAGER_FILE_ENV_VAR     "TOKEN_MANAGER_FILE"
#define TOKEN_MANAGER_PORT_ENV_VAR     "TOKEN_MANAGER_PORT"
#define TOKEN_MANAGER_PRIVPORT_ENV_VAR "TOKEN_MANAGER_PRIVPORT"
#define TOKEN_MANAGER_LOGFILE_ENV_VAR  "TOKEN_MANAGER_LOGFILE"
#define TOKEN_MANAGER_DEBUG            "TOKEN_MANAGER_DEBUG"

/** Default base name of the token file */
#define TOKEN_MANAGER_DEFAULT_TOKEN_FILE  "tokens"

/** Settings for and result of a Token Manager */
typedef struct
{
    const char *file;     /**< Token file */
    const char *logfile;  /**< Log file */
    uint16_t port;        /**< Port for clients */
    uint16_t priv_port;   /**< Privileged port (for administration) */
    bool debug;           /**< Whether debugging is enabled */
    int result;           /**< Result (exit code) of the Token Manager */
} token_manager_data_t;

/**
 * Token Manager thread.
 *
 * @param[in,out] data  Token manager data, of type token_manager_data_t,
 *                      containing the settings as input, and the thread's
 *                      result (exit code) as output.
 */
void token_manager_thread(void *data);

/**
 * Tell the Token Manager thread to stop.
 *
 * Note that this is asynchronous, thus the Token Manager thread may not
 * actually be stopped upon return.
 */
void token_manager_thread_stop(void);

/**
 * Tell the Token Manager to reopen its log file.
 */
void token_manager_reopen_log(void);

#endif /* __TOKEN_MANAGER_H__ */
