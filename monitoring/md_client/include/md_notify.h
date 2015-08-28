/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_NOTIFY_H__
#define __MD_NOTIFY_H__


#include "common/include/uuid.h"
#include "examsg/include/examsg.h"


void md_client_notify_disk_up(ExamsgHandle local_mh,
			      const exa_uuid_t *rdev_uuid,
			      const char *node_name,
			      const char *disk_name);
void md_client_notify_disk_down(ExamsgHandle local_mh,
				const exa_uuid_t *rdev_uuid,
				const char *node_name,
				const char *disk_name);
void md_client_notify_disk_broken(ExamsgHandle local_mh,
				  const exa_uuid_t *rdev_uuid,
				  const char *node_name,
				  const char *disk_name);

void md_client_notify_node_up(ExamsgHandle local_mh,
			      exa_nodeid_t id, /* play uuid role */
			      const char *node_name);
void md_client_notify_node_down(ExamsgHandle local_mh,
				exa_nodeid_t id, /* play uuid role */
				const char *node_name);


void md_client_notify_diskgroup_degraded(ExamsgHandle local_mh,
					 const exa_uuid_t *dg_uuid,
					 const char *dg_name);
void md_client_notify_diskgroup_offline(ExamsgHandle local_mh,
					const exa_uuid_t *dg_uuid,
					const char *dg_name);
void md_client_notify_diskgroup_ok(ExamsgHandle local_mh,
				   const exa_uuid_t *dg_uuid,
				   const char *dg_name);


void md_client_notify_filesystem_readonly(ExamsgHandle local_mh,
					  const exa_uuid_t *vol_uuid,
					  const char *dg_name,
					  const char *vol_name);
void md_client_notify_filesystem_offline(ExamsgHandle local_mh,
					 const exa_uuid_t *vol_uuid,
					 const char *dg_name,
					 const char *vol_name);



#endif /* __MD_NOTIFY_H__ */
