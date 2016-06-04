/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <errno.h>

#include "admind/include/service_vrt.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_service.h"
#include "admind/src/adm_volume.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/admindstate.h"
#include "admind/src/rpc.h"
#include "admind/src/saveconf.h"
#include "admind/src/commands/command_api.h"
#include "admind/src/commands/command_common.h"
#include "admind/src/instance.h"
#include "common/include/exa_config.h"
#include "log/include/log.h"
#include "nbd/service/include/nbdservice_client.h"
#include "vrt/virtualiseur/include/vrt_client.h"
#include "os/include/os_disk.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"

__export(EXA_ADM_DGCREATE) struct dgcreate_params
{
    xmlDocPtr config;		/**< XML corresponding to structure dgcreate_info */
    bool alldisks; 	/**< Create the group with all disks available in the cluster */
};


/**
 * The structure used to pass the command parameters from the cluster
 * command to the local command.
 */
struct dgcreate_info
{
    char name[EXA_MAXSIZE_GROUPNAME + 1];
    char layout[EXA_MAXSIZE_LAYOUTNAME + 1];
    exa_uuid_t uuid;
    uint32_t slot_width;
    uint32_t chunk_size;
    uint32_t su_size;
    uint32_t dirty_zone_size;
    uint32_t blended_stripes;
    uint32_t nb_disks;
    uint32_t nb_spare;
    uint32_t pad;
    exa_uuid_t disks[NBMAX_DISKS_PER_GROUP];
};


/**
 * Extracts an unsigned int value from a XML node
 */
static int xml_get_uint_prop(const xmlNodePtr xml_node, const char *prop_name,
                             uint32_t *prop_val, cl_error_desc_t *err_desc)
{
    const char *val_str = xml_get_prop(xml_node, prop_name);
    if (sscanf(val_str, "%u", prop_val) != 1)
    {
        set_error(err_desc, -EXA_ERR_XML_GET,
                  "Inappropriate value '%s' for parameter '%s'", val_str, prop_name);
        return -1;
    }

    return 0;
}

static void
get_info_from_params(const struct dgcreate_params *params,
		     struct dgcreate_info *info,
		     cl_error_desc_t *err_desc)
{
    xmlDocPtr config;
    xmlNodePtr diskgroup_ptr;
    xmlAttrPtr attr;
    int i;

    EXA_ASSERT(params);
    EXA_ASSERT(info);
    EXA_ASSERT(err_desc);

    config = params->config;
    memset(info, 0, sizeof(*info));

    diskgroup_ptr = xml_conf_xpath_singleton(config, "/Exanodes/diskgroup");

    uuid_generate(&info->uuid);
    /* 0 means that the slot width will be automagically computed */
    info->slot_width = 0;
    info->chunk_size = adm_cluster_get_param_int("default_chunk_size");
    info->su_size = adm_cluster_get_param_int("default_su_size");
    info->dirty_zone_size = adm_cluster_get_param_int("default_dirty_zone_size");
    info->blended_stripes = false;
    info->nb_disks = 0;
    info->nb_spare = VRT_DEFAULT_NB_SPARES;
    info->layout[0] = '\0';

    for (attr = diskgroup_ptr->properties; attr != NULL; attr = attr->next)
    {
	if (xmlStrEqual(attr->name, BAD_CAST("name")))
	    strlcpy(info->name, xml_get_prop(diskgroup_ptr, "name"), EXA_MAXSIZE_GROUPNAME + 1);
	else if (xmlStrEqual(attr->name, BAD_CAST("layout")))
	    strlcpy(info->layout, xml_get_prop(diskgroup_ptr, "layout"), EXA_MAXSIZE_LAYOUTNAME + 1);
	else if (xmlStrEqual(attr->name, BAD_CAST("slot_width")))
	{
	    if (xml_get_uint_prop(diskgroup_ptr, "slot_width",
				  &info->slot_width, err_desc) != 0)
		return;
	    /* NOTE User can not give a zero value
	     * If slot_width is not provided, we pass zero
	     * to vrt so that it can calculate the proper slot_width
	     */
	    if (info->slot_width == 0)
	    {
		set_error(err_desc, -EXA_ERR_XML_GET,
			  "slot_width must be greater than zero");
		return;
	    }
	}
	else if (xmlStrEqual(attr->name, BAD_CAST("chunk_size")))
	{
	    if (xml_get_uint_prop(diskgroup_ptr, "chunk_size",
				  &info->chunk_size, err_desc) != 0)
		return;
	}
	else if (xmlStrEqual(attr->name, BAD_CAST("su_size")))
	{
	    if (xml_get_uint_prop(diskgroup_ptr, "su_size",
				  &info->su_size, err_desc) != 0)
		return;
	}
	else if (xmlStrEqual(attr->name, BAD_CAST("dirty_zone_size")))
	{
	    if (xml_get_uint_prop(diskgroup_ptr, "dirty_zone_size",
				  &info->dirty_zone_size, err_desc) != 0)
		return;
	}
	else if (xmlStrEqual(attr->name, BAD_CAST("blended_stripes")))
	{
	    if (xml_get_uint_prop(diskgroup_ptr, "blended_stripes",
				  &info->blended_stripes, err_desc) != 0)
		return;
	}
	else if (xmlStrEqual(attr->name, BAD_CAST("nb_spare")))
	{
	    if (xml_get_uint_prop(diskgroup_ptr, "nb_spare",
				  &info->nb_spare, err_desc) != 0)
		return;
	}
	else if (!xmlStrEqual(attr->name, BAD_CAST("cluster")))
	{
	    set_error(err_desc, -EXA_ERR_XML_GET,
		      "Unknown group property '%s'", (char *)attr->name);
	    return;
	}
    }

    /* Check the group name */
    if (info->name[0] == '\0')
    {
	set_error(err_desc, -EXA_ERR_INVALID_PARAM, NULL);
	return;
    }

    /* Check if a group with that name already exist */
    if (adm_group_get_group_by_name(info->name) != NULL)
    {
	set_error(err_desc, -VRT_ERR_GROUPNAME_USED, NULL);
	return;
    }

    if (info->layout[0] == '\0')
    {
	set_error(err_desc, -EXA_ERR_XML_GET, NULL);
	return;
    }

    if (params->alldisks)
    {
	struct adm_node *node;
	adm_cluster_for_each_node(node)
	{
	    struct adm_disk *disk;
	    adm_node_for_each_disk(node, disk)
	    {
		if (uuid_is_zero(&disk->group_uuid))
                {
                    if (disk->path[0] == '\0')
                    {
                        set_error(err_desc, -ADMIND_ERR_UNKNOWN_DISK,
                              "disk " UUID_FMT " is unknown", UUID_VAL(&disk->uuid));
                              return;
                    }

		    if (info->nb_disks >= NBMAX_DISKS_PER_GROUP)
		    {
			set_error(err_desc, -ADMIND_ERR_TOO_MANY_DISKS_IN_GROUP,
				  "too many disks in group (> %d)", NBMAX_DISKS_PER_GROUP);
			return;
		    }

		    uuid_copy(&info->disks[info->nb_disks], &disk->uuid);
		    info->nb_disks++;
		}
	    }
	}
    }
    else
    {
	xmlNodeSetPtr xmldisk_set;
	xmlNodePtr xmldisk;

	/* Get the list of XML nodes for all the disks of the new group */
	xmldisk_set = xml_conf_xpath_query(config,
					   "/Exanodes/diskgroup[@name='%s']/physical/disk",
					   info->name);

	xml_conf_xpath_result_for_each(xmldisk_set, xmldisk, i)
	{
	    const char *path_prop;
	    const char *node_name;
	    char path[EXA_MAXSIZE_DEVPATH + 1];
	    struct adm_disk *disk;
	    struct adm_node *node;

	    node_name = xml_get_prop(xmldisk, "node");
	    node = adm_cluster_get_node_by_name(node_name);
	    if (!node)
	    {
		set_error(err_desc, -ADMIND_ERR_UNKNOWN_NODENAME,
			  "Node '%s' is not part of the cluster.", node_name);
		xml_conf_xpath_free(xmldisk_set);
		return;
	    }

	    path_prop = xml_get_prop(xmldisk, "path");
	    os_disk_normalize_path(path_prop, path, sizeof(path));

	    exalog_debug("Checking rdev '%s:%s' is not already used", node_name, path);

	    if (info->nb_disks >= NBMAX_DISKS_PER_GROUP)
	    {
		set_error(err_desc, -ADMIND_ERR_TOO_MANY_DISKS_IN_GROUP,
			  "too many disks in group (> %d)", NBMAX_DISKS_PER_GROUP);
		xml_conf_xpath_free(xmldisk_set);
		return;
	    }

	    disk = adm_cluster_get_disk_by_path(node_name, path);
	    if (!disk)
	    {
		set_error(err_desc, -ADMIND_ERR_UNKNOWN_DISK,
			  "disk '%s:%s' is unknown", node_name, path);
		xml_conf_xpath_free(xmldisk_set);
		return;
	    }

	    uuid_copy(&info->disks[info->nb_disks], &disk->uuid);
	    info->nb_disks++;
	}

	xml_conf_xpath_free(xmldisk_set);
    }

    set_success(err_desc);
}

/** dgcreate cluster command
 *
 * The dgcreate cluster commands basically reads the arguments from
 * the XML description of the command, and builds a struct
 * dgcreate_params message used to pass the arguments to the local
 * commands.
 */
static void
cluster_dgcreate(admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc)
{
    struct dgcreate_params *params = data;
    struct dgcreate_info info;
    int error_val;
    vrt_layout_t layout;

    get_info_from_params(params, &info, err_desc);
    xmlFreeDoc(params->config); /* free must be done even if an error occurred. */

    if (err_desc->code != EXA_SUCCESS)
	return;

    exalog_info("received dgcreate '%s' layout=%s, slot_width=%u, chunk_size=%u, "
		"su_size=%u, dirty_zone_size=%u, blended_stripes=%u, nb_spare=%u",
		info.name, info.layout, info.slot_width, info.chunk_size,
		info.su_size, info.dirty_zone_size,
		info.blended_stripes, info.nb_spare);

    /* Check the license status to send warnings/errors */
    cmd_check_license_status();

    layout = vrt_layout_from_name(info.layout);


    if (layout == VRT_LAYOUT_RAINX && !adm_license_has_ha(exanodes_license))
    {
        set_error(err_desc, -ADMIND_ERR_LICENSE, "RainX layout not available "
                    "due to HA being disabled in license");
        return;
    }

    if (info.nb_disks <= 0)
    {
        set_error(err_desc, -VRT_ERR_NO_RDEV_IN_GROUP, NULL);
        return;
    }

    vrt_layout_validate_disk_group_rules(layout, info.disks,
                    info.nb_disks, info.slot_width, info.nb_spare, err_desc);
    if (err_desc->code != EXA_SUCCESS)
    {
	return;
    }

    error_val = admwrk_exec_command(ctx, &adm_service_admin, RPC_ADM_DGCREATE,
				    &info, sizeof(info));

    set_error(err_desc, error_val, NULL);
}


/**
 * Add a group in the in-memory admind configuration.
 *
 * @param[in] info The parameters of the dgcreate command, that
 *                 describe the group
 *
 * @param[out] out_group The group created
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
static int local_exa_dgcreate_config_add(admwrk_ctx_t *ctx, struct dgcreate_info *info,
					 struct adm_group **out_group,
					 char *error_msg)
{
    exa_nodeid_t nodeid;
    exa_uuid_t vrt_uuids[info->nb_disks];
    char buffer[sizeof(vrt_uuids)];
    admwrk_request_t handle;
    struct adm_group *group;
    int ret = EXA_SUCCESS, down_ret = EXA_SUCCESS;
    int i;

    /* FIXME: this method does dangerous things because it mixes temporary object
     * (group allocated here) with pre-existent objects (disks) which are already
     * associated with the cluster. think about a cleaner way to do this.
     */

    /* Generate and broadcast uuids from the leader. */

    if (adm_is_leader())
	for(i = 0; i < info->nb_disks; i++)
	    uuid_generate(&vrt_uuids[i]);

    admwrk_bcast(admwrk_ctx(), &handle, EXAMSG_SERVICE_RDEV_DEAD_INFO,
		 adm_is_leader() ? vrt_uuids : NULL,
		 adm_is_leader() ? sizeof(vrt_uuids) : 0);

    while (admwrk_get_bcast(&handle, &nodeid, buffer, sizeof(buffer), &down_ret))
    {
	/* get data on non leader nodes */
	if (!adm_is_leader()
	    && adm_leader_set
	    && nodeid == adm_leader_id)
	{
	    if (down_ret == EXA_SUCCESS)
		memcpy(vrt_uuids, buffer, sizeof(buffer));

	    ret = down_ret;
	}
    }

    if (ret != EXA_SUCCESS)
	return ret;

    group = adm_group_alloc();
    if (group == NULL)
    {
	exalog_error("adm_group_alloc() failed");
	return -ENOMEM;
    }

    group->sb_version = sb_version_new(&info->uuid);
    if (group->sb_version == NULL)
    {
        adm_group_free(group);
	return -ENOMEM;
    }

    strlcpy(group->name, info->name, sizeof(group->name));
    group->layout = vrt_layout_from_name(info->layout);
    EXA_ASSERT(VRT_LAYOUT_IS_VALID(group->layout));

    uuid_copy(&group->uuid, &info->uuid);
    group->committed = false;
    group->goal = ADM_GROUP_GOAL_STOPPED;

    exalog_debug("insert group %s", group->name);
    ret = adm_group_insert_group(group);
    if (ret != EXA_SUCCESS)
    {
	exalog_error("adm_group_insert_group(): %s", exa_error_msg(ret));
	os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1, "%s", exa_error_msg(ret));
        sb_version_delete(group->sb_version);
	adm_group_free(group);
	return ret;
    }

    for(i = 0; i < info->nb_disks; i++)
    {
	struct adm_disk *disk;
        struct adm_node *disk_node;

	disk = adm_cluster_get_disk_by_uuid(&info->disks[i]);
	if (!disk)
	{
	    exalog_error("Can't find disk " UUID_FMT, UUID_VAL(&info->disks[i]));
	    ret = -EINVAL;
	    break;
	}

        disk_node = adm_cluster_get_node_by_id(disk->node_id);
        EXA_ASSERT(disk_node != NULL);

	if (uuid_is_equal(&disk->group_uuid, &group->uuid))
	{
	    exalog_error("Disk %s:%s used twice when trying to create group %s",
			 disk_node->name, disk->path, group->name);
	    os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1, "Disk %s:%s duplicated in your group config",
			disk_node->name, disk->path);
	    ret = -EINVAL;
	    break;
	}

	if (!uuid_is_zero(&disk->group_uuid))
	{
            struct adm_group *disk_group = adm_group_get_group_by_uuid(&disk->group_uuid);
	    exalog_error("Disk %s:%s already used in group %s when trying to create group %s",
			 disk_node->name, disk->path, disk_group->name, group->name);
	    os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1, "Disk %s:%s already used in group %s",
			disk_node->name, disk->path, disk_group->name);
	    ret = -VRT_ERR_RDEV_ALREADY_USED;
	    break;
	}

	uuid_copy(&disk->vrt_uuid, &vrt_uuids[i]);

	exalog_debug("insert disk %s in group %s", disk->path, group->name);
	ret = adm_group_insert_disk(group, disk);
	if (ret != EXA_SUCCESS)
	{
	    exalog_error("adm_disk_insert(): %s", exa_error_msg(ret));
	    break;
	}
    }

    if (ret != EXA_SUCCESS)
    {
	adm_group_remove_group(group);
	adm_group_cleanup_group(group);
        sb_version_delete(group->sb_version);
	adm_group_free(group);
	group = NULL;
    }

    *out_group = group;
    return ret;
}

/**
 * Create the group inside the virtualizer.
 *
 * @param[in] ctx       Thread number in which we are (needed to send
 *                         and receive messages)
 *
 * @param[in] group        The structure describing the group to create
 *
 * @return EXA_SUCCESS on success, an error code on failure
 */
static int local_exa_dgcreate_vrt_create(admwrk_ctx_t *ctx, struct adm_group *group,
			      struct dgcreate_info *info,
                              char *error_msg, int error_msg_size)
{
    int ret;

    ret = service_vrt_prepare_group(group);
    if (ret != EXA_SUCCESS)
    {
	exalog_error("Failed to prepare group '%s' ("UUID_FMT"): %s (%d)",
		     group->name, UUID_VAL(&group->uuid), exa_error_msg(ret), ret);
	return ret;
    }

    ret = vrt_client_group_create(adm_wt_get_localmb(), &group->uuid,
				  info->slot_width, info->chunk_size, info->su_size,
				  info->dirty_zone_size, info->blended_stripes,
				  info->nb_spare, error_msg);
    if (ret != EXA_SUCCESS)
    {
	exalog_error("Failed to create group '%s' ("UUID_FMT"): %s (%d)",
                     group->name, UUID_VAL(&group->uuid), exa_error_msg(ret), ret);
	return ret;
    }

    return EXA_SUCCESS;
}

/**
 * The dgcreate local command, executed on all nodes.
 */
static void
local_exa_dgcreate (admwrk_ctx_t *ctx, void *msg)
{
    int ret, barrier_ret, rollback_ret;
    struct dgcreate_info *info = msg;
    struct adm_group *group = NULL;
    int force_undo = false;
    char error_msg[EXA_MAXSIZE_LINE + 1] = "";

    EXA_ASSERT(info->nb_disks > 0);
    ret = local_exa_dgcreate_config_add(ctx, info, &group, error_msg);

    barrier_ret = admwrk_barrier_msg(ctx, ret,
				     "Adding diskgroup in the configuration",
				     "%s", error_msg);
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
	goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
	goto undo_diskgroup_add;

    ret = conf_save_synchronous();
    barrier_ret = admwrk_barrier(ctx, ret, "Saving configuration file");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
	goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
	goto undo_xml_save;

    ret = local_exa_dgcreate_vrt_create(ctx, group, info, error_msg, EXA_MAXSIZE_LINE + 1);

    barrier_ret = admwrk_barrier_msg(ctx, ret, "Creating group", "%s", error_msg);
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
	goto metadata_corruption;
    else if (barrier_ret != EXA_SUCCESS)
	goto undo_vrt_create;

    group->committed = true;

    ret = conf_save_synchronous();
    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "%s", exa_error_msg(ret));

    goto local_exa_dgcreate_end;

undo_vrt_create:
    /* Nothing done on non-leader nodes, the failure can only come from
       the leader */
    force_undo = true;

undo_xml_save:
    force_undo = true;

undo_diskgroup_add:
    if (ret == EXA_SUCCESS || force_undo)
    {
	adm_group_remove_group(group);
	adm_group_cleanup_group(group);
	adm_group_free(group);
    }

    /* Always save the configuration file after rollback */
    rollback_ret = conf_save_synchronous();
    barrier_ret = admwrk_barrier(ctx, rollback_ret, "conf_save_synchronous");
    if (barrier_ret == -ADMIND_ERR_NODE_DOWN)
	goto metadata_corruption;

    goto local_exa_dgcreate_end;

metadata_corruption:
    ret = -ADMIND_ERR_METADATA_CORRUPTION;

local_exa_dgcreate_end:
    admwrk_ack(ctx, ret);
}

/**
 * Definition of the dgcreate command.
 */
const AdmCommand exa_dgcreate = {
    .code            = EXA_ADM_DGCREATE,
    .msg             = "dgcreate",
    .accepted_status = ADMIND_STARTED,
    .match_cl_uuid   = true,
    .cluster_command = cluster_dgcreate,
    .local_commands = {
	{ RPC_ADM_DGCREATE, local_exa_dgcreate },
	{ RPC_COMMAND_NULL, NULL }
    }
};
