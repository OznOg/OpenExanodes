/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <string.h>
#include <errno.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_deserialize.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_license.h"
#include "admind/src/admindstate.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr_mship.h"
#include "common/include/exa_names.h"
#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "os/include/os_dir.h"
#include "os/include/os_thread.h"

#include "examsgd/examsgd_client.h"

#define ADM_CONFIG_RECOVER_BUF_LEN 2047


struct config_version_info
{
  int64_t version;
  int confsize;
};


/* The list of nodes known by examsg. */
static exa_nodeset_t examsg_nodeset;


static int
admin_init(int thr_nb)
{
  struct adm_node *node;

  exalog_debug("admind init");

  /* save the cluster goal to 'started'. This is mandatory to be able to
   * restart automatically a cluster that crashed.
   * XXX This makes the start *always* persistent. Why not allow a
   * non-persistent start? */
  if (adm_cluster.persistent_goal != ADM_CLUSTER_GOAL_STARTED)
  {
    int error_val;

    adm_cluster.persistent_goal = ADM_CLUSTER_GOAL_STARTED;
    error_val = adm_cluster_save_goal(adm_cluster.persistent_goal);
    if (error_val)
    {
      adm_cluster.persistent_goal = ADM_CLUSTER_GOAL_STOPPED;
      return error_val;
    }
  }

  adm_cluster.goal = adm_cluster.persistent_goal;

  /* Some cleanup */
  os_dir_remove("/dev/" DEV_ROOT_NAME);

  exa_nodeset_reset(&examsg_nodeset);

  /* declare all nodes to exa_msgd in order to be able to receive messages
   * from theses nodes */
  adm_cluster_for_each_node(node)
  {
    int ret;

    ret = examsgAddNode(adm_wt_get_localmb(), node->id, node->name);
    EXA_ASSERT_VERBOSE(ret != -EEXIST, "Node %u:`%s' already known by exa_msgd",
		       node->id, node->name);
    if (ret != EXA_SUCCESS)
    {
      exalog_error("exa_msgd failed to add node %u:%s. ret=%d",
		   node->id, node->name, ret);
      return ret;
    }
    exa_nodeset_add(&examsg_nodeset, node->id);
  }

  /* Log cltunes parameters that differs from default value hardcoded in
     service_parameter.c. */
  adm_cluster_log_tuned_params();

  return EXA_SUCCESS;
}

static void
admin_verify_license_unicity(int thr_nb)
{
  exa_nodeid_t nodeid;
  admwrk_request_t handle;
  int err;
  const exa_uuid_t *license_uuid = adm_license_get_uuid(exanodes_license);

  admwrk_run_command(thr_nb, &adm_service_admin, &handle,
		     RPC_SERVICE_ADMIND_CHECK_LICENSE, license_uuid,
		     sizeof(exa_uuid_t));

  while (admwrk_get_ack(&handle, &nodeid, &err))
  {
    exalog_debug("%s: license UUID comparison returned %d on '%s'",
		 adm_wt_get_name(), err,
		 adm_cluster_get_node_by_id(nodeid)->name);

    /* FIXME: We'd like to get -ADMIND_ERR_LICENSE if the node answering
     * has a different license, but as it asserted, we'll get node down.
     */
    if (err == -ADMIND_ERR_NODE_DOWN)
    {
      exalog_info("%s is down",
		   adm_cluster_get_node_by_id(nodeid)->name);
      continue;
    }

    if (err != EXA_SUCCESS)
      exalog_error("Unexpected reply from %s: %s (%d)",
		   adm_cluster_get_node_by_id(nodeid)->name,
		   exa_error_msg(err), err);
  }
}

static void
admin_check_license_local(int thr_nb, void *msg)
{
  const exa_uuid_t *leader_license_uuid = (exa_uuid_t *)msg;
  const exa_uuid_t *my_license_uuid = adm_license_get_uuid(exanodes_license);

  exalog_debug("comparing my UUID " UUID_FMT " with the leader's " UUID_FMT,
	       UUID_VAL(my_license_uuid), UUID_VAL(leader_license_uuid));

  if (uuid_is_equal(leader_license_uuid, my_license_uuid))
  {
    admwrk_ack(thr_nb, EXA_SUCCESS);
  }
  else
  {
    /* This node's license UUID is different than the leader's. This can
     * not happen unless the user has fiddled with the licenses himself.
     * This is not supported, hence we assert. FIXME: We should make sure
     * it can't happen on normal use conditions.
     */
    /* FIXME: admwrk_ack only sets the reply, which is sent only after
     * returning from this function. We won't return as we'll assert...
     */
    admwrk_ack(thr_nb, -ADMIND_ERR_LICENSE);
    exalog_error("Wrong license UUID " UUID_FMT ", expected " UUID_FMT,
		    UUID_VAL(my_license_uuid), UUID_VAL(leader_license_uuid));
    EXA_ASSERT_VERBOSE(false, "Wrong license UUID " UUID_FMT
		    ", expected " UUID_FMT,
		    UUID_VAL(my_license_uuid), UUID_VAL(leader_license_uuid));
  }
}

static int
admin_recover(int thr_nb)
{
  EXA_ASSERT_VERBOSE(adm_is_leader(), "the clustered command must run on the master");
  EXA_ASSERT_VERBOSE(thr_nb == RECOVERY_THR_ID,
                     "the recovery must use working thread %d", RECOVERY_THR_ID);

  admin_verify_license_unicity(thr_nb);

  return admwrk_exec_command(thr_nb, &adm_service_admin, RPC_SERVICE_ADMIND_RECOVER, NULL, 0);
}


static int
save_config(int thr_nb, const char *buffer, int size, char *error_msg)
{
  int ret;

  /* Parse the configuration file */

  adm_cluster_cleanup();
  ret = adm_deserialize_from_memory(buffer, size, error_msg, false /* create */);
  EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "adm_deserialize_from_memory(): %s (%d)",
		     exa_error_msg(ret), ret);

  /* Save the configuration file */

  conf_save_synchronous_without_inc_version();

  exalog_debug("Save the configuration file: DONE\n");

  return EXA_SUCCESS;
}

static void update_examsgd_node_list(void)
{
    exa_nodeid_t nodeid;
    int err;

    for (nodeid = 0; nodeid < EXA_MAX_NODES_NUMBER; nodeid++)
    {
        const struct adm_node *node = adm_cluster_get_node_by_id(nodeid);
        bool node_known_by_examsgd = exa_nodeset_contains(&examsg_nodeset, nodeid);

        if (node != NULL && !node_known_by_examsgd)
        {
            err = examsgAddNode(adm_wt_get_localmb(), nodeid, node->name);
            if (err != EXA_SUCCESS)
                exalog_warning("examsgAddNode(%d, %s): %s", nodeid, node->name,
                               exa_error_msg(err));
            exa_nodeset_add(&examsg_nodeset, nodeid);
        }
        else if (node == NULL && node_known_by_examsgd)
        {
            err = examsgDelNode(adm_wt_get_localmb(), nodeid);
            if (err != EXA_SUCCESS)
                exalog_warning("examsgDelNode(%d): %s", nodeid, exa_error_msg(err));
            exa_nodeset_del(&examsg_nodeset, nodeid);
        }
    }
}

static void
admin_recover_local(int thr_nb, void *msg)
{
  char error_msg[EXA_MAXSIZE_LINE] = "\0";
  struct config_version_info info;
  /* FIXME config_version_info's "version" field is signed, make it unsigned
   * since everywhere the config version is considered to be unsigned! */
  uint64_t best_version;
  uint64_t worst_version;
  exa_nodeid_t best_node_id;
  admwrk_request_t rpc;
  exa_nodeid_t nodeid;
  char *buffer;
  int best;
  int size;
  int pos;
  int ret;
  int tm_err = EXA_SUCCESS;

  /* Exchange config file version numbers and compute best node */

  info.version = adm_cluster.version;
  info.confsize = adm_config_size;
  best_node_id = EXA_NODEID_NONE;
  best_version = 0;
  worst_version = UINT64_MAX;
  size = -1;

  COMPILE_TIME_ASSERT(sizeof(info) <= ADM_MAILBOX_PAYLOAD_PER_NODE);

  if (evmgr_mship_token_manager_is_set()
      && !evmgr_mship_token_manager_is_connected())
      tm_err = -ADMIND_WARN_TOKEN_MANAGER_DISCONNECTED;

  ret = admwrk_barrier(thr_nb, tm_err, "Sending token manager status");

  admwrk_bcast(thr_nb, &rpc, EXAMSG_SERVICE_ADMIND_VERSION_ID,
	       &info, sizeof(info));

  while (admwrk_get_bcast(&rpc, &nodeid, &info, sizeof(info), &ret))
  {
    if (ret == -ADMIND_ERR_NODE_DOWN)
      continue;

    if (info.version > best_version ||
	(info.version == best_version && nodeid < best_node_id))
    {
      best_version = info.version;
      best_node_id = nodeid;
      size = info.confsize;
    }

    if (info.version < worst_version)
      worst_version = info.version;

    exalog_debug("node '%u' has version %" PRId64 " and size %d",
		 nodeid, info.version, info.confsize);
  }

  exalog_debug("best: node=%u, version=%" PRId64 ", size=%d / worst: version=%" PRId64,
	       best_node_id, best_version, size, worst_version);

  EXA_ASSERT(EXA_NODEID_VALID(best_node_id));

  best = (best_node_id == adm_my_id);

  /* Best node send its config file data if needed */

  if (best_version == worst_version)
  {
    exalog_debug("We are all up to date version '%" PRId64 "'", best_version);
    EXA_ASSERT(adm_config_size == size);
    ret = EXA_SUCCESS;
    goto ack;
  }

  if (adm_cluster.version == best_version)
  {
    exalog_debug("I am up-to-date at '%" PRId64 "'", best_version);
    buffer = adm_config_buffer;
  }
  else
  {
    exalog_debug("I need to be updated from '%" PRId64 "' to '%" PRId64 "'",
                 adm_cluster.version, best_version);
    buffer = os_malloc(size + 1);
  }

  /* This loop MUST be completely done (ie no break is allowed on a node down)
   * because there is no guaranty that all nodes will detect the node down at
   * the same time (some can finish on success and some can finish on nodedown)
   * */
  for (pos = 0 ; pos < size; pos += ADM_CONFIG_RECOVER_BUF_LEN)
  {
      char tmp_buf[ADM_CONFIG_RECOVER_BUF_LEN];
      int chunk_size;
      int chunk_ret;

      chunk_size = size - pos;

      if (chunk_size > ADM_CONFIG_RECOVER_BUF_LEN)
	  chunk_size = ADM_CONFIG_RECOVER_BUF_LEN;

      exalog_debug("bcast chunk pos=%d, size=%d", pos, chunk_size);

      /* only the best sends chunks */
      admwrk_bcast(thr_nb, &rpc, EXAMSG_SERVICE_ADMIND_CONFIG_CHUNK,
	      buffer + pos, best ? chunk_size : 0);

      /* if the node already has a good buffer, just throw away the messages by
       * passing NULL and 0 for buffer and size */
      while (admwrk_get_bcast(&rpc, &nodeid,
		  (adm_cluster.version == best_version) ? NULL : tmp_buf,
		  (adm_cluster.version == best_version) ? 0 : chunk_size,
		  &chunk_ret))
      {
	  /* remember that a something went wrong, node down being the most
	   * critical*/
	  if (ret != -ADMIND_ERR_NODE_DOWN && chunk_ret != EXA_SUCCESS)
	      ret = chunk_ret;

	  /* If the message comes from the best node, and if we need update,
	   * append data into buffer */
	  if (!ret && nodeid == best_node_id
		   && adm_cluster.version != best_version)
	      memcpy(buffer + pos, tmp_buf, chunk_size);
      }
  }

  /* Save the new config on disk and reload it */
  if (ret == EXA_SUCCESS && adm_cluster.version != best_version)
  {
    exalog_debug("Save the new config");

    ret = save_config(thr_nb, buffer, size, error_msg);
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "save_config(): %s", exa_error_msg(ret));

    os_free(adm_config_buffer);
    adm_config_buffer = buffer;
    adm_config_size = size;
  }

  ret = admwrk_barrier_msg(thr_nb, ret, "ADMIND: Save the config file",
			   "Failed to parse the config file: %s", error_msg);

  update_examsgd_node_list();

ack:

  admwrk_ack(thr_nb, ret);

}


static int
admin_nodestop(int thr_nb, const exa_nodeset_t *nodes_to_stop)
{
  if (!adm_nodeset_contains_me(nodes_to_stop))
    return EXA_SUCCESS;

  adm_cluster.persistent_goal = ADM_CLUSTER_GOAL_STOPPED;
  adm_cluster.goal = ADM_CLUSTER_GOAL_STOPPED;

  adm_set_state(ADMIND_STARTING);

  /* XXX (Comment adapted from old conf-saving version) The goal can be saved
   * here even if it is in recovery thread because admind service is always
   * the last to be stopped, thus the virtualizer is already stopped, and thus
   * there cannot be IO on exanodes volumes at this point. */
  /* FIXME Locking? */
  return adm_cluster_save_goal(adm_cluster.persistent_goal);
}


static void
admin_stop_local(int thr_nb, void *msg)
{
  exa_nodeset_t *nodes_to_stop = &((stop_data_t *)msg)->nodes_to_stop;

  admin_nodestop(thr_nb, nodes_to_stop);
  admwrk_ack(thr_nb, EXA_SUCCESS);
}


static int
admin_stop(int thr_nb, const stop_data_t *stop_data)
{
  int error_val;
  exalog_debug("admind stop");

  error_val = admwrk_exec_command(thr_nb, &adm_service_admin, RPC_SERVICE_ADMIND_STOP,
                                  stop_data, sizeof(*stop_data));

  return error_val;
}


static int
admin_shutdown(int thr_nb)
{
  exa_nodeid_t nodeid;

  exalog_debug("admind shutdown (non clustered) begin");

  for (nodeid = 0; nodeid < EXA_MAX_NODES_NUMBER; nodeid++)
  {
    if (exa_nodeset_contains(&examsg_nodeset, nodeid))
      examsgDelNode(adm_wt_get_localmb(), nodeid);
  }

  exa_nodeset_reset(&examsg_nodeset);

  return 0;
}


const struct adm_service adm_service_admin =
{
  .id       = ADM_SERVICE_ADMIN,
  .init     = admin_init,
  .recover  = admin_recover,
  .stop     = admin_stop,
  .shutdown = admin_shutdown,
  .local_commands =
  {
    { RPC_SERVICE_ADMIND_RECOVER,          admin_recover_local       },
    { RPC_SERVICE_ADMIND_STOP,             admin_stop_local          },
    { RPC_SERVICE_ADMIND_CHECK_LICENSE,    admin_check_license_local },
    { RPC_COMMAND_NULL, NULL },
  }
};
