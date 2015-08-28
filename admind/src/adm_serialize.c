/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/adm_serialize.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "config/exa_version.h"
#include "common/include/exa_error.h"
#include "common/include/exa_config.h"
#include "vrt/common/include/spof.h"
#include "log/include/log.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nic.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_volume.h"

#ifdef WITH_FS
#include "admind/src/adm_fs.h"
#endif

typedef struct
{
  FILE *file;
  char *buffer;
  int size;
  int create;
  int ret;
} adm_serialize_handle_t;


static void  __attribute__ ((format (printf, 2, 3)))
adm_serialize_printf(adm_serialize_handle_t *handle, const char *fmt, ...)
{
  va_list ap;
  int ret;

  if (handle->ret < 0)
    return;

  va_start(ap, fmt);

  if (handle->file != NULL)
  {
    ret = vfprintf(handle->file, fmt, ap);
  }
  else if (handle->buffer != NULL)
  {
    ret = os_vsnprintf(handle->buffer + handle->ret,
	               handle->size - handle->ret, fmt, ap);
    if (ret >= handle->size)
      ret = -ENOSPC;
  }
  else
  {
    ret = os_vsnprintf(NULL, 0, fmt, ap);
  }

  va_end(ap);

  if (ret < 0)
  {
    exalog_error("xxprintf(%s%s): %s", handle->file != NULL ? "F" : "",
		 handle->buffer != NULL ? "B" : "", exa_error_msg(ret));
    handle->ret = ret;
  }
  else
  {
    handle->ret += ret;
  }
}


static void
adm_serialize(adm_serialize_handle_t *handle)
{
  struct adm_node *node;
  struct adm_group *group;
#ifdef WITH_FS
  struct adm_fs *fs;
#endif
  struct adm_cluster_param *param;

  EXA_ASSERT(adm_cluster.created);

  adm_serialize_printf(handle, "<?xml version=\"1.0\"?>\n");

  adm_serialize_printf(handle, "<Exanodes release=\"%s\"", EXA_VERSION);
  if (!handle->create)
  {
    adm_serialize_printf(handle, " format_version=\"%" PRIu32 "\"",
                         EXA_CONF_FORMAT_VERSION);
    adm_serialize_printf(handle, " config_version=\"%" PRId64 "\"",
                         adm_cluster.version);
  }
  adm_serialize_printf(handle, ">\n");

  /* Cluster */

  adm_serialize_printf(handle, "  <cluster name=\"%s\""
		       " uuid=\"" UUID_FMT "\">\n",
		       adm_cluster.name,
		       UUID_VAL(&adm_cluster.uuid));

  /* Monitoring */
  if (adm_cluster.monitoring_parameters.started)
  {
      adm_serialize_printf(handle, "    <%s %s=\"%s\""
			   " %s=\"%d\""
			   "/>\n",
			   EXA_CONF_MONITORING,
			   EXA_CONF_MONITORING_SNMPD_HOST, adm_cluster.monitoring_parameters.snmpd_host,
			   EXA_CONF_MONITORING_SNMPD_PORT, adm_cluster.monitoring_parameters.snmpd_port
	  );
  }

  /* Nodes */

  adm_cluster_for_each_node(node)
  {
    struct adm_nic *nic = adm_node_get_nic(node);
    struct adm_disk *disk;

    adm_serialize_printf(handle, "    <node name=\"%s\" hostname=\"%s\""
			 " number=\"%d\" spof_id=\"%"PRIspof_id"\">\n",
			 node->name, node->hostname, node->id,
                         adm_node_get_spof_id(node));

    adm_serialize_printf(handle, "      <network hostname=\"%s\"/>\n",
                         adm_nic_get_hostname(nic));

    if (!handle->create)
    {
      adm_node_for_each_disk(node, disk)
      {
        adm_serialize_printf(handle, "      <disk uuid=\"" UUID_FMT "\"/>\n",
			    UUID_VAL(&disk->uuid));
      }
    }

    adm_serialize_printf(handle, "    </node>\n");
  }

  adm_serialize_printf(handle, "  </cluster>\n");

  if (!handle->create)
  {
    adm_group_for_each_group(group)
    {
      struct adm_disk *disk;
      struct adm_volume *volume;

      adm_serialize_printf(handle, "  <diskgroup name=\"%s\""
			  " layout=\"%s\" uuid=\"" UUID_FMT "\""
			  " transaction=\"%s\" goal=\"%s%s\""
			  " tainted=\"%s\">\n",
			  group->name, vrt_layout_get_name(group->layout),
                          UUID_VAL(&group->uuid),
			  group->committed ?
			      ADMIND_PROP_COMMITTED :
			      ADMIND_PROP_INPROGRESS,
			  group->goal == ADM_GROUP_GOAL_STARTED ?
			      ADMIND_PROP_STARTED :
			      "",
			  group->goal == ADM_GROUP_GOAL_STOPPED ?
			      ADMIND_PROP_STOPPED :
			      "",
			  group->tainted ?
			      ADMIND_PROP_TRUE :
			      ADMIND_PROP_FALSE);

      adm_serialize_printf(handle, "    <physical>\n");

      adm_group_for_each_disk (group, disk)
      {
	EXA_ASSERT(disk->node_id != EXA_NODEID_NONE);

	adm_serialize_printf(handle, "      <disk uuid=\"" UUID_FMT "\""
                             " vrt_uuid=\"" UUID_FMT "\"/>\n",
                             UUID_VAL(&disk->uuid), UUID_VAL(&disk->vrt_uuid));
      }

      adm_serialize_printf(handle, "    </physical>\n");

      adm_serialize_printf(handle, "    <logical>\n");

      adm_group_for_each_volume (group, volume)
      {
	adm_serialize_printf(handle, "      <volume name=\"%s\""
			    " uuid=\"" UUID_FMT "\""
			    " size=\"%" PRIu64 "\""
			    " accessmode=\"%s\""
                            " readahead=\"%" PRIu32 "\""
                            " goal_stopped=\"" EXA_NODESET_FMT "\""
			    " goal_started=\"" EXA_NODESET_FMT "\""
			    " goal_readonly=\"" EXA_NODESET_FMT "\""
			    " transaction=\"%s\">\n",
			    volume->name,
			    UUID_VAL(&volume->uuid),
			    volume->size,
			    volume->shared ?
				ADMIND_PROP_SHARED :
				ADMIND_PROP_PRIVATE,
                            volume->readahead,
                            EXA_NODESET_VAL(&volume->goal_stopped),
			    EXA_NODESET_VAL(&volume->goal_started),
			    EXA_NODESET_VAL(&volume->goal_readonly),
			    volume->committed ?
				ADMIND_PROP_COMMITTED :
				ADMIND_PROP_INPROGRESS);
#ifdef WITH_FS
        if (volume->filesystem)
	  {
	    fs = volume->filesystem;
	    adm_serialize_printf(handle, "        <fs size=\"%" PRIu64 "\""
				 " type=\"%s\" mountpoint=\"%s\""
				 " transaction=\"%s\""
				 " mount_option=\"%s\"",
				 fs->size, fs->type, fs->mountpoint,
				 fs->committed ? ADMIND_PROP_COMMITTED : ADMIND_PROP_INPROGRESS,
				 fs->mount_option);
	    if (strcmp(fs->type, "sfs") == 0 && fs->committed)
	      adm_serialize_printf(handle, " sfs_uuid=\"%s\""
				   " sfs_nb_logs=\"%u\""
				   " sfs_readahead=\"%" PRIu64 "\""
				   " sfs_rg_size=\"%" PRIu64 "\""
				   " sfs_fuzzy_statfs=\"%s\""
				   " sfs_demote_secs=\"%u\""
				   " sfs_glock_purge=\"%u\"",
				   fs->gfs_uuid,
				   fs->gfs_nb_logs,
				   fs->gfs_readahead,
				   fs->gfs_rg_size,
				   fs->gfs_fuzzy_statfs ? ADMIND_PROP_TRUE : ADMIND_PROP_FALSE,
				   fs->gfs_demote_secs,
				   fs->gfs_glock_purge);
	    adm_serialize_printf(handle, "/>\n");
	  }
#endif
	adm_serialize_printf(handle, "      </volume>\n");
      }

      adm_serialize_printf(handle, "    </logical>\n");

      adm_serialize_printf(handle, "  </diskgroup>\n");
    }
  }

  adm_serialize_printf(handle, "  <tunables>\n");

  adm_cluster_for_each_param(param)
  {
    adm_serialize_printf(handle, "    <tunable name=\"%s\" default_value=\"%s\"",
			 param->name, param->default_value);
    if (param->set)
      adm_serialize_printf(handle, " value=\"%s\"", param->value);
    adm_serialize_printf(handle, "/>\n");
  }

  adm_serialize_printf(handle, "  </tunables>\n");

  adm_serialize_printf(handle, "</Exanodes>\n");

  if (handle->ret < 0)
    exalog_error("finished with %s", exa_error_msg(handle->ret));
  else
    exalog_debug("%d bytes successfully written", handle->ret);
}


int
adm_serialize_to_memory(char *buffer, int size, int create)
{
  adm_serialize_handle_t handle;

  handle.file   = NULL;
  handle.buffer = buffer;
  handle.size   = size;
  handle.create = create;
  handle.ret    = EXA_SUCCESS;

  adm_serialize(&handle);

  return handle.ret;
}


int
adm_serialize_to_null(int create)
{
  adm_serialize_handle_t handle;

  handle.file   = NULL;
  handle.buffer = NULL;
  handle.size   = 0;
  handle.create = create;
  handle.ret    = EXA_SUCCESS;

  adm_serialize(&handle);

  return handle.ret;
}
