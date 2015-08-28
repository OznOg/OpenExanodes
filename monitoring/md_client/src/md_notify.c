
/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "monitoring/md_client/include/md_notify.h"
#include "monitoring/common/include/md_types.h"

#include "examsg/include/examsg.h"
#include "log/include/log.h"
#include "os/include/os_stdio.h"

#include <string.h>
#include <stdbool.h>


EXAMSG_DCLMSG(md_examsg_event_t, md_msg_event_t event);


static bool md_client_send_trap(ExamsgHandle local_mh , md_msg_event_trap_t* trap)
{
    int ret;
    md_examsg_event_t msg;

    msg.any.type = EXAMSG_MD_EVENT;
    msg.event.type = MD_MSG_EVENT_TRAP;
    msg.event.content.trap = *trap;

    ret = examsgSendNoBlock(local_mh, EXAMSG_MONITORD_EVENT_ID, EXAMSG_LOCALHOST,
			    &msg, sizeof(msg));

    return ret == sizeof(msg);
}



static void md_client_notify_disk_status(ExamsgHandle local_mh,
					 const exa_uuid_t *rdev_uuid,
					 const char *node_name,
					 const char *disk_name,
					 md_disk_status_type_t status)
{
    md_msg_event_trap_t trap;
    trap.type = MD_DISK_TRAP;
    uuid2str(rdev_uuid, trap.id);
    os_snprintf(trap.name, sizeof(trap.name), "%s:%s", node_name, disk_name);
    trap.status = status;

    if (!md_client_send_trap(local_mh, &trap))
    {
	exalog_error("Failed to send disk status trap through examsg.");
    }
}


void md_client_notify_disk_down(ExamsgHandle local_mh,
				const exa_uuid_t *rdev_uuid,
				const char *node_name,
				const char *disk_name)
{
    md_client_notify_disk_status(local_mh, rdev_uuid, node_name, disk_name,
				 MD_DISK_STATUS_DOWN);
}


void md_client_notify_disk_up(ExamsgHandle local_mh,
			      const exa_uuid_t *rdev_uuid,
			      const char *node_name,
			      const char *disk_name)
{
    md_client_notify_disk_status(local_mh, rdev_uuid, node_name, disk_name,
				 MD_DISK_STATUS_UP);
}


void md_client_notify_disk_broken(ExamsgHandle local_mh,
				  const exa_uuid_t *rdev_uuid,
				  const char *node_name,
				  const char *disk_name)
{
    md_client_notify_disk_status(local_mh, rdev_uuid, node_name, disk_name,
				 MD_DISK_STATUS_BROKEN);
}



static void md_client_notify_node_status(ExamsgHandle local_mh,
					 exa_nodeid_t id, /* play uuid role */
					 const char *node_name,
					 md_node_status_type_t status)
{
    md_msg_event_trap_t trap;
    trap.type = MD_NODE_TRAP;
    os_snprintf(trap.id, sizeof(exa_uuid_str_t),
	     "%08X:%08X:%08X:%08X",
	     0, 0, 0, id);
    os_snprintf(trap.name, sizeof(trap.name), "%s", node_name);
    trap.status = status;

    if (!md_client_send_trap(local_mh, &trap))
    {
	exalog_error("Failed to send node status trap through examsg.");
    }
}

void md_client_notify_node_down(ExamsgHandle local_mh,
				exa_nodeid_t id, /* play uuid role */
				const char *node_name)
{
    md_client_notify_node_status(local_mh, id, node_name, MD_NODE_STATUS_DOWN);
}

void md_client_notify_node_up(ExamsgHandle local_mh,
			      exa_nodeid_t id, /* play uuid role */
			      const char *node_name)
{
    md_client_notify_node_status(local_mh, id, node_name, MD_NODE_STATUS_UP);
}



static void md_client_notify_diskgroup_status(ExamsgHandle local_mh,
					      const exa_uuid_t *dg_uuid,
					      const char *dg_name,
					      md_diskgroup_status_type_t status)
{
    md_msg_event_trap_t trap;
    trap.type = MD_DISKGROUP_TRAP;
    uuid2str(dg_uuid, trap.id);
    os_snprintf(trap.name, sizeof(trap.name), "%s", dg_name);

    trap.status = status;

    if (!md_client_send_trap(local_mh, &trap))
    {
	exalog_error("Failed to send diskgroup status trap through examsg.");
    }
}


void md_client_notify_diskgroup_degraded(ExamsgHandle local_mh,
					 const exa_uuid_t *dg_uuid,
					 const char *dg_name)
{
    md_client_notify_diskgroup_status(local_mh, dg_uuid, dg_name,
				      MD_DISKGROUP_STATUS_DEGRADED);
}

void md_client_notify_diskgroup_offline(ExamsgHandle local_mh,
					const exa_uuid_t *dg_uuid,
					const char *dg_name)
{
    md_client_notify_diskgroup_status(local_mh, dg_uuid, dg_name,
				      MD_DISKGROUP_STATUS_OFFLINE);
}

void md_client_notify_diskgroup_ok(ExamsgHandle local_mh,
				   const exa_uuid_t *dg_uuid,
				   const char *dg_name)
{
    md_client_notify_diskgroup_status(local_mh, dg_uuid, dg_name,
				      MD_DISKGROUP_STATUS_OK);
}




static void md_client_notify_filesystem_status(ExamsgHandle local_mh,
					       const exa_uuid_t *vol_uuid,
					       const char *dg_name,
					       const char *vol_name,
					       md_filesystem_status_type_t status)
{
    md_msg_event_trap_t trap;
    trap.type = MD_FILESYSTEM_TRAP;
    uuid2str(vol_uuid, trap.id);
    os_snprintf(trap.name, sizeof(trap.name), "%s:%s", dg_name, vol_name);
    trap.status = status;

    if (!md_client_send_trap(local_mh, &trap))
    {
	exalog_error("Failed to send filesystem status trap through examsg.");
    }
}


void md_client_notify_filesystem_readonly(ExamsgHandle local_mh,
					  const exa_uuid_t *vol_uuid,
					  const char *dg_name,
					  const char *vol_name)
{
    md_client_notify_filesystem_status(local_mh, vol_uuid, dg_name, vol_name,
				       MD_FILESYSTEM_STATUS_READONLY);
}



void md_client_notify_filesystem_offline(ExamsgHandle local_mh,
					 const exa_uuid_t *vol_uuid,
					 const char *dg_name,
					 const char *vol_name)
{
    md_client_notify_filesystem_status(local_mh, vol_uuid, dg_name, vol_name,
				       MD_FILESYSTEM_STATUS_OFFLINE);
}
