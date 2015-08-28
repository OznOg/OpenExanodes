/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "adm_hostname.h"

#include "log/include/log.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_env.h"

#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_network.h"
#include "os/include/strlcpy.h"
#include "os/include/os_string.h"
#include "os/include/os_stdio.h"

#include <stdlib.h>
#include <ctype.h>

#ifndef WIN32
#include <sys/utsname.h>
#endif

/** Default hostname file */
#define HOSTNAME_FILENAME   "hostname"

/** Hostname given by uname() */
static char uname_hostname[EXA_MAXSIZE_HOSTNAME + 1];
static bool uname_set;  /**< uname_hostname set ? */

/** Hostname configured by clcreate() */
static char configured_hostname[EXA_MAXSIZE_HOSTNAME + 1];
static bool configured_set;  /**< configured_hostname set ? */

/**
 * Get the local hostname.
 *
 * Has the side effect of getting the real hostname the first time it is
 * called. If you don't want to call uname() while Exanodes is running,
 * call this function at the very beginning of Admind.
 *
 * \return The configured hostname if the real hostname has been overridden,
 *         the (real) hostname given by uname() otherwise, and NULL if error
 */
const char *
adm_hostname(void)
{
    if (!uname_set)
    {
        if (os_local_host_name(uname_hostname, sizeof(uname_hostname)) != 0)
            return NULL;
	uname_set = true;
    }

  return (configured_set ? configured_hostname : uname_hostname);
}

/**
 * Override the real hostname with the one given.
 *
 * \param[in] hostname  Hostname to use
 */
void
adm_hostname_override(const char *hostname)
{
  exalog_debug("overriding hostname with '%s'", hostname);

  strlcpy(configured_hostname, hostname, sizeof(configured_hostname));
  configured_set = true;

  exalog_set_hostname(adm_hostname());
}

/**
 * Restore the real hostname.
 */
void
adm_hostname_reset(void)
{
  configured_set = false;
  exalog_set_hostname(adm_hostname());

  exalog_debug("removed hostname override");
}

/**
 * Read our hostname from disk.
 *
 * XXX Should probably store the cluster uuid within hostname file
 *     and check this uuid matches the one in the cluster config ?
 *
 * \param[out] hostname  Hostname read
 *
 * \return 0 if successful, a negative error code otherwise (most notably,
 *         -ENOENT if the hostname file does not exist)
 */
int
adm_hostname_load(char *hostname)
{
  char path[OS_PATH_MAX];
  FILE *file;
  char tmp[EXA_MAXSIZE_LINE + 1];
  size_t len;
  int err = 0, err2;

  exalog_trace("loading hostname from %s", path);

  exa_env_make_path(path, sizeof(path), exa_env_cachedir(), HOSTNAME_FILENAME);

  file = fopen(path, "rt");
  if (file == NULL)
    return -errno;

  if (fgets(tmp, sizeof(tmp) - 1, file) == NULL)
    err = -EIO;

  err2 = fclose(file);

  if (err)
    return err;

  if (err2)
    return err2;

  /* Eat up all whitespace at the end of the line */
  len = strlen(tmp);
  while (len > 0 && isspace(tmp[len - 1]))
  {
    tmp[len - 1] = '\0';
    len--;
  }

  if (len == 0 || len > EXA_MAXSIZE_HOSTNAME)
    return -EINVAL;

  strlcpy(hostname, tmp, len + 1);

  exalog_trace("hostname read: '%s'", hostname);

  return 0;
}

/**
 * Write the hostname to disk.
 *
 * \param[in] hostname  Hostname to write
 *
 * \return 0 if successful, a negative error code otherwise
 */
int
adm_hostname_save(const char *hostname)
{
  char path[OS_PATH_MAX];
  FILE *file;
  char tmp[EXA_MAXSIZE_HOSTNAME + 1 + 1]; /* +2 for \n and \0 */
  int err = 0, err2;

  exalog_trace("saving hostname to %s", path);

  exa_env_make_path(path, sizeof(path), exa_env_cachedir(), HOSTNAME_FILENAME);

  if (os_snprintf(tmp, sizeof(tmp), "%s\n", hostname) >= sizeof(tmp))
    return -EINVAL;

  file = fopen(path, "wt");
  if (file == NULL)
    return -errno;

  if (fputs(tmp, file) == EOF)
    err = -EIO;

  err2 = fclose(file);

  if (err)
    return err;

  if (err2)
    return err2;

  return 0;
}

/**
 * Delete the hostname file.
 * Succeeds also if the hostname file doesn't exist.
 *
 * \return 0 if successful, a negative error code otherwise
 */
int
adm_hostname_delete_file(void)
{
  char path[OS_PATH_MAX];

  exa_env_make_path(path, sizeof(path), exa_env_cachedir(), HOSTNAME_FILENAME);

  exalog_trace("deleting %s", path);
  if (unlink(path) < 0 && errno != ENOENT)
    return -errno;

  return 0;
}
