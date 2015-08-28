/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _EXA_ENV_H
#define _EXA_ENV_H

#include "os/include/os_inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exanodes environment variables */
#define EXA_ENV_BIN_DIR        "EXANODES_BIN_DIR"
#define EXA_ENV_SBIN_DIR       "EXANODES_SBIN_DIR"
#define EXA_ENV_LIB_DIR        "EXANODES_LIB_DIR"
#define EXA_ENV_PID_DIR        "EXANODES_PID_DIR"
#define EXA_ENV_NODE_CONF_DIR  "EXANODES_NODE_CONF_DIR"
#define EXA_ENV_CACHE_DIR      "EXANODES_CACHE_DIR"
#define EXA_ENV_DATA_DIR       "EXANODES_DATA_DIR"
#define EXA_ENV_LOG_DIR        "EXANODES_LOG_DIR"
#ifdef WITH_PERF
#define EXA_ENV_PERF_CONFIG    "EXANODES_PERF_CONFIG"
#endif
/* const char *exa_env_installdir(void); */
const char *exa_env_bindir(void);
const char *exa_env_sbindir(void);
const char *exa_env_libdir(void);
const char *exa_env_piddir(void);
const char *exa_env_nodeconfdir(void);
const char *exa_env_cachedir(void);
const char *exa_env_datadir(void);
const char *exa_env_logdir(void);
#ifdef WITH_PERF
const char *exa_env_perf_config(void);
#endif

/**
 * Check that the environment is properly set, i.e. that all variables
 * defined above exist and are non-empty.
 */
bool exa_env_properly_set(void);

/**
 * Make a path.
 *
 * NOTE: Handles duplicate path separators fine between 'dir' and the result
 * of the formatted arguments, but does not do so between arguments.
 *
 * @param[out] buf   Buffer to hold the resulting path
 * @param[in]  size  Buffer size
 * @param[in]  dir   Prefix directory
 * @param[in]  fmt   Format, may be NULL
 * @param[in]  ...   Optional arguments to be formatted
 *
 * @return 0 if successful, -EINVAL if a parameter is invalid
 *         and -ENAMETOOLONG if the buffer is too small
 */
int exa_env_make_path(char *buf, size_t size, const char *dir, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* _EXA_ENV_H */
