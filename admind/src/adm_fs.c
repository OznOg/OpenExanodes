/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/adm_fs.h"

#include <string.h>
#include <errno.h>

#include "common/include/exa_error.h"
#include "os/include/os_mem.h"


struct adm_fs *
adm_fs_alloc(void)
{
  struct adm_fs *fs;

  fs = os_malloc(sizeof(struct adm_fs));
  if (fs == NULL)
    return NULL;

  memset(fs, 0, sizeof(struct adm_fs));

  return fs;
}


void
__adm_fs_free(struct adm_fs *fs)
{
  os_free(fs);
}
