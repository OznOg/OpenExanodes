/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_NBDSERVICE_CLIENT
#define H_NBDSERVICE_CLIENT

#include "nbd/clientd/src/nbd_stats.h"

#include "common/include/exa_constants.h"
#include "examsg/include/examsg.h"


/** This structure is exchanged between two nodes before the import phase, it must be aligned **/
struct exported_device_info
{
  uint64_t device_sectors;
  int device_nb;
  int status;
};

typedef struct exported_device_info exported_device_info_t;

/* --- Client API ------------------------------------------------- */

int clientd_open_session(ExamsgHandle h, const char *node_name,
			 const char *net_id, exa_nodeid_t node_id);

int clientd_close_session(ExamsgHandle h, const char *node_name,
			  exa_nodeid_t node_id);

int clientd_quit(ExamsgHandle h, const char *node_name);

int serverd_quit(ExamsgHandle h, const char *node_name);

int serverd_add_client(ExamsgHandle h, const char *node_name,
		       const char *net_id, exa_nodeid_t remote_node_id);

int serverd_remove_client(ExamsgHandle h, exa_nodeid_t node_id);

int serverd_device_export(ExamsgHandle h, const char *device_path,
			  const exa_uuid_t *uuid);

int serverd_device_unexport(ExamsgHandle h, const exa_uuid_t *uuid);

int clientd_device_import(ExamsgHandle h, exa_nodeid_t node_id,
                         const exa_uuid_t *uuid, int device_nb,
                         uint64_t device_sectors);

int clientd_device_suspend(ExamsgHandle h, const exa_uuid_t *uuid);

int clientd_device_down(ExamsgHandle h, const exa_uuid_t *uuid);

int clientd_device_resume(ExamsgHandle h, const exa_uuid_t *uuid);

int clientd_device_remove(ExamsgHandle h, const exa_uuid_t *uuid);

int serverd_get_device_size(ExamsgHandle h, const exa_uuid_t *uuid,
		     uint64_t *device_size);

int serverd_device_get_info(ExamsgHandle h, const exa_uuid_t *uuid,
			    exported_device_info_t *device_info);

int clientd_stat_get(ExamsgHandle h, const struct nbd_stats_request *request,
		     struct nbd_stats_reply *reply);

#endif
