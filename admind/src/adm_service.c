/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/adm_service.h"

#include <string.h>

#include "admind/src/rpc_command.h"
#include "log/include/log.h"

const struct adm_service *const adm_services[ADM_SERVICE_LAST + 1] =
{
    [ADM_SERVICE_ADMIN]     = &adm_service_admin,
    [ADM_SERVICE_RDEV]      = &adm_service_rdev,
    [ADM_SERVICE_NBD]       = &adm_service_nbd,
    [ADM_SERVICE_VRT]       = &adm_service_vrt,
    [ADM_SERVICE_LUM]       = &adm_service_lum,
#ifdef WITH_FS
    [ADM_SERVICE_FS]        = &adm_service_fs,
#endif
#ifdef WITH_MONITORING
    [ADM_SERVICE_MONITOR]   = &adm_service_monitor,
#endif
};

void
adm_service_init_command_processing(void)
{
  const struct adm_service *service;

  adm_service_for_each(service)
  {
    int i;

    /* Register local commands */
    for (i = 0; service->local_commands[i].function; i++)
      rpc_command_set(service->local_commands[i].id,
	              service->local_commands[i].function);
  }
}


const char *adm_service_name(adm_service_id_t id)
{
  EXA_ASSERT_VERBOSE(id >= ADM_SERVICE_FIRST && id <= ADM_SERVICE_LAST,
		     "Unknown service id '%d'", id);
  switch (id)
  {
      case ADM_SERVICE_ADMIN:
	  return "Admin";
      case ADM_SERVICE_RDEV:
	  return "Real device";
      case ADM_SERVICE_NBD:
	  return "Network Block Device";
      case ADM_SERVICE_VRT:
	  return "Virtualizer";
      case ADM_SERVICE_LUM:
	  return "Logical Unit Manager";
#ifdef WITH_FS
      case ADM_SERVICE_FS:
	  return "File system";
#endif
#ifdef WITH_MONITORING
      case ADM_SERVICE_MONITOR:
          return "Monitor";
#endif
  }
  return "Error";
}
