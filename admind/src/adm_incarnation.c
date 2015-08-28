/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "adm_incarnation.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_env.h"
#include "common/include/exa_error.h"
#include "log/include/log.h"
#include "os/include/os_file.h"
#include "os/include/os_random.h"
#include "os/include/os_stdio.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/** Current incarnation */
unsigned short incarnation;

/**
 * Save incarnation to incarnation file.
 *
 * \param[in] inca Incarnation to write
 *
 * \return 0 if successfull, negative error code otherwise
 */
static int
adm_save_incarnation(unsigned short inca)
{
  FILE *f;
  char tmp[16];
  int err = 0, err2;
  char incarnation_path[OS_PATH_MAX];
  char incarnation_path_new[OS_PATH_MAX];

  exa_env_make_path(incarnation_path, sizeof(incarnation_path),
                    exa_env_cachedir(), "incarnation");

  exa_env_make_path(incarnation_path_new, sizeof(incarnation_path_new),
                    exa_env_cachedir(), "incarnation.new");

  if ((f = fopen(incarnation_path_new, "wt")) == NULL)
    return -errno;

  if (os_snprintf(tmp, sizeof(tmp), "%hu\n", inca) >= sizeof(tmp))
    err = -EINVAL;
  else
    if (fputs(tmp, f) == EOF)
      err = -EIO;

  err2 = fclose(f);

  if (err || err2)
    return err ? err : err2;

  return os_file_rename(incarnation_path_new, incarnation_path);
}

/**
 * Load incarnation from incarnation file.
 *
 * \param[out] inca Loaded incarnation
 *
 * \return 0 if successfull, negative error code otherwise
 */
static int
adm_load_incarnation(unsigned short *inca)
{
  FILE *f;
  char tmp[16];
  int err = 0;
  char incarnation_path[OS_PATH_MAX];

  exa_env_make_path(incarnation_path, sizeof(incarnation_path),
                    exa_env_cachedir(), "incarnation");

  if ((f = fopen(incarnation_path, "rt")) == NULL)
    {
      /* When no incarnation file is found, we just take a random value as the
       * new incarnation. Taking a random value is a way to change incarnation
       * when the whole machine is being reinstalled. See bug #3142 for the
       * detailed use case.*/
      /* FIXME This makes the bug less frequent but is not a perfect solution
       *  as there may be cases where the random incarnation picked here is the
       *  same as the incarnation that was stored on the other machines */

      os_get_random_bytes(inca, sizeof(*inca));

      return EXA_SUCCESS;
    }

  if (fgets(tmp, sizeof(tmp), f) == NULL)
    err = -EIO;
  else
    if (sscanf(tmp, "%hu", inca) != 1)
      err = -EINVAL;

  fclose(f);

  return err;
}

/**
 * Set the current incarnation.
 *
 * The incarnation is read from the incarnation file, incremented and
 * written back to that file.
 *
 * \return 0 if successfull, negative error code otherwise
 */
int
adm_set_incarnation(void)
{
  int err;

  err = adm_load_incarnation(&incarnation);
  if (err < 0)
    {
      exalog_error("Failed loading old incarnation");
      return err;
    }

  incarnation++;
  if (incarnation == 0)
    incarnation++;

  err = adm_save_incarnation(incarnation);
  if (err < 0)
    {
      exalog_error("Failed saving new incarnation");
      return err;
    }

  exalog_debug("Incarnation: %hu", incarnation);

  return 0;
}

/*
 * Delete the incarnation file.
 *
 * \return 0 if successfull, negative error code otherwise
 */
int
adm_delete_incarnation(void)
{
  char incarnation_path[OS_PATH_MAX];

  exa_env_make_path(incarnation_path, sizeof(incarnation_path),
                    exa_env_cachedir(), "incarnation");

  if (unlink(incarnation_path) != 0)
      return -errno;

  return 0;
}
