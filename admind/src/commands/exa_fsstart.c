/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_fs.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "os/include/strlcpy.h"
#include "log/include/log.h"
#include "admind/src/commands/exa_fscommon.h"

__export(EXA_ADM_FSSTART) struct fsstart_params
{
  char group_name[EXA_MAXSIZE_GROUPNAME + 1];
  char volume_name[EXA_MAXSIZE_VOLUMENAME + 1];
  char host_list[EXA_MAXSIZE_HOSTSLIST + 1];
  char mountpoint[EXA_MAXSIZE_MOUNTPOINT + 1];
  bool read_only;
};


static int
fs_start_one_fs(int thr_nb, fs_data_t *data_fs,
                const exa_nodeset_t *nodelist, const char *mountpoint,
                uint32_t readonly,
		bool print_warning)
{
  int ret, ret_start;
  const fs_definition_t* fs_definition;
  exa_nodeset_t list_to_start, start_succeeded, list_to_start_read_only;
  exa_nodeset_iter_t iter_nodes;
  exa_nodeid_t current_node;

  exa_nodeset_copy(&list_to_start, nodelist);

  if (!data_fs->transaction)
    return -FS_ERR_INVALID_FILESYSTEM;

  fs_definition = fs_get_definition(data_fs->fstype);
  EXA_ASSERT_VERBOSE(fs_definition, "Unknown file system type found.");

  if (mountpoint[0] != '\0' && strcmp(mountpoint, data_fs->mountpoint))
    {
      if (!exa_nodeset_is_empty(&data_fs->goal_started))
        {
          /* it cannot. This is a case of error */
          ret = -FS_ERR_CHANGE_MOUNTPOINT;
          goto fs_start_one_fs_end;
        }
      else
        {
          /* It can. Conf must be saved with the new value */
          strlcpy(data_fs->mountpoint, mountpoint, sizeof(data_fs->mountpoint));
	  ret = fs_update_tree(thr_nb, data_fs);
	  if (ret != EXA_SUCCESS)
	    goto fs_start_one_fs_end;
        }
    }

  /* Check it can be used and started. Then start it. */
  EXA_ASSERT(fs_definition->check_before_start);
  EXA_ASSERT(fs_definition->start_fs);
  ret = fs_definition->check_before_start(thr_nb, data_fs);
  if (ret != EXA_SUCCESS)
    goto fs_start_one_fs_end;

  /* Remove nodes already started and check the mode of those already started (RO/RW) */
  exa_nodeset_iter_init(&list_to_start, &iter_nodes);
  while (exa_nodeset_iter(&iter_nodes, &current_node))
    {
      if  (exa_nodeset_contains(&data_fs->goal_started, current_node))
	{
	  if (!exa_nodeset_contains(&data_fs->goal_started_ro, current_node) && readonly)
	    {
	      return -FS_ERR_RW_TO_RO;
	    }
	  if (exa_nodeset_contains(&data_fs->goal_started_ro, current_node) && !readonly)
	    {
	      return -FS_ERR_RO_TO_RW;
	    }
	  if (print_warning)
	    {
	      adm_write_inprogress(adm_nodeid_to_name(current_node),
                                   exa_error_msg(-FS_INFO_ALREADY_STARTED),
                                   -FS_INFO_ALREADY_STARTED,
                                   exa_error_msg(-FS_INFO_ALREADY_STARTED));
	    }
	}
    }

  if (exa_nodeset_count(&list_to_start) == 0)
    return EXA_SUCCESS;

  exa_nodeset_reset(&list_to_start_read_only);
  if (readonly)
    {
      exa_nodeset_copy(&list_to_start_read_only, &list_to_start);
      exa_nodeset_reset(&list_to_start);
    }
  ret_start = fs_definition->start_fs(thr_nb, &list_to_start, &list_to_start_read_only,
				      data_fs, &start_succeeded, false);

  /* For each node that is not part of the membership, send a warning */
  if (print_warning)
    {
      adm_warning_node_down(&list_to_start,
	                    "Some nodes are down. The filesystem will be"
			    " started on them at next boot.");
    }

  if (ret_start != EXA_SUCCESS)
    return ret_start;

 fs_start_one_fs_end:
  return ret;
}

int
fs_start_all_fs(int thr_nb, const struct adm_group *group)
{
  int ret = EXA_SUCCESS;
  int overall_ret = EXA_SUCCESS;
  exa_nodeset_t list_to_start_rw;
  fs_iterator_t iter;
  fs_data_t* struct_fs;

  init_fs_iterator(&iter);
  while ((struct_fs = iterate_fs_instance(&iter, true, NULL, true)) != NULL)
    {
      if (iter.fs->volume->group != group)
	continue;

      exa_nodeset_copy(&list_to_start_rw, &struct_fs->goal_started);
      exa_nodeset_substract(&list_to_start_rw, &struct_fs->goal_started_ro);

      if (!exa_nodeset_is_empty(&list_to_start_rw))
	ret = fs_start_one_fs(thr_nb, struct_fs, &list_to_start_rw,
			      struct_fs->mountpoint, false, false);
      if (!exa_nodeset_is_empty(&struct_fs->goal_started_ro))
	ret = fs_start_one_fs(thr_nb, struct_fs, &struct_fs->goal_started_ro,
			      struct_fs->mountpoint, true, false);
      if (ret != EXA_SUCCESS)
	overall_ret = ret;
    }
  return overall_ret;
}

/** \brief Implements the fsstart command
 *
 * - Check the file system transaction parameter. It should be "COMMITTED".
 * - Start it on the specified nodes, using FS specific function.
 * - Update the XML tree for a specific filesystem name, setting nodes goal to started.
 */
static void
cluster_fsstart(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  const struct fsstart_params *params = data;
  exa_nodeset_t list;
  fs_data_t fs;
  int error_val;

  exalog_info("received fsstart '%s:%s' from '%s' on '%s' read-only='%s'",
	      params->group_name, params->volume_name, adm_cli_ip(),
	      params->host_list,
	      params->read_only ? ADMIND_PROP_TRUE:ADMIND_PROP_FALSE);

  /* Check the license status to send warnings/errors */
  cmd_check_license_status();

  /* Get the FS */
  if ((error_val = fscommands_params_get_fs(params->group_name,
	                                    params->volume_name,
					    &fs,
					    NULL,
					    true)) != EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }

  if (params->host_list[0] == '\0')/* An empty hostlist means all nodes */
    adm_nodeset_set_all(&list);
  else
  {
    if ((error_val = adm_nodeset_from_names(&list, params->host_list))
	!= EXA_SUCCESS)
    {
      set_error(err_desc, error_val, NULL);
      return;
    }
  }

  error_val = fs_start_one_fs(thr_nb, &fs, &list, params->mountpoint,
			      params->read_only, true);

  set_error(err_desc, error_val, exa_error_msg(error_val));
}


/**
 * Definition of the exa_fsstart command.
 */
const AdmCommand exa_fsstart = {
  .code            = EXA_ADM_FSSTART,
  .msg             = "fsstart",
  .accepted_status = ADMIND_STARTED,
  .match_cl_uuid   = true,
  .cluster_command = cluster_fsstart,
  .local_commands  = {
    { RPC_COMMAND_NULL, NULL }
  }
};


