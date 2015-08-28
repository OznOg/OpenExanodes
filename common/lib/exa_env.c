/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_env.h"
#include "common/include/exa_error.h"

#include "os/include/os_file.h"
#include "os/include/os_error.h"
#include "os/include/os_stdio.h"
#include "os/include/strlcpy.h"
#ifdef WIN32
#include "os/include/os_windows.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* XXX TODO Refactor all exa_env_xxxdir() functions. */

static char *ensure_slash_ended(char *path, size_t size)
{
    size_t len = strlen(path);

    /* Leave the path unchanged if it's empty */
    if (len == 0)
        return path;

    if (len < size - 1 && path[len - 1] != *OS_FILE_SEP)
    {
        path[len++] = *OS_FILE_SEP;
        path[len] = '\0';
    }

    return path;
}

const char *exa_env_bindir(void)
{
    static char dir[OS_PATH_MAX];
    char *var = getenv(EXA_ENV_BIN_DIR);

    if (var == NULL)
        return NULL;

    strlcpy(dir, var, sizeof(dir));
    return ensure_slash_ended(dir, sizeof(dir));
}

const char *exa_env_sbindir(void)
{
    static char dir[OS_PATH_MAX];
    char *var = getenv(EXA_ENV_SBIN_DIR);

    if (var == NULL)
        return NULL;

    strlcpy(dir, var, sizeof(dir));
    return ensure_slash_ended(dir, sizeof(dir));
}

const char *exa_env_libdir(void)
{
    static char dir[OS_PATH_MAX];
    char *var = getenv(EXA_ENV_LIB_DIR);

    if (var == NULL)
        return NULL;

    strlcpy(dir, var, sizeof(dir));
    return ensure_slash_ended(dir, sizeof(dir));
}

const char *exa_env_piddir(void)
{
    static char dir[OS_PATH_MAX];
    char *var = getenv(EXA_ENV_PID_DIR);

    if (var == NULL)
        return NULL;

    strlcpy(dir, var, sizeof(dir));
    return ensure_slash_ended(dir, sizeof(dir));
}

const char *exa_env_nodeconfdir(void)
{
    static char dir[OS_PATH_MAX];
    char *var = getenv(EXA_ENV_NODE_CONF_DIR);

    if (var == NULL)
        return NULL;

    strlcpy(dir, var, sizeof(dir));
    return ensure_slash_ended(dir, sizeof(dir));
}

const char *exa_env_cachedir(void)
{
    static char dir[OS_PATH_MAX];
    char *var = getenv(EXA_ENV_CACHE_DIR);

    if (var == NULL)
        return NULL;

    strlcpy(dir, var, sizeof(dir));
    return ensure_slash_ended(dir, sizeof(dir));
}

const char *exa_env_datadir(void)
{
    static char dir[OS_PATH_MAX];
    char *var = getenv(EXA_ENV_DATA_DIR);

    if (var == NULL)
        return NULL;

    strlcpy(dir, var, sizeof(dir));
    return ensure_slash_ended(dir, sizeof(dir));
}

const char *exa_env_logdir(void)
{
    static char dir[OS_PATH_MAX];
    char *var = getenv(EXA_ENV_LOG_DIR);

    if (var == NULL)
        return NULL;

    strlcpy(dir, var, sizeof(dir));
    return ensure_slash_ended(dir, sizeof(dir));
}

#ifdef WITH_PERF
const char *exa_env_perf_config(void)
{
    static char file[OS_PATH_MAX];
    char *var = getenv(EXA_ENV_PERF_CONFIG);

    if (var == NULL)
        return NULL;

    strlcpy(file, var, sizeof(file));
    return file;
}
#endif

static bool env_ok(const char *(*env_fn)(void))
{
    const char *e = env_fn();
    return e != NULL && e[0] != '\0';
}

bool exa_env_properly_set(void)
{
    return env_ok(exa_env_bindir)
        && env_ok(exa_env_sbindir)
        && env_ok(exa_env_libdir)
        && env_ok(exa_env_piddir)
        && env_ok(exa_env_nodeconfdir)
        && env_ok(exa_env_cachedir)
        && env_ok(exa_env_datadir)
        && env_ok(exa_env_logdir);
}

int exa_env_make_path(char *buf, size_t size, const char *dir, const char *fmt, ...)
{
    char __tail[OS_PATH_MAX];
    char *tail = __tail;
    va_list ap;
    size_t ret;
    char *sep;

    if (buf == NULL || size == 0 || dir == NULL)
        return -EINVAL;

    if (fmt == NULL)
    {
        tail[0] = '\0';
    }
    else
    {
        va_start(ap, fmt);
        ret = os_vsnprintf(__tail, sizeof(__tail), fmt, ap);
        va_end(ap);

        if (ret >= sizeof(__tail))
            return -ENAMETOOLONG;
    }

    /* Add/remove separators if needed */

    if (dir[strlen(dir) - 1] == *OS_FILE_SEP || tail[0] == *OS_FILE_SEP)
        sep = "";
    else
        sep = OS_FILE_SEP;

    if (dir[strlen(dir) - 1] == *OS_FILE_SEP && tail[0] == *OS_FILE_SEP)
        tail++;

    ret = os_snprintf(buf, size, "%s%s%s", dir, sep, tail);
    if (ret >= size)
        return -ENAMETOOLONG;

    return 0;
}
