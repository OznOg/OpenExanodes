/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>

#ifdef WITH_FS
#include "admind/services/fs/generic_fs.h"
#include "admind/services/fs/service_fs.h"
#include "admind/services/fs/type_gfs.h"
#include "admind/src/adm_fs.h"
#include "fs/include/exa_fsd.h"
#endif

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/evmgr/evmgr_mship.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/adm_monitor.h"
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"


struct components_reply
{
  uint64_t ret;
  uint64_t daemons[EXA_DAEMON__LAST + 1];
  uint64_t modules[EXA_MODULE__LAST + 1];
#ifdef WITH_FS
  uint64_t handle_gfs;
#endif
  bool token_manager_set;
  bool token_manager_connected;
};


void local_clinfo_components(admwrk_ctx_t *ctx, void *msg)
{
  struct components_reply reply;
  char line[EXA_MAXSIZE_LINE + 1];
  FILE *file;
  int i;

  /* reset all info */

  memset(&reply, 0, sizeof(reply));

#ifdef WITH_FS
  /* Still handling GFS ? */
  reply.handle_gfs = fs_handle_gfs;
#endif

  /* get daemons status
   * XXX for now this information is dummy since it is useless (it cannot
   * appear in clinfo since evmgr asserts if a daemon is missing) */
  /* FIXME remove this code as it is unused/useless */
  for (i = EXA_DAEMON__FIRST; i <= EXA_DAEMON__LAST; i++)
    reply.daemons[i] = true;

  reply.token_manager_set = evmgr_mship_token_manager_is_set();
  if (reply.token_manager_set)
      reply.token_manager_connected = evmgr_mship_token_manager_is_connected();

  /* get modules status */
#ifndef WIN32
  file = fopen("/proc/modules", "r");
  if (file == NULL)
  {
    reply.ret = -errno;
    exalog_error("fopen(\"/proc/modules\") returned %" PRIu64, reply.ret);
    goto error;
  }

  while (fgets(line, EXA_MAXSIZE_LINE + 1, file) != NULL)
  {
    char *ptr = strchr(line, ' ');
    if(ptr == NULL)
    {
      exalog_error("failed to find a space char");
      reply.ret = -ENOENT;
      goto error;
    }

    *ptr = '\0'; /* cut after the first colum */

    for (i = EXA_MODULE__FIRST; i <= EXA_MODULE__LAST; i++)
      if (!strcmp(exa_module_name(i), line))
	reply.modules[i] = true;
  }

  if (fclose(file) != 0)
  {
    reply.ret = -errno;
    exalog_error("fclose() returned %" PRIu64, reply.ret);
    goto error;
  }

error:
#else
  for (i = EXA_MODULE__FIRST; i <= EXA_MODULE__LAST; i++)
      reply.modules[i] = true;
#endif
  COMPILE_TIME_ASSERT(sizeof(reply) <= ADM_MAILBOX_PAYLOAD_PER_NODE);
  admwrk_reply(ctx, &reply, sizeof(reply));
}


int cluster_clinfo_components(admwrk_ctx_t *ctx, xmlNodePtr exanodes_node)
{
  exa_nodeid_t nodeid;
  xmlNodePtr software_node;
  xmlNodePtr modules_node;
  int ret;
  struct components_reply reply;
  int dummy;
  exa_nodeset_t todo;

  adm_nodeset_set_all(&todo);

  /* Create the software node in the XML doc */

  software_node = xmlNewChild(exanodes_node, NULL, BAD_CAST("software"), NULL);
  if (software_node == NULL)
  {
    exalog_error("xmlNewChild() returned NULL");
    return -EXA_ERR_XML_ADD;
  }

  /* Create the module node in the XML doc */

  modules_node = xmlNewChild(exanodes_node, NULL, BAD_CAST("modules"), NULL);
  if (modules_node == NULL)
  {
    exalog_error("xmlNewChild() returned NULL");
    return -EXA_ERR_XML_ADD;
  }

  exalog_debug("RPC_ADM_CLINFO_COMPONENTS");

  admwrk_run_command(ctx, &adm_service_admin, RPC_ADM_CLINFO_COMPONENTS, NULL, 0);

  while (admwrk_get_reply(ctx, &nodeid, &reply, sizeof(reply), &ret))
  {
    int node_down;
    xmlNodePtr software_node_node;
    xmlNodePtr modules_node_node;
    xmlNodePtr component_node;
    int i;

    exa_nodeset_del(&todo, nodeid);

    node_down = (ret == -ADMIND_ERR_NODE_DOWN);

    /* Create the software node node in the XML doc */

    software_node_node = xmlNewChild(software_node, NULL, BAD_CAST("node"), NULL);
    if (software_node_node == NULL)
    {
      exalog_error("xmlNewChild() returned NULL");
      ret = -EXA_ERR_XML_ADD;
      goto error;
    }

    /* Create software node properties in the XML doc */

    ret = xml_set_prop(software_node_node, "name",
		       adm_cluster_get_node_by_id(nodeid)->name);
    ret = ret ? ret : xml_set_prop(software_node_node, "status",
				   node_down ? ADMIND_PROP_DOWN : ADMIND_PROP_UP);
#ifdef WITH_FS
    ret = ret ? ret : xml_set_prop_ok(software_node_node, EXA_CONF_FS_HANDLE_GFS,
				  reply.handle_gfs);

    if (gfs_using_gulm())
    {
	if (gfs_get_designated_gulm_master() == nodeid)
	    xml_set_prop(software_node_node, "gulm_mode", "master");
	else if (gfs_is_gulm_lockserver(nodeid))
	    xml_set_prop(software_node_node, "gulm_mode", "slave");
	else
	    xml_set_prop(software_node_node, "gulm_mode", "client");
    }
#endif

    if (ret != EXA_SUCCESS)
      goto error;

    /* Create the modules node node in the XML doc */

    modules_node_node = xmlNewChild(modules_node, NULL, BAD_CAST("node"), NULL);
    if (modules_node_node == NULL)
    {
      exalog_error("xmlNewChild() returned NULL");
      ret = -EXA_ERR_XML_ADD;
      goto error;
    }

    /* Create modules node properties in the XML doc */

    ret =             xml_set_prop(modules_node_node, "name",
	                       adm_cluster_get_node_by_id(nodeid)->name);
    ret = ret ? ret : xml_set_prop(modules_node_node, "status",
			       node_down ? ADMIND_PROP_DOWN : ADMIND_PROP_UP);
    if (ret != EXA_SUCCESS)
      goto error;

    /* Do not try to get daemons & modules status is the node is down */

    if (node_down)
      continue;

    /* Create software component nodes in the XML doc */

    for (i = EXA_DAEMON__FIRST; i <= EXA_DAEMON__LAST; i++)
    {
      component_node = xmlNewChild(software_node_node, NULL, BAD_CAST("component"), NULL);
      if (component_node == NULL)
      {
	exalog_error("xmlNewChild() returned NULL");
	ret = -EXA_ERR_XML_ADD;
	goto error;
      }

      ret =             xml_set_prop(component_node, "name", exa_daemon_name(i));
      ret = ret ? ret : xml_set_prop_ok(component_node, "status", reply.daemons[i]);
      if (ret != EXA_SUCCESS)
	goto error;
    }

    component_node = xmlNewChild(software_node_node, NULL, BAD_CAST("component"), NULL);
    if (component_node == NULL)
    {
      exalog_error("xmlNewChild() returned NULL");
      ret = -EXA_ERR_XML_ADD;
      goto error;
    }

    ret =             xml_set_prop(component_node, "name", "token_manager");
    ret = ret ? ret : xml_set_prop_bool(component_node, "configured", reply.token_manager_set);
    ret = ret ? ret : xml_set_prop(component_node, "status",
                                   reply.token_manager_connected
                                   ? "CONNECTED"
                                   : "NOT CONNECTED");
    if (ret != EXA_SUCCESS)
      goto error;

    /* Create module component nodes in the XML doc */

    for (i = EXA_MODULE__FIRST; i <= EXA_MODULE__LAST; i++)
    {
      xmlNodePtr component_node;

      component_node = xmlNewChild(modules_node_node, NULL, BAD_CAST("component"), NULL);
      if (component_node == NULL)
      {
	exalog_error("xmlNewChild() returned NULL");
	ret = -EXA_ERR_XML_ADD;
	goto error;
      }

      ret =             xml_set_prop(component_node, "name", exa_module_name(i));
      ret = ret ? ret : xml_set_prop_ok(component_node, "status", reply.modules[i]);
      if (ret != EXA_SUCCESS)
	goto error;
    }
  }

  exa_nodeset_foreach(&todo, nodeid)
  {
    xmlNodePtr software_node_node;
    xmlNodePtr modules_node_node;

    /* Create the software node node in the XML doc */

    software_node_node = xmlNewChild(software_node, NULL, BAD_CAST("node"), NULL);
    if (software_node_node == NULL)
    {
      exalog_error("xmlNewChild() returned NULL");
      ret = -EXA_ERR_XML_ADD;
      goto error;
    }

    /* Create software node properties in the XML doc */

    ret =             xml_set_prop(software_node_node, "name", adm_cluster_get_node_by_id(nodeid)->name);
    ret = ret ? ret : xml_set_prop(software_node_node, "status", ADMIND_PROP_DOWN);
#ifdef WITH_FS
    if (gfs_using_gulm())
    {
	if (gfs_get_designated_gulm_master() == nodeid)
	    xml_set_prop(software_node_node, "gulm_mode", "master");
	else if (gfs_is_gulm_lockserver(nodeid))
	    xml_set_prop(software_node_node, "gulm_mode", "slave");
	else
	    xml_set_prop(software_node_node, "gulm_mode", "client");
    }
#endif


    if (ret != EXA_SUCCESS)
      goto error;

    /* Create the modules node node in the XML doc */

    modules_node_node = xmlNewChild(modules_node, NULL, BAD_CAST("node"), NULL);
    if (modules_node_node == NULL)
    {
      exalog_error("xmlNewChild() returned NULL");
      ret = -EXA_ERR_XML_ADD;
      goto error;
    }

    /* Create modules node properties in the XML doc */

    ret =             xml_set_prop(modules_node_node, "name", adm_cluster_get_node_by_id(nodeid)->name);
    ret = ret ? ret : xml_set_prop(modules_node_node, "status", ADMIND_PROP_DOWN);
    if (ret != EXA_SUCCESS)
      goto error;
  }

  return EXA_SUCCESS;

error:
  while (admwrk_get_reply(ctx, &nodeid, &reply, sizeof(reply), &dummy));
  return ret;
}

