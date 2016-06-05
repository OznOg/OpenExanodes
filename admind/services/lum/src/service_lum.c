/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/services/lum/include/target_config.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_file_ops.h"
#include "lum/export/include/export.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"
#include "admind/include/service_lum.h"
#include "target/iscsi/include/iqn.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_config.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_env.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_error.h"
#include "os/include/os_string.h"
#include "log/include/log.h"
#include "lum/client/include/lum_client.h"

#include "target/iscsi/include/iqn.h"

static iqn_t target_iqn_val;
static iqn_t *target_iqn = NULL;

static int iface_list_fd;

const iqn_t *lum_get_target_iqn(void)
{
    return target_iqn;
}

static int lum_target_config_load(void)
{
    int err;
    char conf_file[OS_PATH_MAX];

    err = exa_env_make_path(conf_file, sizeof(conf_file), exa_env_nodeconfdir(),
                            TARGET_CONF);
    if (err != 0)
        return err;

    target_config_init_defaults();

    return target_config_load(conf_file);
}

static int lum_init(admwrk_ctx_t *ctx)
{
    char iqn_compliant_name[EXA_MAXSIZE_CLUSTERNAME + 1];
    lum_init_params_t params;
    char *p;
    int err;

    iface_list_fd = os_socket(AF_INET, SOCK_STREAM, 0);

    err = lum_target_config_load();
    if (err != EXA_SUCCESS)
    {
        exalog_error("Failed to load the target configuration");
        return err;
    }

    /* Replace underscores ('_') with dashes ('-') as underscore is
       not allowed in IQNs */
    os_strlcpy(iqn_compliant_name, adm_cluster.name, sizeof(iqn_compliant_name));
    for (p = iqn_compliant_name; *p != '\0'; p++)
        if (*p == '_')
            *p = '-';

    /* initialize target IQN */
    iqn_set(&target_iqn_val, "iqn.2004-05.com.seanodes:exanodes-%s",
            iqn_compliant_name);
    target_iqn = &target_iqn_val;

    params.iscsi_queue_depth = adm_cluster_get_param_int("target_queue_depth");
    params.bdev_queue_depth = adm_cluster_get_param_int("bdev_target_queue_depth");
    params.buffer_size = adm_cluster_get_param_int("target_buffer_size");
    params.target_listen_address = target_config_get_listen_address();
    iqn_copy(&params.target_iqn, lum_get_target_iqn());

    /* initialize the LUM executive */
    err = lum_client_init(adm_wt_get_localmb(), &params);

    if (err != EXA_SUCCESS)
    {
        exalog_error("Failed to initialize the LUM executive");
        return err;
    }

    /* Initialize exports into memory */
    return lum_deserialize_exports();
}

static int lum_shutdown(admwrk_ctx_t *ctx)
{
    int err;

    err = lum_client_cleanup(adm_wt_get_localmb());
    if (err != EXA_SUCCESS)
    {
        exalog_error("Failed to cleanup the LUM executive");
        return err;
    }

    /* clear target IQN */
    target_iqn = NULL;

    /* free exports from memory */
    lum_exports_clear();

    lum_exports_set_version(EXPORTS_VERSION_DEFAULT);

    os_closesocket(iface_list_fd);

    return EXA_SUCCESS;
}

int lum_service_publish_export(const exa_uuid_t *uuid)
{
    size_t buf_size = export_serialized_size();
    char buf[buf_size];
    int ret;

    if (adm_volume_is_exported(uuid))
        return EXA_SUCCESS;

    /* Set the reaonly flag corrctly as it is handled by volume for now */
    lum_exports_set_readonly_by_uuid(uuid,
                  adm_volume_is_goal_readonly(uuid, adm_my_id));

    ret = lum_exports_serialize_export_by_uuid(uuid, buf, buf_size);
    if (ret != EXA_SUCCESS)
        return ret;

    ret = lum_client_export_publish(adm_wt_get_localmb(), buf, buf_size);

    if (ret == -ADMIND_ERR_NODE_DOWN)
    {
        exalog_debug("Exporting volume " UUID_FMT " failed on : %s (%d) ",
                     UUID_VAL(uuid), exa_error_msg(ret), ret);
        return ret;
    }
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS,
                       "Exporting volume " UUID_FMT " failed on : %s (%d) ",
                        UUID_VAL(uuid), exa_error_msg(ret), ret);

    if (ret == EXA_SUCCESS)
        adm_volume_set_exported(uuid, true);

    if (ret == EXA_SUCCESS && lum_exports_get_type_by_uuid(uuid) == EXPORT_BDEV)
    {
        /* FIXME What to do when set readahead fails during recovery ?
         * Is that really a problem that needs to make the whole recovery
         * fail ?*/
        ret =  lum_client_set_readahead(adm_wt_get_localmb(), uuid,
                                        adm_volume_get_readahead(uuid));
        if (ret != EXA_SUCCESS)
            exalog_warning("Failed setting readahead on "UUID_FMT,
                     UUID_VAL(uuid));
    }

#ifdef WITH_FS
    /* FIXME The offline condition is legacy but doesn't seem right here.
       It has to do with a volume's start status, not the export, so it
       should not be checked here but rather in _restart_volume(), except
       that we can only notify the FS after the volume has been exported */
    if (adm_volume_is_offline(uuid))
        inst_set_resources_changed_down(&adm_service_fs);
    else
        inst_set_resources_changed_up(&adm_service_fs);
#endif

    return EXA_SUCCESS;
}

static int local_lum_recover_reexport_all_exports(admwrk_ctx_t *ctx)
{
    const adm_export_t *adm_export;
    int i;

    lum_exports_for_each_export(i, adm_export)
    {
        const exa_uuid_t *uuid = adm_export_get_uuid(adm_export);
        if (adm_volume_is_started(uuid, adm_my_id))
        {
            int err = lum_service_publish_export(uuid);
            if (err != EXA_SUCCESS)
            {
                /* XXX Why stop instead of doing best effort and try to
                       export as many volumes as possible? */
                return err;
            }
        }
    }

    return EXA_SUCCESS;
}

static int local_lum_set_peers(void)
{
    adapter_peers_t peers;
    exa_nodeid_t node_id;

    for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
    {
        const struct adm_node *node = adm_cluster_get_node_by_id(node_id);

        if (node != NULL)
            os_strlcpy(peers.addresses[node_id], adm_nic_ip_str(node->nic),
                       sizeof(peers.addresses[node_id]));
        else
            memset(peers.addresses[node_id], '\0', sizeof(peers.addresses[node_id]));
    }

    return lum_client_set_peers(adm_wt_get_localmb(), &peers);
}

#define MAX_NUM_ADDRESSES 10
typedef struct
{
    in_addr_t listen_addr[MAX_NUM_ADDRESSES];
} __node_target_addresses_t;

static void local_lum_get_listen_addresses(__node_target_addresses_t *addresses)
{
    in_addr_t listen_addr;
    int i;

    /* Initialize all addresses to 0.0.0.0 (which will be ignored) */
    for (i = 0; i < MAX_NUM_ADDRESSES; i++)
        addresses->listen_addr[i] = INADDR_ANY;

    listen_addr = target_config_get_listen_address();

    if (listen_addr == INADDR_ANY)
    {
        os_iface ifaces[MAX_NUM_ADDRESSES];
        int num_ifs, n;

        num_ifs = os_iface_get_all(iface_list_fd, ifaces, MAX_NUM_ADDRESSES);

        n = 0;
        for (i = 0; i < num_ifs; i++)
        {
            const struct sockaddr_in *addr = os_iface_addr(&ifaces[i]);

            if (addr->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
                continue;

            addresses->listen_addr[n] = addr->sin_addr.s_addr;
            n++;
        }
    }
    else /* Only one address on which we listen. */
        addresses->listen_addr[0] = listen_addr;
}

static void local_lum_recover(admwrk_ctx_t *ctx, void *msg)
{
  uint64_t version, best_version = EXPORTS_VERSION_DEFAULT;
  uint64_t exports_version = lum_exports_get_version();
  exa_nodeid_t best_node_id = EXA_NODEID_NONE;
  exa_nodeid_t nodeid;
  int ret, chunk_ret;
  int i, elements_to_sync = 0;
  bool am_best = false, need_update = false;
  int barrier_ret;
  exa_nodeset_t nodes_up, nodes_going_up, nodes_going_down;
  __node_target_addresses_t tmp_addr;
  in_addr_t addresses[MAX_NUM_ADDRESSES * EXA_MAX_NODES_NUMBER];
  int num_addr = 0;

  ret = local_lum_set_peers();

  barrier_ret = admwrk_barrier(ctx, ret, "Setting peers");
  if (barrier_ret != EXA_SUCCESS)
  {
      exalog_error("Cannot set peers: %s (%d).",
		   exa_error_msg(barrier_ret), barrier_ret);
      admwrk_ack(ctx, barrier_ret);
      return;
  }

  inst_get_nodes_up(&adm_service_lum, &nodes_up);
  inst_get_nodes_going_up(&adm_service_lum, &nodes_going_up);
  inst_get_nodes_going_down(&adm_service_lum, &nodes_going_down);

  exa_nodeset_sum(&nodes_up, &nodes_going_up);
  exa_nodeset_substract(&nodes_up, &nodes_going_down);

  ret = lum_client_set_membership(adm_wt_get_localmb(), &nodes_up);

  barrier_ret = admwrk_barrier(ctx, ret, "Setting membership");
  if (barrier_ret != EXA_SUCCESS)
  {
      exalog_error("Cannot set LUM membership: %s (%d).",
		   exa_error_msg(barrier_ret), barrier_ret);
      admwrk_ack(ctx, barrier_ret);
      return;
  }

  local_lum_get_listen_addresses(&tmp_addr);

  /* Exchange target listen addresses */
  admwrk_bcast(admwrk_ctx(), EXAMSG_SERVICE_LUM_TARGET_LISTEN_ADDRESSES,
	       &tmp_addr, sizeof(tmp_addr));

  ret = EXA_SUCCESS;
  while (admwrk_get_bcast(ctx, &nodeid, EXAMSG_SERVICE_LUM_TARGET_LISTEN_ADDRESSES, &tmp_addr, sizeof(tmp_addr), &chunk_ret))
  {
      if (ret != -ADMIND_ERR_NODE_DOWN && chunk_ret != EXA_SUCCESS)
          ret = chunk_ret;

      if (chunk_ret == -ADMIND_ERR_NODE_DOWN)
          continue;

      for (i = 0; i < MAX_NUM_ADDRESSES; i++)
      {
          if (tmp_addr.listen_addr[i] != INADDR_ANY)
              addresses[num_addr++] = tmp_addr.listen_addr[i];
      }
  }

  barrier_ret = admwrk_barrier(ctx, ret, "Exchanging target listen addresses");
  if (barrier_ret != EXA_SUCCESS)
  {
      exalog_error("Cannot exchange listen addresses: %s (%d).",
		   exa_error_msg(barrier_ret), barrier_ret);
      admwrk_ack(ctx, barrier_ret);
      return;
  }

  ret = lum_client_set_targets(adm_wt_get_localmb(), num_addr, addresses);

  barrier_ret = admwrk_barrier(ctx, ret, "Setting targets");
  if (barrier_ret != EXA_SUCCESS)
  {
      exalog_error("Cannot set targets: %s (%d).",
		   exa_error_msg(barrier_ret), barrier_ret);
      admwrk_ack(ctx, barrier_ret);
      return;
  }
  /* Exchange exports file version number */
  admwrk_bcast(admwrk_ctx(), EXAMSG_SERVICE_LUM_EXPORTS_VERSION,
	       &exports_version, sizeof(exports_version));

  while (admwrk_get_bcast(ctx, &nodeid, EXAMSG_SERVICE_LUM_EXPORTS_VERSION, &version, sizeof(version), &ret))
  {
      if (ret == -ADMIND_ERR_NODE_DOWN)
          continue;

      if (version > best_version ||
          (version == best_version && nodeid < best_node_id))
      {
          best_version = version;
          best_node_id = nodeid;
      }

      exalog_debug("node '%u' has exports file version %" PRIu64,
                   nodeid, version);
  }

  barrier_ret = admwrk_barrier(ctx, ret, "Exchanging exports version");
  if (barrier_ret != EXA_SUCCESS)
  {
      exalog_error("Cannot exchange exports version: %s (%d).",
		   exa_error_msg(barrier_ret), barrier_ret);
      admwrk_ack(ctx, barrier_ret);
      return;
  }

  /* we can now figure out who to get the data from */
  EXA_ASSERT(EXA_NODEID_VALID(best_node_id));

  am_best = best_node_id == adm_my_id;
  need_update = exports_version < best_version;

  exalog_debug("Best exports file on node %u (version %" PRIu64 ").%s%s",
               best_node_id, best_version,
               am_best ? " I have the best file and will send." : "",
               need_update ? " I need to update" : " I don't have to update");

  if (am_best)
      EXA_ASSERT(!need_update);

  if (need_update)
      EXA_ASSERT(adm_nodeset_contains_me(&nodes_going_up));

  /* First send/receive the number of exports to get */
  elements_to_sync = lum_exports_get_number();

  if (am_best)
  {
      exalog_debug("sending number of elements to sync: %d", elements_to_sync);
      admwrk_bcast(admwrk_ctx(), EXAMSG_SERVICE_LUM_EXPORTS_NUMBER,
                   &elements_to_sync, sizeof(elements_to_sync));
  }
  else
  {
      if (need_update)
      {
          lum_exports_clear();
          elements_to_sync = 0;
      }
      admwrk_bcast(admwrk_ctx(), EXAMSG_SERVICE_LUM_EXPORTS_NUMBER, NULL, 0);
  }

  while (admwrk_get_bcast(ctx, &nodeid, EXAMSG_SERVICE_LUM_EXPORTS_NUMBER,
                          need_update ? &i : NULL,
                          need_update ? sizeof(i) : 0,
                          &chunk_ret))
  {
      if (ret != -ADMIND_ERR_NODE_DOWN && chunk_ret != EXA_SUCCESS)
          ret = chunk_ret;
      if (need_update && nodeid == best_node_id)
      {
          exalog_debug("got %d elements to sync from %d", i, nodeid);
          elements_to_sync = i;
      }
  }

  barrier_ret = admwrk_barrier(ctx, ret, "Exchanging exports number");
  if (barrier_ret != EXA_SUCCESS)
  {
      exalog_error("Cannot exchange exports number: %s (%d).",
		   exa_error_msg(barrier_ret), barrier_ret);
      admwrk_ack(ctx, barrier_ret);
      return;
  }

  /* now synchronise the exports */
  for (i = 0; i < elements_to_sync; i++)
  {
      size_t buf_size = adm_export_serialized_size();
      char buf[buf_size];

      /* send/receive export i */
      if (am_best)
      {
          const adm_export_t *adm_export = lum_exports_get_nth_export(i);

          exalog_debug("sending element %d " UUID_FMT, i,
                       UUID_VAL(adm_export_get_uuid(adm_export)));

          EXA_ASSERT(adm_export_serialize(adm_export, buf, buf_size) == buf_size);
          admwrk_bcast(admwrk_ctx(), EXAMSG_SERVICE_LUM_EXPORTS_EXPORT,
                       buf, buf_size);
      }
      else
          admwrk_bcast(admwrk_ctx(), EXAMSG_SERVICE_LUM_EXPORTS_EXPORT,
                       NULL, 0);

      /* receive the export for those needing it */
      while (admwrk_get_bcast(ctx, &nodeid, EXAMSG_SERVICE_LUM_EXPORTS_EXPORT,
                              need_update ? buf : NULL,
                              need_update ? buf_size : 0,
                              &chunk_ret))
      {
          if (ret != -ADMIND_ERR_NODE_DOWN && chunk_ret != EXA_SUCCESS)
              ret = chunk_ret;

          if (need_update && ret == EXA_SUCCESS && nodeid == best_node_id)
          {
              /* if this comes from the best node, get it. */
              adm_export_t *adm_export = adm_export_alloc();

              EXA_ASSERT(adm_export);
              EXA_ASSERT(adm_export_deserialize(adm_export, buf, buf_size) == buf_size);

              exalog_debug("got element %d "UUID_FMT" from best node", i,
                           UUID_VAL(adm_export_get_uuid(adm_export)));
              EXA_ASSERT(lum_exports_insert_export(adm_export) == EXA_SUCCESS);
          }
      }
  }

  if (ret != EXA_SUCCESS)
  {
      exalog_error("Cannot exchange exports data: %s (%d).",
		   exa_error_msg(ret), ret);
      admwrk_ack(ctx, ret);
      if (need_update)
          /* rollback MUST be successful otherwise situation is unrecoverable */
          EXA_ASSERT(lum_deserialize_exports() == EXA_SUCCESS);
      return;
  }

  if (need_update)
  {
      exalog_debug("Exports exchange done, saving file.\n");
      lum_exports_set_version(best_version);
      lum_serialize_exports();
  }

  barrier_ret = admwrk_barrier(ctx, ret, "Exchanging exports data");
  if (barrier_ret != EXA_SUCCESS)
  {
      exalog_error("Cannot exchange exports data: %s (%d).",
		   exa_error_msg(barrier_ret), barrier_ret);
      admwrk_ack(ctx, barrier_ret);
      return;
  }

  /* and re-export our exports */
  ret = local_lum_recover_reexport_all_exports(ctx);

  barrier_ret = admwrk_barrier(ctx, ret, "Re-exporting exports");
  if (barrier_ret != EXA_SUCCESS)
  {
      exalog_error("Cannot re-export exports: %s (%d).",
		   exa_error_msg(barrier_ret), barrier_ret);
      admwrk_ack(ctx, barrier_ret);
      return;
  }

  ret = lum_client_start_target(adm_wt_get_localmb());

  admwrk_ack(ctx, ret);
}

static int
lum_recover(admwrk_ctx_t *ctx)
{
  return admwrk_exec_command(ctx, &adm_service_lum,
                             RPC_SERVICE_LUM_RECOVER, NULL, 0);
}

static int lum_suspend(admwrk_ctx_t *ctx)
{
  return admwrk_exec_command(ctx, &adm_service_lum,
                             RPC_SERVICE_LUM_SUSPEND, NULL, 0);
}

static void local_lum_suspend(admwrk_ctx_t *ctx, void *msg)
{
  int ret = lum_client_suspend(adm_wt_get_localmb());

  admwrk_ack(ctx, ret);
}

static int lum_resume(admwrk_ctx_t *ctx)
{
  return admwrk_exec_command(ctx, &adm_service_lum,
                             RPC_SERVICE_LUM_RESUME, NULL, 0);
}

static void local_lum_resume(admwrk_ctx_t *ctx, void *msg)
{
  int ret = lum_client_resume(adm_wt_get_localmb());

  admwrk_ack(ctx, ret);
}

typedef struct
{
    exa_uuid_t uuid;
    exa_nodeset_t nodelist;
} lum_unpublish_args_t;

int lum_master_export_unpublish(admwrk_ctx_t *ctx, const exa_uuid_t *uuid,
                                const exa_nodeset_t *nodelist, bool force)
{
    lum_unpublish_args_t args;
    exa_nodeid_t nodeid;
    exa_nodeset_t nodes;
    int err, res = EXA_SUCCESS;

    if (force)
    {
        adm_write_inprogress(adm_nodeid_to_name(adm_my_id), "Checking force parameter",
                -ADMIND_WARNING_FORCE_DISABLED,
                "Force option is disabled on this version:"
                " ignoring during volume stop");
    }

    uuid_copy(&args.uuid, uuid);
    exa_nodeset_copy(&args.nodelist, nodelist);

    inst_get_current_membership_cmd(&adm_service_lum, &nodes);

    admwrk_run_command(ctx, &nodes,
                       RPC_SERVICE_LUM_UNPUBLISH, &args, sizeof(args));

    while (!exa_nodeset_is_empty(&nodes))
    {
        admwrk_get_ack(ctx, &nodes, &nodeid, &err);
        if (err == -ADMIND_ERR_NODE_DOWN)
            res = -ADMIND_ERR_NODE_DOWN;

        if (res != -ADMIND_ERR_NODE_DOWN
            && err != EXA_SUCCESS
            && err != -ADMIND_ERR_NOTHINGTODO)
        {
            char msg[EXA_MAXSIZE_LINE + 1];

            os_snprintf(msg, sizeof(msg), "Failed to unpublish "
                        "export '%" UUID_FMT"'", UUID_VAL(uuid));
            adm_write_inprogress(adm_nodeid_to_name(nodeid),
                                 msg, err, exa_error_msg(err));
            res = err;
        }
    }

    return res;
}

static void local_lum_unpublish(admwrk_ctx_t *ctx, void *msg)
{
    const lum_unpublish_args_t *args = msg;
    int err;

    /* Unpublish all exports on nodes that are stopping in order to make
     * sure that no IO can be performed from this node when PR is stopped.
     */
    if (adm_volume_is_exported(&args->uuid)
        && adm_nodeset_contains_me(&args->nodelist))
    {
        err = lum_client_export_unpublish(adm_wt_get_localmb(), &args->uuid);
        if (err == EXA_SUCCESS)
            adm_volume_set_exported(&args->uuid, false);
    }
    else
        err = -ADMIND_ERR_NOTHINGTODO;

  admwrk_ack(ctx, err);
}

static int lum_stop(admwrk_ctx_t *ctx, const stop_data_t *stop_data)
{
    const adm_export_t *export;
    int i, err;

    lum_exports_for_each_export(i, export)
    {
        err = lum_master_export_unpublish(ctx, adm_export_get_uuid(export),
                                          &stop_data->nodes_to_stop,
                                          stop_data->force);
        if (err != EXA_SUCCESS)
            return err;
    }

    return admwrk_exec_command(ctx, &adm_service_lum,
	 	     RPC_SERVICE_LUM_STOP, stop_data, sizeof(*stop_data));
}

static void local_lum_stop(admwrk_ctx_t *ctx, void *msg)
{
  const exa_nodeset_t *nodes_to_stop = &((const stop_data_t *)msg)->nodes_to_stop;
  exa_nodeset_t membership;
  int err, barrier_err;

  if (adm_nodeset_contains_me(nodes_to_stop))
      err = lum_client_stop_target(adm_wt_get_localmb());
  else
      err = -ADMIND_ERR_NOTHINGTODO;

  barrier_err = admwrk_barrier(ctx, err, "Stopping target");
  if (barrier_err != EXA_SUCCESS)
  {
      exalog_error("Cannot stop target: %s (%d).",
		   exa_error_msg(barrier_err), barrier_err);
      admwrk_ack(ctx, barrier_err);
      return;
  }

  /* FIXME export should be unpublished BEFORE stoping PR otherwise, an initiator
   * may have a reservation on a volume without guarantee that others clients
   * are not also doing reservations
   */

  if (adm_nodeset_contains_me(nodes_to_stop))
  {
      /* Put my node only in mship when stopping */
      exa_nodeset_single(&membership, adm_my_id);
  }
  else
  {
      exa_nodeset_t remaining_nodes;
      inst_get_nodes_up(&adm_service_lum, &remaining_nodes);
      exa_nodeset_substract(&remaining_nodes, nodes_to_stop);
      exa_nodeset_copy(&membership, &remaining_nodes);
  }

  err = lum_client_set_membership(adm_wt_get_localmb(), &membership);

  admwrk_ack(ctx, err);
}

/* Note: this is called on all nodes */
static void lum_nodedel(admwrk_ctx_t *ctx, struct adm_node *node)
{
    local_lum_set_peers();
}

const struct adm_service adm_service_lum =
{
  .id = ADM_SERVICE_LUM,
  .init = lum_init,
  .recover = lum_recover,
  .resume = lum_resume,
  .suspend = lum_suspend,
  .shutdown = lum_shutdown,
  .check_up = NULL,
  .check_down = NULL,
  .stop = lum_stop,
  .nodedel = lum_nodedel,
  .local_commands =
  {
   { RPC_SERVICE_LUM_RECOVER,   local_lum_recover   },
   { RPC_SERVICE_LUM_SUSPEND,   local_lum_suspend   },
   { RPC_SERVICE_LUM_RESUME,    local_lum_resume    },
   { RPC_SERVICE_LUM_STOP,      local_lum_stop      },
   { RPC_SERVICE_LUM_UNPUBLISH, local_lum_unpublish },
   { RPC_COMMAND_NULL, NULL }
  }
};
