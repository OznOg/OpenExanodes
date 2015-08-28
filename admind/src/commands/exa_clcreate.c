/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "admind/services/rdev/include/rdev.h"
#include "admind/src/adm_cache.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_deserialize.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_license.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_hostname.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admindstate.h"
#include "admind/src/saveconf.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "os/include/os_thread.h"

#ifdef USE_YAOURT
#include <yaourt/yaourt.h>
#endif


__export(EXA_ADM_CLCREATE) struct clcreate_params
  {
    char hostname[EXA_MAXSIZE_HOSTNAME + 1];
    __optional xmlDocPtr license __default(NULL);
    xmlDocPtr config;
    bool join;
  };

/*---------------------------------------------------------------------------*/
/** \brief Implements the clcreate command
 *
 * \param[in]     doc       XML doc to parse
 * \param[out]    data      Resulting parsed data
 * \param[in,out] err_desc  Error, if any
 */
/*---------------------------------------------------------------------------*/

static void
cluster_clcreate(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  char *hostname = ((struct clcreate_params *)data)->hostname;
  xmlDocPtr license = ((struct clcreate_params *)data)->license;
  xmlDocPtr xml_config = ((struct clcreate_params *)data)->config;
  bool join = ((struct clcreate_params *)data)->join;
  xmlChar *buffer;
  char __error_msg[EXA_MAXSIZE_LINE + 1] = "";
  struct adm_node *node;
  struct adm_disk *disk;
  int ret = EXA_SUCCESS;
  int size;

  if (adm_cluster.created)
    {
      xmlFreeDoc(xml_config);

      if (license)
	xmlFreeDoc(license);
      set_error(err_desc, -ADMIND_ERR_CLUSTER_ALREADY_CREATED,
	        "Trying to create a cluster but"
	        " we already have one ('%s')", adm_cluster.name);
      return;
    }

  /* Start fresh. */
  adm_cache_cleanup();
  adm_cache_create();

  EXA_ASSERT(license != NULL);
  { /*FIXME remove braces and reindent */
    xmlNodePtr license_nptr = xml_conf_xpath_singleton(license, "/Exanodes");
    const char *license_str = NULL;

    if (license_nptr != NULL)
        license_str = xml_get_prop(license_nptr, "license");

    if (license_str == NULL)
    {
      set_error(err_desc, -EINVAL, "License is malformed");
      xmlFreeDoc(license);
      return;
    }

    EXA_ASSERT(exanodes_license == NULL);
    exanodes_license = adm_license_install(license_str, strlen(license_str),
                                           err_desc);
    xmlFreeDoc(license);
    license = NULL;

    if (exanodes_license == NULL)
      return;
  }

  /* from now, license is checked and installed properly */
  EXA_ASSERT(exanodes_license != NULL);

  /* Update our hostname. Must be done *before* the config is parsed: the
   * hostname is used to determine the entry in the config that describes
   * ourself */
  adm_hostname_override(hostname);
  adm_hostname_save(hostname);

  /* Load the config */

  xmlDocDumpMemory(xml_config, &buffer, &size);
  ret = adm_deserialize_from_memory((char *)buffer, size, __error_msg, true /* create */);
  xmlFree(buffer);
  xmlFreeDoc(xml_config);

  if (ret != EXA_SUCCESS)
    {
      set_error(err_desc, ret, "Failed to parse the configuration: %s", __error_msg);
      adm_license_uninstall(exanodes_license);
      exanodes_license = NULL;
      return;
    }

  /* Log this command */
  exalog_info("received clcreate '%s' (" UUID_FMT ") as host '%s' from %s",
	      adm_cluster.name, UUID_VAL(&adm_cluster.uuid), hostname,
	      adm_cli_ip());

#ifdef USE_YAOURT
  yaourt_event_wait(examsgOwner(adm_wt_get_inboxmb(thr_nb)), "cmd_clcreate begin");
#endif

  /* Check the license */
  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  if (!adm_license_nb_nodes_ok(exanodes_license, adm_cluster_nb_nodes(), err_desc))
  {
    adm_cluster_cleanup();
    adm_license_uninstall(exanodes_license);
    exanodes_license = NULL;
    return;
  }

  /* Write the rdev superblocks */

  adm_node_for_each_disk(adm_myself(), disk)
  {
    ret = rdev_initialize_sb(disk->path, &disk->uuid);
    if (ret != EXA_SUCCESS)
    {
      set_error(err_desc, ret, "Unable to write superblock on device %s: %s",
	        disk->path, exa_error_msg(ret));

      /* If we're doing a clcreate from scratch, errors are fatal; else,
       * if join is set, this means we're clnodeadd'ing, and errors are just
       * ignored. Up to him to cldiskdel failed disks later.
       * See bug #4098.
       */
      if (!join)
      {
        adm_cluster_cleanup();
        adm_license_uninstall(exanodes_license);
        exanodes_license = NULL;
        return;
      }
    }
  }

  /* Reset path. They will be found by rdev recovery */

  adm_cluster_for_each_node(node)
    adm_node_for_each_disk(node, disk)
      memset(disk->path, 0, sizeof(disk->path));

  /* Ensure the cluster's goal will be stopped */
  adm_cluster_save_goal(ADM_CLUSTER_GOAL_STOPPED);

  /* Force a config file save. If this is a clnode recover --join, don't
     increment the version number to make sure the config file will be
     overwritten at recovery. */
  if (join)
    ret = conf_save_synchronous_without_inc_version();
  else
    ret = conf_save_synchronous();
  if (ret != EXA_SUCCESS)
    {
      /* FIXME: The config file maybe written on some nodes and should be
         deleted since a command that fails should rollback all side effects. */
      set_error(err_desc, ret, exa_error_msg(ret));
      adm_cluster_cleanup();
      adm_license_uninstall(exanodes_license);
      exanodes_license = NULL;
      return;
    }

  /* everything is initialize we can safely get into ADMIND_STOPPED state
   * waiting for the clinit */
  adm_set_state(ADMIND_STOPPED);

  set_success(err_desc);
}


const AdmCommand exa_clcreate = {
  .code            = EXA_ADM_CLCREATE,
  .msg             = "clcreate",
  .accepted_status = ADMIND_NOCONFIG | ADMIND_STOPPED,
  .match_cl_uuid   = false,
  .cluster_command = cluster_clcreate,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};
