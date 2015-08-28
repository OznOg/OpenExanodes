/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "nbd/service/include/nbdservice_client.h"

#include <string.h>

#include "common/include/daemon_api_client.h"
#include "common/include/exa_error.h"
#include "os/include/strlcpy.h"
#include "examsg/include/examsg.h"
#include "log/include/log.h"
#include "nbd/service/include/nbd_msg.h"


/*
 *  nodes events
 */

int clientd_close_session(ExamsgHandle h, const char *node_name,
			  exa_nodeid_t node_id)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_SESSION_CLOSE;
  strlcpy(req.node_name, node_name, sizeof(req.node_name));
  req.node_id = node_id;

  /*  Send message to clientd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int
clientd_open_session(ExamsgHandle h, const char * node_name,
		     const char *net_id,
		     exa_nodeid_t node_id)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_SESSION_OPEN;
  strlcpy(req.node_name, node_name, sizeof(req.node_name));
  strlcpy(req.net_id, net_id, sizeof(req.net_id));
  req.node_id = node_id;

  /*  Send message to clientd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int
serverd_add_client(ExamsgHandle h, const char *node_name,
		   const char *net_id, exa_nodeid_t remote_node_id)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_ADD_CLIENT;
  strlcpy(req.node_name, node_name, sizeof(req.node_name));
  strlcpy(req.net_id, net_id, sizeof(req.net_id));
  req.node_id = remote_node_id;

  /*  Send message to serverd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_SERVER_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}


int serverd_remove_client(ExamsgHandle h, exa_nodeid_t node_id)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_REMOVE_CLIENT;
  req.node_id = node_id;

  /*  Send message to serverd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_SERVER_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int
serverd_quit(ExamsgHandle h, const char * node_name)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_QUIT;
  strlcpy(req.node_name, node_name, sizeof(req.node_name));

  /*  Send message to serverd */
  ret = admwrk_daemon_query_nointr(h, EXAMSG_NBD_SERVER_ID, EXAMSG_DAEMON_RQST,
				   &req, sizeof(nbd_request_t),
				   &ans, sizeof(nbd_answer_t));
  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int
clientd_quit(ExamsgHandle h, const char * node_name)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_QUIT;
  strlcpy(req.node_name, node_name, sizeof(req.node_name));

  /*  Send message to clientd */
  ret = admwrk_daemon_query_nointr(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
				   &req, sizeof(nbd_request_t),
				   &ans, sizeof(nbd_answer_t));
  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

/*
 *  devices events
 */

int
clientd_device_resume(ExamsgHandle h, const exa_uuid_t *uuid)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_DEVICE_RESUME;
  uuid_copy(&req.device_uuid, uuid);

  /*  Send message to clientd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}


int
serverd_device_export(ExamsgHandle h,const char *device_path,
		      const exa_uuid_t *uuid)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_DEVICE_EXPORT;
  strlcpy(req.device_path, device_path, sizeof(req.device_path));
  uuid_copy(&req.device_uuid, uuid);

  /*  Send message to serverd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_SERVER_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int
serverd_device_unexport(ExamsgHandle h, const exa_uuid_t *uuid)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_DEVICE_UNEXPORT;
  uuid_copy(&req.device_uuid, uuid);

  /*  Send message to serverd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_SERVER_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int
clientd_device_import(ExamsgHandle h, const char *remote_node_name,
                     const exa_uuid_t *uuid, int device_nb,
                     uint64_t device_sectors)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_DEVICE_IMPORT;
  strlcpy(req.node_name, remote_node_name, sizeof(req.node_name));
  uuid_copy(&req.device_uuid, uuid);
  req.device_nb = device_nb;
  req.device_sectors = device_sectors;

  /*  Send message to clientd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int
clientd_device_suspend(ExamsgHandle h, const exa_uuid_t *uuid)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_DEVICE_SUSPEND;
  uuid_copy(&req.device_uuid, uuid);

  /*  Send message to clientd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int
clientd_device_down(ExamsgHandle h, const exa_uuid_t *uuid)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_DEVICE_DOWN;
  uuid_copy(&req.device_uuid, uuid);

  /*  Send message to clientd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int clientd_device_add(ExamsgHandle h, const char *node_name,
		       const exa_uuid_t *uuid, exa_nodeid_t node_id)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_DEVICE_ADD;
  strlcpy(req.node_name, node_name, sizeof(req.node_name));
  uuid_copy(&req.device_uuid, uuid);
  req.node_id = node_id;

  /*  Send message to clientd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

int
clientd_device_remove(ExamsgHandle h, const exa_uuid_t *uuid)
{
  nbd_request_t req;
  nbd_answer_t ans;
  int ret;

  req.event   = NBDCMD_DEVICE_REMOVE;
  uuid_copy(&req.device_uuid, uuid);

  /*  Send message to clientd */
  ret = admwrk_daemon_query(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(nbd_request_t),
			    &ans, sizeof(nbd_answer_t));

  if (ret != EXA_SUCCESS)
    return ret;

  if (ans.status != EXA_SUCCESS)
    return ans.status;

  return EXA_SUCCESS;
}

/**
 * get size about a device handled by serverd
 * @param[in] h message handle
 * @param[in] uuid UUID of the device to get the size
 * @param[out] device_size pointer to where to store the size
 *             (size is in kilobytes)
 * @return EXA_SUCCESS if successfull, error otherwise
 */
int
serverd_get_device_size(ExamsgHandle h, const exa_uuid_t *uuid,
		 uint64_t *device_size)
{
  nbd_request_t req;
  exported_device_info_t ans;
  int retval;

  exalog_debug("client asked to send a get ndev major/minor  to nbd");

  req.event = NBDCMD_NDEV_INFO;
  uuid_copy(&req.device_uuid, uuid);

  retval = admwrk_daemon_query_nointr(h, EXAMSG_NBD_SERVER_ID,
				      EXAMSG_DAEMON_RQST,
				      &req, sizeof(req),
				      &ans, sizeof(ans));

  if (retval != EXA_SUCCESS)
    return retval;

  /* Disk not found */
  if(ans.status != EXA_SUCCESS)
    return ans.status;

  *device_size = ans.device_sectors / 2; /* exported_device_info_t has size in sectors */

  return EXA_SUCCESS;
}


int
serverd_device_get_info(ExamsgHandle h, const exa_uuid_t *uuid,
			exported_device_info_t *device_info)
{
  nbd_request_t req;
  int retval;

  req.event = NBDCMD_NDEV_INFO;
  uuid_copy(&req.device_uuid, uuid);

  retval = admwrk_daemon_query_nointr(h, EXAMSG_NBD_SERVER_ID,
				      EXAMSG_DAEMON_RQST,
				      &req, sizeof(nbd_request_t),
				      device_info,
				      sizeof(exported_device_info_t));

  if (retval != EXA_SUCCESS)
    return retval;

  // Disk not found
  if(device_info->status != EXA_SUCCESS)
    return device_info->status;

  return EXA_SUCCESS;
}


int
clientd_stat_get(ExamsgHandle h, const struct nbd_stats_request *request,
		 struct nbd_stats_reply *reply)
{
  nbd_request_t req;

  req.event = NBDCMD_STATS;
  /* FIXME the only field usefull seems to be reset... */
  req.stats_reset = request->reset;
  strlcpy(req.node_name, request->node_name, sizeof(req.node_name));
  strlcpy(req.device_path, request->disk_path, sizeof(req.device_path));
  uuid_copy(&req.device_uuid, &request->device_uuid);

  return admwrk_daemon_query_nointr(h, EXAMSG_NBD_CLIENT_ID, EXAMSG_DAEMON_RQST,
				    &req, sizeof(req),
				    reply, sizeof(*reply));
}

