/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "os/include/os_network.h"
#include <string.h>

#include "admind/src/service_parameter.h"
#include "admind/src/adm_nodeset.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "common/include/exa_config.h"
#include "common/include/exa_nodeset.h"
#include "os/include/strlcpy.h"

#ifdef WITH_FS
#include "admind/services/fs/service_fs.h"
#endif


/*!\file
 * \brief Implementation of a common API to access variable parameters
 * from all server side services and from the client.
 *
 * Server side only READ parameters, the CLI/GUI can list and set them.
 *
 * Once set on the client side, they will be transmitted to admind via
 * the XML admind protocol.
 */


/*
 * Global  table
 */

static exa_service_parameter_t exa_service_parameter_table[] = {
#ifdef WITH_FS
  /* Service FS */
  {
    .name            = EXA_OPTION_FS_GULM_MASTERS,
    .description     = "Selection of the Gulm masters.",
    .type            = EXA_PARAM_TYPE_NODELIST,
    .default_value   = "",
  },
  {
    .name            = EXA_OPTION_FS_GFS_LOCK_PROTOCOL,
    .description     = "Selection of the Seanodes FS lock manager/protocol.",
    .type            = EXA_PARAM_TYPE_LIST,
    .choices         =
    {
      GFS_LOCK_GULM,
      GFS_LOCK_DLM,
      NULL
    },
    .default_value   = GFS_LOCK_GULM,
  },
  {
    .name            = "default_demote_secs",
    .description     = "Default delay (seconds) before demoting SFS write locks into less\n"
                       "restricted states and subsequently flush the cache data into disk.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 0,
    .max             = EXA_FSTUNE_DEMOTE_SECS_MAX,
    .default_value   = "150",
  },
  {
    .name            = "default_glock_purge",
    .description     = "Default percentage of unused SFS locks to trim every 5 seconds.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 0,
    .max             = 100,
    .default_value   = "50",
  },
#endif
  /* Service NBD */
  {
    .name            = "server_buffers",
    .description     = "The number of buffers used by the server to handle the clients' requests\n"
                       "(store the data to be written to disks in the write requests, or the data\n"
                       "read from the disks in read requests).",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 1,
    .max             = 300,
    .default_value   = "300",
  },
  {
    .name            = "max_request_size",
    .description     = "The maximum size of requests exchanged between the NBD clients and servers.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 4096,
    .max             = 1048576,
    .default_value   = "131072",
  },
  {
    .name            = "max_client_requests",
    .description     = "The maximum number of requests handled in parallel by the NBD client.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 1,
    .max             = 750,
    .default_value   = "750",
  },
  /********************** Deprecated ****************************************/
  {
    /* FIXME this is deprecated and deserve to be removed. It is kept only
     * for backward compatibility purpose in 5.0.0 as service_parameter does
     * not handle suppression of parameters for now. */
#define NBD_SCHED_FIFO                "fifo"
#define NBD_SCHED_INTERLEAVE          "interleave"
#define NBD_SCHED_BALANCED_INTERLEAVE "balanced_interleave"
    .name            = "scheduler",
    .description     = "Deprecated.",
    .type            = EXA_PARAM_TYPE_LIST,
    .min             = 0,
    .max             = 0,
    .choices         =
    {
      NBD_SCHED_FIFO,
      NBD_SCHED_INTERLEAVE,
      NBD_SCHED_BALANCED_INTERLEAVE,
      NULL
    },
    .default_value   = NBD_SCHED_FIFO,
  },
  /**************************************************************************/
  {
    .name            = "tcp_client_buffer_size",
    .description     = "The maximum size in bytes of the buffers (send buffers) used by the sockets to transfer data.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 4096,
    .max             = 262144,
    .default_value   = "131072",
  },
  {
    .name            = "tcp_server_buffer_size",
    .description     = "The maximum size in bytes of the buffers (send buffers) used by the sockets to transfer data.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 4096,
    .max             = 262144,
    .default_value   = "131072",
  },
  {
    .name            = "tcp_data_net_timeout",
    .description     = "The maximum wait time (in seconds) without receiving keepalives from clients before TCP data \n"
		       "network on server is declared down. 0 means data network failure detection is deactivated.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 0,
    .max             = 43200,
    .default_value   = "0",
  },
  {
    .name            = "io_barriers",
    .description     = "IO barriers prevent metadata corruption when electrically powering off a node.",
    .type            = EXA_PARAM_TYPE_BOOLEAN,
    .default_value   = "FALSE",
  },
  /* Service Csupd */
  {
    .name            = "heartbeat_period",
    .description     = "Time in seconds between each heartbeat message sent. A small time increases network overhead.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 1,
    .max             = 30,
    .default_value   = "1",
  },
  {
    .name            = "alive_timeout",
    .description     = "The number of seconds without receiving heartbeat messages before\n"
	"considering that a node is down. A small value reduces response time, but can lead\n"
	"to false node down detection.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 1,
    .max             = 30,
    .default_value   = "5",
  },
  /* Clusterization */
  {
      .name          = "token_manager_address",
      .description   = "Address of the Seanodes Token Manager to use in a 2-node cluster\n"
                       "to ensure resilience to a node crash.",
      .type          = EXA_PARAM_TYPE_TEXT,
      .default_value = ""
  },
  /* Service VRT */
  {
    .name            = "max_requests",
    .description     = "The maximum number of requests that can be handled by the\n"
                       "virtualizer engine at the same time. An higher value increases I/O parallelism\n"
                       "but increases memory usage.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 1,
    .max             = 65535,
    .default_value   = "4096",
  },
  {
    .name            = "default_chunk_size",
    .description     = "Default chunk size in KiB. A small chunk size increases the memory consumption,\n"
                       "whereas a large chunk size can lead to space loss when creating small volumes.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = VRT_MIN_CHUNK_SIZE,
    .max             = -1,
    .default_value   = "262144",
  },
  {
    .name            = "default_su_size",
    .description     = "Default size ot the striping unit in KiB at group creation.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 64,
    .max             = -1,
    .default_value   = "1024",
  },
  {
    .name            = "default_dirty_zone_size",
    .description     = "Default dirty zone size in KiB at group creation.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = VRT_MIN_DIRTY_ZONE_SIZE,
    .max             = -1,
    .default_value   = "32768",
  },
  {
    .name            = "default_readahead",
    .description     = "Default read ahead in KiB at filesystem and volume creation.\n"
    "Large readahead size may increase the performance of large sequential reads.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 0,
    .max             = -1,
    .default_value   = "8192",
  },
  {
    .name            = "target_queue_depth",
    .description     = "The size of the iSCSI target queue depth",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 1,
    .max             = 64,
    .default_value   = "64",
  },
  {
    .name            = "bdev_target_queue_depth",
    .description     = "The size of the bdev target queue depth",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 1,
    .max             = 256,
    .default_value   = "256",
  },
  {
    .name            = "target_buffer_size",
    .description     = "iSCSI target buffers size",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 262144,
    .max             = 1048576,
    .default_value   = "262144",
  },
  /* Service RDEV */
  {
    .name            = "disk_patterns",
    .description     = "Shell patterns of block devices to scan to find Exanodes disks (separated by spaces).\n"
                       "For example '/dev/sd* /dev/hd*' for all SCSI and IDE disks.",
    .type            = EXA_PARAM_TYPE_TEXT,
#ifdef WIN32
    .default_value   = "\\\\?\\Volume*",
#else
    .default_value   = "/dev/sd* /dev/hd* /dev/xvd* /dev/mapper/* /dev/cciss/*",
#endif
  },
  {
    .name            = "multicast_address",
    .description     = "IP multicast address used to communicated between the nodes.",
    .type            = EXA_PARAM_TYPE_IPADDRESS,
    .default_value   = "229.230.231.232",
  },
  {
    .name            = "multicast_port",
    .description     = "UDP port used to communicated between the nodes.",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 0,
    .max             = 65535,
    .default_value   = "30798",
  },
  /* FIXME These should be moved to the VRT section further up */
  {
    .name            = "rebuilding_slowdown",
    .description     = "Rebuilding slowdown on a not DEGRADED group. Allowed slowdown values range between 0 (maximum priority to rebuilding) to 4 (maximum priority to client IO)",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 0,
    .max             = 4,
    .default_value   = "0", /* 0ms */
  },
  {
    .name            = "degraded_rebuilding_slowdown",
    .description     = "Rebuilding slowdown on a DEGRADED group. Allowed slowdown values range between 0 (maximum priority to rebuilding) to 4 (maximum priority to client IO)",
    .type            = EXA_PARAM_TYPE_INT,
    .min             = 0,
    .max             = 4,
    .default_value   = "0", /* 0ms */
  }
};

static int exa_service_params_number = \
  (sizeof (exa_service_parameter_table) / sizeof ((exa_service_parameter_table)[0]));

/** Check a parameter value is valid based on its internal definition
 *
 * \param service_param: the service param to check
 * \param value: the value to set the parameter to
 *
 * \return EXA_ERR_INVALID_VALUE if the parameter is out of bounds
 *         EXA_SUCCESS if the parameter value is valid.
 */
int
exa_service_parameter_check(exa_service_parameter_t *service_param,
                            const char *value)
  {
    EXA_ASSERT_VERBOSE(value, "Cannot check parameter for a NULL value");

    /* Check the given value is valid */
    switch(service_param->type)
      {
      case EXA_PARAM_TYPE_INT:
	{
	  char *endptr;
	  int num = strtol(value, &endptr, 0);

	if(*endptr)
	  return -EXA_ERR_INVALID_VALUE;

	if(num < service_param->min ||
	   ((service_param->max != -1) && (num > service_param->max)))
	  return -EXA_ERR_INVALID_VALUE;
      }
      break;
    case EXA_PARAM_TYPE_BOOLEAN:
      {
	if(strncmp(value, "TRUE", 4) &&
	   strncmp(value, "FALSE", 5))
	  return -EXA_ERR_INVALID_VALUE;
      }
      break;
    case EXA_PARAM_TYPE_TEXT:
      break;
    case EXA_PARAM_TYPE_LIST:
      {
	int i = 0;

	while(service_param->choices[i++])
	  if(strcmp(value, service_param->choices[i-1]) == 0)
	    break;

	if(service_param->choices[i-1] == NULL)
	  return -EXA_ERR_INVALID_VALUE;
      }
      break;
    case EXA_PARAM_TYPE_NODELIST:
      {
          exa_nodeset_t nodeset;
          return adm_nodeset_from_names(&nodeset, value);
      }
    case EXA_PARAM_TYPE_IPADDRESS:
      {
        struct in_addr inp;
        if (os_inet_aton(value, &inp) == 0)
          return -EXA_ERR_INVALID_VALUE;
      }
      break;
    }
    return EXA_SUCCESS;
  }


/** return the exa_service_parameter_t for the param_name
 *
 * \return the exa_service_parameter_t or NULL if not found
 */
exa_service_parameter_t *
exa_service_parameter_get(const char *param_name)
{
  int i;

  if (param_name == NULL)
      return NULL;

  for(i=0; i<exa_service_params_number; i++)
      if (strncmp(exa_service_parameter_table[i].name, param_name,
                  EXA_MAXSIZE_PARAM_NAME) == 0)
	  return &exa_service_parameter_table[i];

  return NULL;
}

/** Let a service get the default value for a parameter
 *
 * \param service: the service for the parameter
 * \param param_name: the parameter to get
 *
 * \return the value
 */
const char *
exa_service_parameter_get_default(const char *param_name)
{
  exa_service_parameter_t *service_param;

  service_param = exa_service_parameter_get(param_name);

  EXA_ASSERT_VERBOSE(service_param, "Service parameter '%s' not found",
		     param_name);

  return service_param->default_value;
}

/** Get the list of possible parameters for a service, any category
 *
 * \param iterator: an int provided by you and used to store the index in
 *                   the iteration. It must be inited by you to 0 when first
 *                   calling exa_service_parameter_get_list()
 *
 * \return a single service_parameter, call us again with the same iterator
 *         to get the next one. NULL is returned when there are no more params.
 *
 * \warning always set your iterator to 0 before calling this function for the first
 *          time.
 */
exa_service_parameter_t *
exa_service_parameter_get_list(int *iterator)
{
  if ((*iterator)++ < exa_service_params_number)
    {
      return &exa_service_parameter_table[*iterator-1];
    }
  return NULL;
}
