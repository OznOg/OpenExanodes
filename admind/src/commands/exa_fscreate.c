/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>
#include <errno.h>

#include "log/include/log.h"
#include "admind/src/commands/exa_fscommon.h"
#include "admind/include/service_lum.h"
#include "admind/include/service_vrt.h"
#include "admind/src/adm_fs.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_service.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "admind/src/instance.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/strlcpy.h"
#include "vrt/virtualiseur/include/vrt_client.h"
#include "lum/export/include/export.h"

#ifdef USE_YAOURT
#include <yaourt/yaourt.h>
#endif

__export(EXA_ADM_FSCREATE) struct fscreate_params
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  char mountpoint[EXA_MAXSIZE_DEVPATH + 1];
  char type[EXA_MAXSIZE_FSTYPE + 1];
  __optional int32_t sfs_nb_logs __default(-1);
  uint64_t sizeKB;
  uint64_t rg_sizeM;
};


/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct fscreate_info
{
    fs_data_t fs;
    exa_nodeid_t nodeid;
};

static exa_error_code
pre_local_create_fs(int thr_nb, fs_data_t *fs);

/** \brief Implements the fscreate command
 *
 * - Set nodes started and stopped.
 * - Set nodes mounted and unmounted.
 * - Add it to the config tree with status NOK
 * - This command runs the FS-specific check and create command: creation
 *   of new volumes is done through specific FS function
 * - Update status to OK in the config file.
 */
static void cluster_fscreate(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  struct fscreate_params *fs_param = data;
  int error_val = EXA_SUCCESS, error_delete;
  struct adm_volume *volume;
  struct adm_group  *group;
  fs_data_t new_fs;
  const fs_definition_t *fs_definition;
  struct vrt_volume_info volume_info;

  exalog_info("received fscreate '%s:%s' "
              "--mountpoint='%s' --type='%s' --size=%" PRIu64
	      " --rg-size=%" PRIu64, fs_param->group_name, fs_param->volume_name,
	      fs_param->mountpoint, fs_param->type, fs_param->sizeKB,
	      fs_param->rg_sizeM);

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  /* This is a workaround for bug #4600. */
  if (fs_param->sizeKB % 4 != 0)
  {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM, "Size must be a multiple of 4KiB");
      return;
  }

  group = adm_group_get_group_by_name(fs_param->group_name);
  if (group == NULL)
    {
      set_error(err_desc, -ADMIND_ERR_UNKNOWN_GROUPNAME,
                NULL, fs_param->group_name);
      return;
    }

  /* The volume MUST NOT exist */
  if (adm_cluster_get_volume_by_name(group->name, fs_param->volume_name))
    {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM,
	        "A file system or a volume with this name already exists");
      return;
    }

  fs_definition = fs_get_definition(fs_param->type);
  if (!fs_definition)
    {
      set_error(err_desc, -EXA_ERR_INVALID_PARAM, "Unknown file system type");
      return;
    }

  /* FIXME use cluster_vlcreate in place of vrt_master_volume_create */
  error_val = vrt_master_volume_create(thr_nb,
                                       group,
				       fs_param->volume_name,
                                       EXPORT_BDEV,
                                       fs_param->sizeKB,
                                       fs_definition->is_volume_private(),
				       adm_cluster_get_param_int("default_readahead"));

  if (error_val != EXA_SUCCESS)
      goto volume_delete;

  /* get the newly create volume */
  volume = adm_group_get_volume_by_name(group, fs_param->volume_name);

  /* Ask the vrt for the real size because "-s max" uses a size of "0" */
  if ((error_val = vrt_client_volume_info(adm_wt_get_localmb(),
                                          &group->uuid,
                                          &volume->uuid,
                                          &volume_info)) != EXA_SUCCESS)
    {
      goto volume_delete;
    }

  /* fill the structure with informations retrieved from the vrt */
  memset(&new_fs, 0, sizeof(new_fs));

  strlcpy(new_fs.fstype, fs_param->type, sizeof(new_fs.fstype));
  strlcpy(new_fs.mountpoint, fs_param->mountpoint, sizeof(new_fs.mountpoint));

  new_fs.sizeKB      = volume_info.size;
  new_fs.volume_uuid = volume->uuid;
  new_fs.transaction = 0;

  adm_volume_path(new_fs.devpath, sizeof(new_fs.devpath),
                  group->name, volume->name);

  /* Parse the filesystem-specific command parameters */
  if (fs_definition->parse_fscreate_parameters)
    {
      error_val = fs_definition->parse_fscreate_parameters(&new_fs,
							   fs_param->sfs_nb_logs,
							   fs_param->rg_sizeM);
      if (error_val != EXA_SUCCESS)
	goto volume_delete;
    }

  /* Write to tree, with INPROGRESS status */
  error_val = fs_update_tree(thr_nb, &new_fs);
  if (error_val != EXA_SUCCESS)
    goto volume_delete;

  exalog_debug("Set FS tree to NOK successfully");

    /* Perform clustered preparation specific to fs type */
    if (fs_definition->pre_create_fs)
    {
        error_val = fs_definition->pre_create_fs(thr_nb, &new_fs);
        if (error_val != EXA_SUCCESS)
            goto volume_delete;
    }

  /* Really create */
  error_val = pre_local_create_fs(thr_nb, &new_fs);

  if (error_val != EXA_SUCCESS)
    goto volume_delete;

  exalog_debug("Created FS successfully");

  /* Set transaction to COMMITTED */
  new_fs.transaction = 1;

  error_val = fs_update_tree(thr_nb, &new_fs);
  if (error_val != EXA_SUCCESS)
    goto volume_delete;

  exalog_debug("Set FS tree to COMMITTED successfully");

  set_success(err_desc);
  return;

volume_delete:
  /* retrieve the volume the was supposed to be created */
  volume = adm_group_get_volume_by_name(group, fs_param->volume_name);

  if (error_val == -ADMIND_ERR_NODE_DOWN ||
      error_val == -ADMIND_ERR_METADATA_CORRUPTION ||
      !volume)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }
  /* Try to remove the volume in the config file.  If this fails
   * for any reason, silently ignore the error: the FS is rolled
   * back to an invalid status and cannot be started. The only way
   * out is to fsdelete it.
   */
  error_delete = vrt_master_volume_delete(thr_nb, volume, false);
  if (error_delete != EXA_SUCCESS)
    {
      exalog_error("Cannot delete file system: %s", exa_error_msg(error_delete));
    }

  set_error(err_desc, error_val, NULL);
  return;
}


/**
 * Find a node to start the fs volume,
 * and then run the local command on that node to create the fs.
 */
static exa_error_code
pre_local_create_fs(int thr_nb, fs_data_t *fs)
{
    int ret = EXA_SUCCESS;
    int error_val;
    struct fscreate_info info;
    struct adm_volume *volume;
    exa_nodeset_iter_t iter_nodes;
    exa_nodeid_t node_id;
    exa_nodeset_t nodelist;

    memcpy(&info.fs, fs, sizeof(info.fs));

    volume = fs_get_volume(&info.fs);

    /* TODO FIXME Use adm_service_fs instead of adm_service_vrt? */
    inst_get_nodes_up(&adm_service_vrt, &iter_nodes);

    EXA_ASSERT(exa_nodeset_count(&iter_nodes) > 0);

    /* Try to start the data volume on one node */
    while (exa_nodeset_iter(&iter_nodes, &node_id))
    {
        exa_nodeset_reset(&nodelist);
        exa_nodeset_add(&nodelist, node_id);

        ret = vrt_master_volume_start(thr_nb, volume, &nodelist,
                                      false /* readonly */,
                                      false /* print_warning */);
        if (ret == EXA_SUCCESS)
        {
            info.nodeid = node_id;
            /* TODO FIXME Use adm_service_fs instead of adm_service_vrt? */
            error_val =
                admwrk_exec_command(thr_nb, &adm_service_vrt,
                            RPC_ADM_FSCREATE, &info, sizeof(info));

            ret = lum_master_export_unpublish(thr_nb, &volume->uuid, &nodelist, false);
            if (ret == EXA_SUCCESS)
                ret = vrt_master_volume_stop(thr_nb, volume, &nodelist,
                                             false /* force */,
                                             ADM_GOAL_CHANGE_VOLUME,
                                             false /* print_warning */);

            return error_val ? error_val : ret;
        }
        else
        {
            /* TODO FIXME This is just a workaround
               for the problem of too much volumes started */
            vrt_master_volume_stop(thr_nb, volume, &nodelist,
                                false /* force */,
                                ADM_GOAL_CHANGE_VOLUME,
                                false /* print_warning */);
        }
    }

    return ret;
}


static void
local_exa_fscreate(int thr_nb, void *msg)
{
    int ret = EXA_SUCCESS;
    struct fscreate_info *info = msg;
    const fs_definition_t *fs_definition;

    if (info->nodeid != adm_my_id)
    {
        ret = -ADMIND_ERR_NOTHINGTODO;
        goto local_exa_fscreate_end;
    }

    fs_definition = fs_get_definition(info->fs.fstype);
    if (!fs_definition)
    {
        ret = -EXA_ERR_INVALID_PARAM;
        goto local_exa_fscreate_end;
    }

    EXA_ASSERT(fs_definition->create_fs);
    ret = fs_definition->create_fs(thr_nb, &info->fs);

local_exa_fscreate_end:
    exalog_debug("local_exa_fscreate() = %s", exa_error_msg(ret));
    admwrk_ack(thr_nb, ret);
}


/**
 * Definition of the fscreate command.
 */
const AdmCommand exa_fscreate = {
  .code            = EXA_ADM_FSCREATE,
  .msg             = "fscreate",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_fscreate,
  .local_commands  = {
    { RPC_ADM_FSCREATE, local_exa_fscreate },
    { RPC_COMMAND_NULL, NULL }
  }
};
