/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_monitor.h"
#include "admind/src/adm_nic.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/admindstate.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/service_parameter.h"
#include "common/include/exa_env.h"
#include "common/include/exa_socket.h"
#include "common/include/exa_nodeset.h"
#include "log/include/log.h"
#include "nbd/service/include/nbd_msg.h"
#include "os/include/os_file.h"
#include "os/include/strlcpy.h"
#include "os/include/os_daemon_parent.h"
#include "os/include/os_stdio.h"

struct nbd_recover_disk_info
{
  uint64_t sectors;
  uint32_t nb;
  uint32_t pad;
};

static os_daemon_t server_daemon = OS_DAEMON_INVALID;
static os_daemon_t client_daemon = OS_DAEMON_INVALID;

static int
nbd_init(int thr_nb)
{
  int error_val = EXA_SUCCESS;
  struct adm_nic *nic;                       /**< the local network card for data network */
  char max_headers[EXA_MAXSIZE_BUFFER + 1];
  char client_network_type[EXA_MAXSIZE_BUFFER + 1];
  char server_network_type[EXA_MAXSIZE_BUFFER + 1];
  char node_id_str[16];
  const char *data_net_timeout = NULL;
  char clientd_path[OS_PATH_MAX];
  char serverd_path[OS_PATH_MAX];

  /* Get the path of daemon executables */
  exa_env_make_path(serverd_path, sizeof(serverd_path), exa_env_sbindir(), "exa_serverd");
  exa_env_make_path(clientd_path, sizeof(clientd_path), exa_env_sbindir(), "exa_clientd");

  /* Get the local network card for the data network */
  nic = adm_node_get_nic(adm_myself());
  if (!nic)
  {
    exalog_error("The network card is not defined for datanetwork on node %s",
		 adm_myself()->name);
    error_val = -EXA_ERR_XML_GET;
    goto end_init;
  }

  os_snprintf(client_network_type, sizeof(client_network_type), "TCP=%d",
	   adm_cluster_get_param_int("tcp_client_buffer_size") / 1024);

  os_snprintf(server_network_type, sizeof(server_network_type), "TCP=%d",
	   adm_cluster_get_param_int("tcp_server_buffer_size") / 1024);

  data_net_timeout = adm_cluster_get_param_text("tcp_data_net_timeout");

  os_snprintf(max_headers, sizeof(max_headers), "%d",
	   adm_cluster_get_param_int("max_client_requests") * adm_cluster_nb_nodes());

  os_snprintf(node_id_str, 16, "%u", adm_myself()->id);

  {
    char *const argv[] = {
        serverd_path,
        "-n", server_network_type,
        "-h", (char *)adm_nic_get_hostname(nic),
        "-i", node_id_str,
        "-m", max_headers,
        "-b", (char *)adm_cluster_get_param_text("max_request_size"),
        "-c", (char *)adm_cluster_get_param_text("server_buffers"),
        "-d", (char *)data_net_timeout,
        NULL
    };

    if (os_daemon_spawn(argv, &server_daemon) != 0)
      {
	error_val = -ADMIND_ERR_MODULESTART;
        goto end_init;
      }

    adm_monitor_register(EXA_DAEMON_SERVERD, server_daemon);
  }

  {
    char *argv[] = {
        clientd_path,
        "-n" , client_network_type,
        "-h", (char *)adm_nic_get_hostname(nic),
        "-p", (char *)adm_cluster_get_param_text("max_client_requests"),
        "-b", (char *)adm_cluster_get_param_text("max_request_size"),
        "-B", (char *)adm_cluster_get_param_text("io_barriers"),
        "-c", (char *)data_net_timeout,
        /* -A and -M come from vrt, -B is used for vrt and nbd */
        "-A", (char *)node_id_str,
        "-M", (char *)adm_cluster_get_param_text("max_requests"),
        /* VRT cl-tunables */
        "-s", (char *)adm_cluster_get_param_text("rebuilding_slowdown"),
        "-S", (char *)adm_cluster_get_param_text("degraded_rebuilding_slowdown"),
        NULL
    };

    if (os_daemon_spawn(argv, &client_daemon) != 0)
      {
        error_val = -ADMIND_ERR_MODULESTART;
        goto module_init_clean;
      }

    adm_monitor_register(EXA_DAEMON_CLIENTD, client_daemon);
  }

  goto end_init;

 module_init_clean:

  adm_monitor_unregister(EXA_DAEMON_SERVERD);
  serverd_quit(adm_wt_get_localmb(), adm_myself()->name);
  os_daemon_wait(server_daemon);

 end_init:
  return error_val;
}


static int
nbd_suspend(int thr_nb)
{
  return admwrk_exec_command(thr_nb, &adm_service_nbd, RPC_SERVICE_NBD_SUSPEND, NULL, 0);
}


static void
nbd_local_suspend(int thr_nb, void *msg)
{
  exa_nodeset_t nodes_up, nodes_going_up, nodes_going_down;
  struct adm_node *node;
  struct adm_disk *disk;
  int ret = EXA_SUCCESS;

  /* Compute the correct mship of service for working
   * FIXME this should probably be a parameter of this function */

  inst_get_nodes_up(&adm_service_nbd, &nodes_up);
  inst_get_nodes_going_up(&adm_service_nbd, &nodes_going_up);
  inst_get_nodes_going_down(&adm_service_nbd, &nodes_going_down);

  exa_nodeset_sum(&nodes_up, &nodes_going_up);
  exa_nodeset_substract(&nodes_up, &nodes_going_down);

  adm_cluster_for_each_node(node)
  {
    adm_node_for_each_disk(node, disk)
    {
      if (disk->suspended || !disk->imported)
	continue;

      if (exa_nodeset_contains(&nodes_up, node->id) && !disk->broken)
	continue;

      exalog_debug("clientd_device_suspend(" UUID_FMT ")", UUID_VAL(&disk->uuid));
      ret = clientd_device_suspend(adm_wt_get_localmb(), &disk->uuid);
      if (ret != EXA_SUCCESS)
      {
        exalog_error("clientd_device_suspend(" UUID_FMT "): %s",
                     UUID_VAL(&disk->uuid), exa_error_msg(ret));
	goto error;
      }

      disk->suspended = true;
    }
  }

error:
  admwrk_ack(thr_nb, ret);
}

/**
 * Unimport newly down disks.
 */
static int
nbd_recover_clientd_device_down(int thr_nb, exa_nodeset_t *nodes_going_down)
{
  struct adm_node *node;

  adm_cluster_for_each_node(node)
  {
    struct adm_disk *disk;

    adm_node_for_each_disk(node, disk)
    {
      int ret;

      if (!disk->imported ||
          (!exa_nodeset_contains(nodes_going_down, node->id) && !disk->broken))
	continue;

      EXA_ASSERT(disk->suspended);

      exalog_debug("clientd_device_down(" UUID_FMT ")", UUID_VAL(&disk->uuid));
      ret = clientd_device_down(adm_wt_get_localmb(), &disk->uuid);
      if (ret == -ADMIND_ERR_NODE_DOWN)
	return ret;
      EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "clientd_device_down(" UUID_FMT "): %s",
                         UUID_VAL(&disk->uuid), exa_error_msg(ret));

      disk->imported = false;
      inst_set_resources_changed_down(&adm_service_vrt);
    }
  }

  return EXA_SUCCESS;
}


/**
 * Close connections with newly down servers.
 */
static int
nbd_recover_clientd_close_session(int thr_nb, exa_nodeset_t *nodes_going_down)
{
  struct adm_node *node;

  adm_cluster_for_each_node(node)
  {
    int ret;

    if (!node->managed_by_clientd ||
	!exa_nodeset_contains(nodes_going_down, node->id))
      continue;

    exalog_debug("clientd_close_session(%s)", node->name);
    ret = clientd_close_session(adm_wt_get_localmb(), node->name, node->id);

    if (ret == -ADMIND_ERR_NODE_DOWN)
      return ret;
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "clientd_close_session(%s): %s",
		       node->name, exa_error_msg(ret));

    node->managed_by_clientd = false;
  }

  return EXA_SUCCESS;
}


/**
 * Add newly up nodes to serverd.
 */
static int
nbd_recover_serverd_add_client(int thr_nb, exa_nodeset_t *nodes_up)
{
  struct adm_node *node;

  adm_cluster_for_each_node(node)
  {
    struct adm_nic *nic;
    int ret;

    if (node->managed_by_serverd ||
	!exa_nodeset_contains(nodes_up, node->id))
      continue;

    nic = adm_node_get_nic(node);
    EXA_ASSERT(nic);

    exalog_debug("serverd_add_client(%s)", node->name);
    /* FIXME: Nodes are registerd on serverd with their node_id, but it would be
     * better to use node UUID to avoid identification problems. Unfortunately,
     * node UUIDs don't exist yet.
     */
    ret = serverd_add_client(adm_wt_get_localmb(), adm_nic_get_hostname(nic),
			     adm_nic_ip_str(nic), node->id);
    if (ret == -ADMIND_ERR_NODE_DOWN)
      return ret;
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "serverd_add_client(%s): %s",
		       node->name, exa_error_msg(ret));

    node->managed_by_serverd = true;
  }

  return EXA_SUCCESS;
}



/**
 * Remove newly down nodes from serverd.
 */
static int
nbd_recover_serverd_remove_client(int thr_nb, exa_nodeset_t *nodes_going_down)
{
  struct adm_node *node;

  adm_cluster_for_each_node(node)
  {
    int ret;

    if (!node->managed_by_serverd ||
        !exa_nodeset_contains(nodes_going_down, node->id))
      continue;

    exalog_debug("serverd_remove_client(%s)", node->name);
    ret = serverd_remove_client(adm_wt_get_localmb(), node->id);
    if (ret == -ADMIND_ERR_NODE_DOWN)
      return ret;
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "serverd_remove_client(%s): %s",
		       node->name, exa_error_msg(ret));

    node->managed_by_serverd = false;
  }

  return EXA_SUCCESS;
}


/**
 * Check if a new node can be added in the clique
 *
 * @param new_id  id of the node to add
 * @param graph   the connection graph
 * @param clique  the current clique
 */
static int
nbd_could_add_node_in_clique(exa_nodeid_t new_id, exa_nodeset_t *graph,
			exa_nodeset_t *clique)
{
  exa_nodeid_t node_id;
  int ret = true;

  /* Check whether the node is connected to itself */

  if (!exa_nodeset_contains(&graph[new_id], new_id))
  {
    exalog_debug("%s not connected to itself",
		 adm_cluster_get_node_by_id(new_id)->name);
    ret = false;
  }

  for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
  {
    /* If the node_id is not in the clique, don't check whether new_id
       is connected to it.*/

    if (!exa_nodeset_contains(clique, node_id))
      continue;

    /* Check whether new_id is connected to node_id in both directions. */

    if (!exa_nodeset_contains(&graph[new_id], node_id))
    {
      exalog_debug("%s not connected to %s",
		   adm_cluster_get_node_by_id(new_id)->name,
		   adm_cluster_get_node_by_id(node_id)->name);
      ret = false;
    }

    if (!exa_nodeset_contains(&graph[node_id], new_id))
    {
      exalog_debug("%s not connected to %s",
		   adm_cluster_get_node_by_id(node_id)->name,
		   adm_cluster_get_node_by_id(new_id)->name);
      ret = false;
    }
  }

  return ret;
}


/** Type of an element of the sorted table of nodes */
typedef struct
{
  exa_nodeid_t id; /**< id of the node */
  int degree;      /**< number of nodes it is connected to */
} node_sort_t;


/** Compare two node_sort_t elements */
static int
nbd_compare_degrees(const void *a, const void *b)
{
  return ((node_sort_t *)b)->degree - ((node_sort_t *)a)->degree;
}


/**
 * Crash if needed to ensure that all node is connected to each other.
 * To to so, it compute a clique with the Welsh & Powell algorithm.
 *
 * @param graph  oriented graph that represents NBD connections.
 */
static void
nbd_assert_disk_consistency(exa_nodeset_t *graph)
{
  node_sort_t sort[EXA_MAX_NODES_NUMBER];
  exa_nodeset_t clique = EXA_NODESET_EMPTY;
  int i, j;

  memset(sort, 0, sizeof(sort));

  /* Compute the degree of each node (the number of nodes
     it is connected to, in both direction) */

  for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
  {
    sort[i].id = i;

    if (adm_cluster_get_node_by_id(i) == NULL)
      continue;

    for (j = 0; j < EXA_MAX_NODES_NUMBER; j++)
    {
      if (exa_nodeset_contains(&graph[i], j))
      {
        sort[i].degree++;
        sort[j].degree++;
      }
    }
  }

  /* Sort the nodes by their degree */

  qsort(sort, EXA_MAX_NODES_NUMBER, sizeof(node_sort_t), nbd_compare_degrees);

  for (i = 0; i < adm_cluster_nb_nodes(); i++)
    exalog_debug("%d. %s (degree=%d)", i,
		 adm_cluster_get_node_by_id(sort[i].id)->name, sort[i].degree);

  /* Add the node to the clique if possible , in the sorted order. */

  for (i = 0; i < adm_cluster_nb_nodes(); i++)
    if (nbd_could_add_node_in_clique(sort[i].id, graph, &clique))
      exa_nodeset_add(&clique, sort[i].id);

  /* Debug */

  exalog_debug("The new datanet clique is " EXA_NODESET_FMT,
	       EXA_NODESET_VAL(&clique));

  /* Crash if we are not in the clique */
  if (!exa_nodeset_contains(&clique, adm_myself()->id))
      crash_need_reboot("Local node not in NBD service membership");
}

/**
 * Open connections to newly up servers.
 */
static int
nbd_recover_clientd_open_session(int thr_nb, exa_nodeset_t *nodes_up,
	                         exa_nodeset_t *nodes_going_up)
{
  struct {
    exa_nodeset_t connected_to;
    int ret;
  } info;
  exa_nodeid_t nodeid;
  exa_nodeset_t graph[EXA_MAX_NODES_NUMBER];
  admwrk_request_t handle;
  struct adm_node *node;
  int ret_down;
  int ret;

  info.connected_to = EXA_NODESET_EMPTY;
  info.ret = EXA_SUCCESS;

  /* Try to open session with all nodes of the cluster */

  adm_cluster_for_each_node(node)
  {
    struct adm_nic *nic;

    if (node->managed_by_clientd
	|| !(exa_nodeset_contains(nodes_up, node->id)
	     || exa_nodeset_contains(nodes_going_up, node->id)))
	continue;

    nic = adm_node_get_nic(node);
    EXA_ASSERT(nic);

    exalog_debug("clientd_open_session(%s)",
		 node->name);
    ret = clientd_open_session(adm_wt_get_localmb(),
                               adm_nic_get_hostname(nic),
			       adm_nic_ip_str(nic), node->id);
    exalog_debug("clientd_open_session(%s): %s",
		 node->name, exa_error_msg(ret));

    if (ret == -ADMIND_ERR_NODE_DOWN)
    {
      info.ret = ret;
      break;
    }

    if (ret != EXA_SUCCESS)
    {
      char msg[80];
      exalog_warning("Failed to open the connection with exa_serverd on %s: %s",
		     node->name, exa_error_msg(ret));
      os_snprintf(msg, 80, "Failed to connect to %s (%s)", adm_nic_get_hostname(nic),
                  adm_nic_ip_str(nic));
      adm_write_inprogress(adm_nodeid_to_name(adm_myself()->id), msg,
			   -NBD_WARN_OPEN_SESSION_FAILED, NULL);
      continue;
    }

    node->managed_by_clientd = true;
  }

  /* Fill in the set of nodes I'm connected to. */

  adm_cluster_for_each_node(node)
    if (node->managed_by_clientd)
      exa_nodeset_add(&info.connected_to, node->id);

  /* Get the full connection graph. */

  memset(graph, 0, sizeof(graph));
  ret = EXA_SUCCESS;

  COMPILE_TIME_ASSERT(sizeof(info) <= ADM_MAILBOX_PAYLOAD_PER_NODE);
  admwrk_bcast(thr_nb, &handle, EXAMSG_SERVICE_NBD_CLIQUE, &info, sizeof(info));

  while (admwrk_get_bcast(&handle, &nodeid, &info, sizeof(info), &ret_down))
  {
    if (ret_down == -ADMIND_ERR_NODE_DOWN ||
        info.ret == -ADMIND_ERR_NODE_DOWN)
    {
      ret = -ADMIND_ERR_NODE_DOWN;
      continue;
    }

    memcpy(&graph[nodeid], &info.connected_to,
	   sizeof(info.connected_to));
  }

  if (ret != EXA_SUCCESS)
    return ret;

  /* Assert if needed so that each node is connected to each other. */

  nbd_assert_disk_consistency(graph);

  return EXA_SUCCESS;
}

/**
 * Get information about local disks.
 */
static void
nbd_recover_serverd_device_get_info(int thr_nb, struct nbd_recover_disk_info *info)
{
  exported_device_info_t device_info;
  struct adm_disk *disk;
  int i = -1;

  memset(info, 0, sizeof(*info) * NBMAX_DISKS_PER_NODE);

  adm_node_for_each_disk(adm_myself(), disk)
  {
    int ret;
    i++; /* Start at 0 */

    if (!disk->local->reachable)
      continue;

    EXA_ASSERT(i < NBMAX_DISKS_PER_NODE);

    exalog_debug("serverd_device_get_info UUID " UUID_FMT,
		 UUID_VAL(&disk->uuid));
    ret = serverd_device_get_info(adm_wt_get_localmb(), &disk->uuid,
				  &device_info);
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "serverd_device_get_info UUID " UUID_FMT ": %s",
		       UUID_VAL(&disk->uuid), exa_error_msg(ret));

    info[i].nb = device_info.device_nb;
    info[i].sectors = device_info.device_sectors;
  }
}


/**
 * Import newly up disks.
 */
static int
nbd_recover_clientd_device_import(int thr_nb,
	                          exa_nodeset_t *nodes_up,
	                          exa_nodeset_t *nodes_going_up,
				  struct nbd_recover_disk_info *info)
{
  exa_nodeid_t nodeid;
  admwrk_request_t handle;
  int ret_down;
  int retval = EXA_SUCCESS;

  /* CAREFUL here ALL informations needs to be shared. Nodes going up needs
   * info from nodes already up, and formerly up nodes need info about new
   * comers */
  COMPILE_TIME_ASSERT(sizeof(*info) * NBMAX_DISKS <= ADM_MAILBOX_PAYLOAD_PER_NODE * EXA_MAX_NODES_NUMBER);
  admwrk_bcast(thr_nb, &handle, EXAMSG_SERVICE_NBD_DISKS_INFO,
               info, sizeof(*info) * adm_node_nb_disks(adm_myself()));

  while (admwrk_get_bcast(&handle, &nodeid, info, sizeof(*info) * NBMAX_DISKS_PER_NODE, &ret_down))
  {
    const struct adm_node *node;
    struct adm_disk *disk;
    int i = -1;

    if (ret_down != EXA_SUCCESS)
      continue;

    node = adm_cluster_get_node_by_id(nodeid);

    EXA_ASSERT(exa_nodeset_contains(nodes_up, node->id)
	       || exa_nodeset_contains(nodes_going_up, node->id));

    /* don't try to import devices from nodes no session is opened with */
    if (!node->managed_by_clientd)
      continue;

    adm_node_for_each_disk(node, disk)
    {
      int ret;
      i++; /* start at 0 */

      if (disk->imported)
	  continue;

      exalog_debug("clientd_device_import with UUID " UUID_FMT,
		   UUID_VAL(&disk->uuid));
      ret = clientd_device_import(adm_wt_get_localmb(), node->id,
				  &disk->uuid, info[i].nb, info[i].sectors);
      if (ret == -ADMIND_ERR_NODE_DOWN)
	{
	  retval = ret;
	  break;
	}

      if (ret == -NBD_ERR_NO_SESSION)
	{
	  exalog_info("can't import disk with UUID " UUID_FMT " as no session opened with node %s",
		      UUID_VAL(&disk->uuid), node->name);
	  break;
	}

      EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS,
			 "clientd_device_import UUID " UUID_FMT,
			 UUID_VAL(&disk->uuid));

      disk->suspended = true;
      disk->imported = true;
      inst_set_resources_changed_up(&adm_service_vrt);
    }
  }

  return retval;
}

/**
 * Main function for NBD recovery.
 *
 * UP and DOWN are independant, but there are 3 steps for each
 * that need to be separated by a barrier:
 *
 *       SERVER STEP 1 |   CLIENT STEP    | SERVER STEP 2
 *  UP      export     | connect / import |  start check
 * DOWN   stop check   | unimport / close |   unexport
 *
 * Some other barriers are needed to exchange info or to send some
 * inprogress messages.
 */
static void
nbd_local_recover(int thr_nb, void *msg)
{
  struct nbd_recover_disk_info info[NBMAX_DISKS_PER_NODE];
  exa_nodeset_t nodes_up, nodes_going_up, nodes_going_down;
  int ret;

  /* Compute the correct mship of service for working
   * FIXME this should probably be a parameter of this function */

  inst_get_nodes_up(&adm_service_nbd, &nodes_up);
  inst_get_nodes_going_up(&adm_service_nbd, &nodes_going_up);
  inst_get_nodes_going_down(&adm_service_nbd, &nodes_going_down);

  /* From a going up node point of view, nodes marked up are going up
   * and nodes marked down are going down (the later case is for recovery up
   * interruptions: a node going up cannot be sure it was not interrupted
   * before by a node down thus it cannot be sure that all recovery down
   * were done locally */
  if (adm_nodeset_contains_me(&nodes_going_up))
  {
      exa_nodeset_t nodes_down;
      inst_get_nodes_down(&adm_service_nbd, &nodes_down);
      /* Obviously, nodes that are going up, even if they are not committed up,
       * must not be considered as down. */
      exa_nodeset_substract(&nodes_down, &nodes_going_up);
      exa_nodeset_sum(&nodes_going_down, &nodes_down);
  }

  /*****************
   * SERVER STEP 1 *
   *****************/

  /* Add newly up nodes to serverd (UP) */

  ret = nbd_recover_serverd_add_client(thr_nb, &nodes_going_up);

  /* Stop data net checking (DOWN) */

  /***********
   * BARRIER *
   ***********/

  ret = admwrk_barrier(thr_nb, ret, "NBD: Export the disks");
  if (ret == -ADMIND_ERR_NODE_DOWN)
    goto end;
  EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS,
		     "nbd_local_recover() failed on another node");

  /***************
   * CLIENT STEP *
   ***************/

  /* Open connections to newly up servers and add all its disks if needed (UP) */
  /* client connect on servers*/

  ret = nbd_recover_clientd_open_session(thr_nb, &nodes_up, &nodes_going_up);
  if (ret == -ADMIND_ERR_NODE_DOWN)
    goto end;

  /* Get information about local disks (UP) */

  if (ret == EXA_SUCCESS)
    nbd_recover_serverd_device_get_info(thr_nb, info);

  /* Import newly up disks (UP) */

  if (ret == EXA_SUCCESS)
    ret = nbd_recover_clientd_device_import(thr_nb, &nodes_up, &nodes_going_up, info);

  /* Unimport newly down disks (DOWN) */

  if (ret == EXA_SUCCESS)
    ret = nbd_recover_clientd_device_down(thr_nb, &nodes_going_down);

  /* Close connections with newly down servers (DOWN) */

  if (ret == EXA_SUCCESS)
    ret = nbd_recover_clientd_close_session(thr_nb, &nodes_going_down);

  /***********
   * BARRIER *
   ***********/

  ret = admwrk_barrier(thr_nb, ret, "NBD: Import the disks");
  if (ret == -ADMIND_ERR_NODE_DOWN)
    goto end;
  EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS,
		     "nbd_local_recover() failed on another node");

  /*****************
   * SERVER STEP 2 *
   *****************/

  /* Remove newly down nodes from serverd (DOWN) */

  if (ret == EXA_SUCCESS)
    ret = nbd_recover_serverd_remove_client(thr_nb, &nodes_going_down);

end:
  admwrk_ack(thr_nb, ret);
}


static int
nbd_recover(int thr_nb)
{
  return admwrk_exec_command(thr_nb, &adm_service_nbd, RPC_SERVICE_NBD_RECOVER, NULL, 0);
}


static int
nbd_resume(int thr_nb)
{
  return admwrk_exec_command(thr_nb, &adm_service_nbd, RPC_SERVICE_NBD_RESUME, NULL, 0);
}


static void
nbd_local_resume(int thr_nb, void *msg)
{
  struct adm_node *node;
  struct adm_disk *disk;
  int ret = EXA_SUCCESS;

  adm_cluster_for_each_node(node)
  {
    adm_node_for_each_disk(node, disk)
    {
      if (!disk->suspended)
	continue;

      exalog_debug("clientd_device_resume device (" UUID_FMT ")",
                   UUID_VAL(&disk->uuid));
      ret = clientd_device_resume(adm_wt_get_localmb(), &disk->uuid);
      if (ret != EXA_SUCCESS)
      {
        exalog_error("clientd_device_resume(" UUID_FMT "): %s",
                     UUID_VAL(&disk->uuid), exa_error_msg(ret));
	goto error;
      }

      disk->suspended = false;
    }
  }

error:
  admwrk_ack(thr_nb, ret);
}


static int
nbd_stop(int thr_nb, const stop_data_t *stop_data)
{
  return admwrk_exec_command(thr_nb, &adm_service_nbd, RPC_SERVICE_NBD_STOP,
                             stop_data, sizeof(*stop_data));
}


static void
nbd_diskstop(int thr_nb, struct adm_node *node, struct adm_disk *disk,
	     const exa_nodeset_t *nodes_to_stop)
{
    int err;

    if (!disk->imported)
        return;

    if (adm_nodeset_contains_me(nodes_to_stop))
    {
        err = clientd_device_remove(adm_wt_get_localmb(), &disk->uuid);
        if (err != EXA_SUCCESS)
            exalog_error("Remove device " UUID_FMT " failed: %s(%d)",
                         UUID_VAL(&disk->uuid), exa_error_msg(err), err);
    }
    else
    {
        err = clientd_device_suspend(adm_wt_get_localmb(), &disk->uuid);
        if (err != EXA_SUCCESS)
            exalog_error("Suspend device" UUID_FMT " failed: %s(%d)",
                         UUID_VAL(&disk->uuid), exa_error_msg(err), err);

        err = clientd_device_down(adm_wt_get_localmb(), &disk->uuid);
        if (err != EXA_SUCCESS)
            exalog_error("Stop device " UUID_FMT " failed: %s(%d)",
                         UUID_VAL(&disk->uuid), exa_error_msg(err), err);

        err = clientd_device_resume(adm_wt_get_localmb(), &disk->uuid);
        if (err != EXA_SUCCESS)
            exalog_error("Resume device " UUID_FMT " failed: %s(%d)",
                         UUID_VAL(&disk->uuid), exa_error_msg(err), err);
    }

    disk->imported = false;
}


static void nbd_diskdel(int thr_nb, const struct adm_node *node,
			struct adm_disk *disk)
{
  int ret;

  EXA_ASSERT(!disk->suspended);

  if (disk->imported)
  {
    exalog_debug("clientd_device_remove(" UUID_FMT ")", UUID_VAL(&disk->uuid));
    ret = clientd_device_remove(adm_wt_get_localmb(), &disk->uuid);
    if (ret != EXA_SUCCESS)
      exalog_error("clientd_device_remove(" UUID_FMT "): %s",
                   UUID_VAL(&disk->uuid), exa_error_msg(ret));

    disk->imported = false;
  }

  admwrk_barrier(thr_nb, EXA_SUCCESS, "NBD: Remove the disk");

}


static int
nbd_nodestop(int thr_nb, const exa_nodeset_t *nodes_to_stop)
{
  struct adm_node *node;

  /***************
   * CLIENT STEP *
   ***************/

  adm_cluster_for_each_node(node)
  {
    struct adm_disk *disk;
    int ret;

    if (!exa_nodeset_contains(nodes_to_stop, node->id) &&
        !adm_nodeset_contains_me(nodes_to_stop))
      continue;

    adm_node_for_each_disk(node, disk)
      nbd_diskstop(thr_nb, node, disk, nodes_to_stop);

    if (!node->managed_by_clientd)
      continue;

    exalog_debug("clientd_close_session(%s)", node->name);
    ret = clientd_close_session(adm_wt_get_localmb(), node->name, node->id);
    if (ret != EXA_SUCCESS)
      exalog_error("clientd_close_session(%s): %s", node->name,
		   exa_error_msg(ret));

    node->managed_by_clientd = false;
  }

  /***********
   * BARRIER *
   ***********/

  admwrk_barrier(thr_nb, EXA_SUCCESS, "NBD: Unimport the disks");

  /*****************
   * SERVER STEP 2 *
   *****************/

  adm_cluster_for_each_node(node)
  {
    int ret;

    if (!exa_nodeset_contains(nodes_to_stop, node->id) &&
	!adm_nodeset_contains_me(nodes_to_stop))
      continue;

    if (node->managed_by_serverd)
    {
      exalog_debug("serverd_remove_client(%s)", node->name);
      ret = serverd_remove_client(adm_wt_get_localmb(), node->id);
      if (ret != EXA_SUCCESS)
	exalog_error("serverd_remove_client(%s): %s",
		     node->name, exa_error_msg(ret));

      node->managed_by_serverd = false;
    }
  }

  return EXA_SUCCESS;
}


static void
nbd_local_stop(int thr_nb, void *msg)
{
  int ret;
  const stop_data_t *stop_data = msg;

  ret = nbd_nodestop(thr_nb, &stop_data->nodes_to_stop);

  admwrk_ack(thr_nb, ret);
}


static int
nbd_shutdown(int thr_nb)
{
  int error_val = EXA_SUCCESS;

  /* stop nbd client daemon */
  if (client_daemon != OS_DAEMON_INVALID)
  {
      adm_monitor_unregister(EXA_DAEMON_CLIENTD);
      error_val = clientd_quit(adm_wt_get_localmb(), adm_myself()->name);
      os_daemon_wait(client_daemon);
      if (error_val != EXA_SUCCESS)
        goto barrier_end;
  }

  /* stop nbd server daemon */
  if (server_daemon != OS_DAEMON_INVALID)
  {
      adm_monitor_unregister(EXA_DAEMON_SERVERD);
      error_val = serverd_quit(adm_wt_get_localmb(), adm_myself()->name);
      os_daemon_wait(server_daemon);
  }

 barrier_end:
  exalog_debug("Done: %d", error_val);

  return error_val;
}


static void nbd_nodedel(int thr_nb, const struct adm_node *node)
{
  struct adm_disk *disk;
  int ret;

  exalog_debug("delete node %s", node->name);

  adm_node_for_each_disk(node, disk)
  {
    EXA_ASSERT(!disk->suspended);

    if (disk->imported)
    {
      exalog_debug("clientd_device_remove(" UUID_FMT ")", UUID_VAL(&disk->uuid));
      ret = clientd_device_remove(adm_wt_get_localmb(), &disk->uuid);
      if (ret != EXA_SUCCESS)
        exalog_error("clientd_device_remove(" UUID_FMT "): %s",
                     UUID_VAL(&disk->uuid), exa_error_msg(ret));

      disk->imported = false;
    }
  }
}


const struct adm_service adm_service_nbd =
{
  .id = ADM_SERVICE_NBD,
  .init = nbd_init,
  .recover = nbd_recover,
  .resume = nbd_resume,
  .suspend = nbd_suspend,
  .stop = nbd_stop,
  .shutdown = nbd_shutdown,
  .nodedel = nbd_nodedel,
  .diskdel = nbd_diskdel,
  .local_commands =
  {
    { RPC_SERVICE_NBD_SUSPEND,            nbd_local_suspend          },
    { RPC_SERVICE_NBD_RESUME,             nbd_local_resume           },
    { RPC_SERVICE_NBD_STOP,               nbd_local_stop             },
    { RPC_SERVICE_NBD_RECOVER,            nbd_local_recover          },
    { RPC_COMMAND_NULL, NULL }
  }
};
