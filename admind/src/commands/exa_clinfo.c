/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>

#include "admind/src/adm_node.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_license.h"
#include "admind/src/adm_nic.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/exa_clinfo_components.h"
#include "admind/src/commands/exa_clinfo_volume.h"
#include "admind/src/commands/exa_clinfo_group.h"
#include "admind/src/commands/exa_clinfo_node.h"
#include "admind/src/commands/exa_clinfo_export.h"
#ifdef WITH_FS
#include "admind/src/commands/exa_clinfo_filesystem.h"
#endif
#ifdef WITH_MONITORING
#include "admind/src/commands/exa_clinfo_monitoring.h"
#endif
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"

__export(EXA_ADM_CLINFO) struct clinfo_params
{
    __optional bool volumes_info     __default(false);
    __optional bool groups_info      __default(false);
    __optional bool disks_info       __default(false);
    __optional bool filesystems_info __default(false);
    __optional bool softwares_info   __default(false);
    __optional bool filesystems_size_info __default(false);
};


static void
cluster_clinfo(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
  const struct clinfo_params *params = data;
  xmlNodePtr exanodes_node;
  xmlNodePtr cluster_node;
  xmlDocPtr doc;
  xmlChar *xmlchar_doc;
  int buf_size;
  int ret = EXA_SUCCESS;
  int in_recovery = evmgr_is_recovery_in_progress();
  adm_license_status_t license_status = adm_license_get_status(exanodes_license);
  uint64_t license_remaining_days = adm_license_get_remaining_days(exanodes_license, true);

  exa_uuid_str_t uuid_str;

  exalog_info("received clinfo%s%s%s%s%s%s from %s",
              params->groups_info ? " groups" : "",
              params->volumes_info ? " volumes" : "",
              params->disks_info ? " realdevices" : "",
              params->filesystems_info ? " filesystems" : "",
              params->filesystems_size_info ? " filesystems(size)" : "",
              params->softwares_info ? " software" : "",
              adm_cli_ip());

  /* Create XML document */

  doc = xmlNewDoc(BAD_CAST("1.0"));
  if (doc == NULL)
  {
    exalog_error("xmlNewDoc() returned NULL");
    ret = -EXA_ERR_XML_INIT;
    goto error;
  }

  exanodes_node = xmlNewNode(NULL, BAD_CAST("Exanodes"));
  if (exanodes_node == NULL)
  {
    exalog_error("xmlNewDocNode() returned NULL");
    ret = -EXA_ERR_XML_INIT;
    goto error;
  }
  xmlDocSetRootElement(doc, exanodes_node);

  cluster_node = xmlNewChild(exanodes_node, NULL, BAD_CAST("cluster"), NULL);
  if (cluster_node == NULL)
  {
    exalog_error("xmlNewChild() returned NULL");
    ret = -EXA_ERR_XML_INIT;
    goto error;
  }

  ret = xml_set_prop(cluster_node, "name", adm_cluster.name);
  if (ret != EXA_SUCCESS)
    goto error;

  os_snprintf(uuid_str, sizeof(uuid_str), UUID_FMT, UUID_VAL(&adm_cluster.uuid));
  ret = xml_set_prop(cluster_node, "uuid", uuid_str);
  if (ret != EXA_SUCCESS)
    goto error;

  /* Get information about the nodes */
  ret = cluster_clinfo_nodes(ctx, cluster_node);
  if (ret != EXA_SUCCESS)
    goto error;

  /* Get information about the data monitoring */
#ifdef WITH_MONITORING
  ret = cluster_clinfo_monitoring(ctx, cluster_node);
  if (ret != EXA_SUCCESS)
    goto error;
#endif

  /* Get information about the data components */
  if (params->softwares_info)
    ret = cluster_clinfo_components(ctx, exanodes_node);
  if (ret != EXA_SUCCESS)
    goto error;

  /* Get information about the data groups and volumes */
  if (params->groups_info || params->volumes_info)
    ret = cluster_clinfo_groups(ctx, exanodes_node, params->disks_info,
				params->volumes_info, params->filesystems_info,
				params->filesystems_size_info);

  if (ret != EXA_SUCCESS)
    goto error;

  if (ret != EXA_SUCCESS)
    goto error;

  /* Add an in_progress property */
  ret = xml_set_prop_bool(cluster_node, "in_recovery",
		      (in_recovery || evmgr_is_recovery_in_progress()));
  if (ret != EXA_SUCCESS)
    goto error;

  ret = xml_set_prop(cluster_node, "license_status", adm_license_status_str(license_status));
  if (ret != EXA_SUCCESS)
    goto error;

  ret = xml_set_prop_u64(cluster_node, "license_remaining_days", license_remaining_days);
  if (ret != EXA_SUCCESS)
    goto error;

  ret = xml_set_prop(cluster_node, "license_type", adm_license_type_str(exanodes_license));
  if (ret != EXA_SUCCESS)
    goto error;

  ret = xml_set_prop_bool(cluster_node, "license_has_ha",
                          adm_license_has_ha(exanodes_license) ? true : false);
  if (ret != EXA_SUCCESS)
    goto error;

  set_success(err_desc);

error:
  /* FIXME a doc is sent even in case of error... is that really handled
   * properly ? */
  set_error(err_desc, ret, exa_error_msg(ret), NULL);
  xmlDocDumpFormatMemory(doc, &xmlchar_doc, &buf_size, 1);

  send_payload_str((char *)xmlchar_doc);

  xmlFree(xmlchar_doc);
  xmlFreeDoc(doc);
}


const AdmCommand exa_clinfo = {
  .code            = EXA_ADM_CLINFO,
  .msg             = "clinfo",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .allowed_in_recovery = true,
  .cluster_command = cluster_clinfo,
  .local_commands  = {
    { RPC_ADM_CLINFO_COMPONENTS,  local_clinfo_components       },
    { RPC_ADM_CLINFO_GROUP_DISK,  local_clinfo_group_disk       },
    { RPC_ADM_CLINFO_NODE_DISKS,  local_clinfo_node_disks       },
    { RPC_ADM_CLINFO_DISK_INFO,   local_clinfo_disk_info        },
    { RPC_ADM_CLINFO_VOLUME,      local_clinfo_volume           },
    { RPC_ADM_CLINFO_EXPORT,      local_clinfo_export           },
    { RPC_ADM_CLINFO_GET_NTH_IQN, local_clinfo_get_nth_iqn	},
#ifdef WITH_FS
    { RPC_ADM_CLINFO_FS,          local_clinfo_fs               },
#endif
#ifdef WITH_MONITORING
    { RPC_ADM_CLINFO_MONITORING,  local_clinfo_monitoring       },
#endif
    { RPC_COMMAND_NULL, NULL }
  }
};
