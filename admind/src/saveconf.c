/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/saveconf.h"

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_license.h"
#include "admind/src/adm_serialize.h"
#include "admind/src/adm_atomic_file.h"
#include "common/include/exa_env.h"
#include "common/include/exa_error.h"
#include "os/include/os_mem.h"
#include "log/include/log.h"
#include "os/include/os_file.h"
#include "os/include/os_error.h"


void
conf_delete(void)
{
    char conf_path[OS_PATH_MAX];
    char license_path[OS_PATH_MAX];

    exa_env_make_path(conf_path, sizeof(conf_path), exa_env_cachedir(),
                      ADMIND_CONF_EXANODES_FILE);
    exa_env_make_path(license_path, sizeof(license_path), exa_env_cachedir(),
                      ADM_LICENSE_FILE);

    adm_cluster_cleanup();

    /* The only acceptable errors are if the files were already removed. */
    EXA_ASSERT(unlink(conf_path) == 0 || errno == ENOENT);
    EXA_ASSERT(unlink(license_path) == 0 || errno == ENOENT);
}


static int
conf_do_save(int64_t version)
{
  char *buffer;
  int size;
  int ret;
  char conf_path[OS_PATH_MAX];

  exa_env_make_path(conf_path, sizeof(conf_path), exa_env_cachedir(),
                    ADMIND_CONF_EXANODES_FILE);

  /* Update version number */

  adm_cluster.version = version;

  do
  {
    /* Compute buffer size */

    adm_cluster_lock();
    size = adm_serialize_to_null(false /*create */);
    adm_cluster_unlock();

    /* Alloc a buffer */

    buffer = os_malloc(size + 1);
    if (buffer == NULL)
    {
      exalog_error("os_malloc() failed");
      return -ENOMEM;
    }

    /* Serialize the config */

    adm_cluster_lock();
    ret = adm_serialize_to_memory(buffer, size + 1, false /*create */);
    adm_cluster_unlock();
    if (ret < EXA_SUCCESS)
    {
      exalog_error("adm_serialize_to_memory(): %s", exa_error_msg(ret));
      goto error;
    }
  } while(ret != size);

  ret = adm_atomic_file_save(conf_path, buffer, size);
  if (ret)
  {
      exalog_error("failed saving cluster config file");
      goto error;
  }

  adm_cluster_lock();
  if (adm_config_buffer)
    os_free(adm_config_buffer);
  adm_config_buffer = buffer;
  adm_config_size = size;
  adm_cluster_unlock();

  exalog_debug("version %" PRId64 " successfully written",
	       version);
  return EXA_SUCCESS;

error:
  os_free(buffer);
  return ret;
}


int
conf_save_synchronous(void)
{
  return conf_do_save(adm_cluster.version + 1);
}


int
conf_save_synchronous_without_inc_version(void)
{
  return conf_do_save(adm_cluster.version);
}

void conf_cleanup()
{
    if (adm_config_buffer)
        os_free(adm_config_buffer);
}
