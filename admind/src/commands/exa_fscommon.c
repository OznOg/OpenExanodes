/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/commands/command_api.h"

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/instance.h"

#include "admind/services/fs/service_fs.h"

#include "log/include/log.h"

#include "admind/src/commands/exa_fscommon.h"

/** \brief function to print a warning for a list of nodes that are down.
 *
 * @param[in] list_to_check   check the nodes in that list, and print the message for those
 *                            which are not part of the cluster.
 * @param[in] error           error code to return with the warning.
 * @param[in] error_message   error_message code to return with the warning;
 *                            NULL defaults to exa_error_msg(error)
 * \return nothing
 */
void adm_warning_node_down(const exa_nodeset_t *list_to_check,
                           const char *message)
{
  exa_nodeid_t current_node;
  exa_nodeset_t nodes_down;

  if (message == NULL)
    return;

  inst_get_nodes_down(&adm_service_fs, &nodes_down);

  exa_nodeset_intersect(&nodes_down, list_to_check);

  exa_nodeset_foreach(&nodes_down, current_node)
    adm_write_inprogress(adm_nodeid_to_name(current_node), message,
	                 -ADMIND_WARNING_NODE_IS_DOWN, message);
}

/**
 * Get FS from the XML command.
 * \param[in]  group_name
 * \param[in]  volume_name
 * \param[out] fs               filled with data, not NULL!
 * \param[out] fs_definition    filled with defnition , may be NULL
 * \param[out] check_ok         Check everything is valid
 * \param[in]  type_fs          Type of FS awaited
 * return error code if parameters were invalid
 */
int fscommands_params_get_fs(const char *group_name,
                             const char *volume_name,
			     fs_data_t* fs,
			     fs_definition_t **fs_definition,
			     bool check_ok)
{
  struct adm_volume* volume;
  struct adm_group* group;

  group = adm_group_get_group_by_name(group_name);
  if (group == NULL)
    return -ADMIND_ERR_UNKNOWN_GROUPNAME;

  if (group->goal == ADM_GROUP_GOAL_STOPPED)
    return -VRT_ERR_GROUP_NOT_STARTED;

  volume = adm_cluster_get_volume_by_name(group->name, volume_name);
  if (!volume)
    return -ADMIND_ERR_UNKNOWN_FSNAME;

  if (!adm_volume_is_in_fs(volume))
    {
      exalog_error("volume '%s:%s' is not managed by the file system layer",
		   group->name, volume_name);
      return -ADMIND_ERR_UNKNOWN_FSNAME;
    }

  fs_fill_data_from_config(fs, volume->filesystem);

  if (fs_definition)
    {
      *fs_definition = (fs_definition_t *)fs_get_definition(fs->fstype);
    }
  if (check_ok)
    {
      if (!fs->transaction)
	{
	  return -FS_ERR_INVALID_FILESYSTEM;
	}
    }
  return EXA_SUCCESS;
}

