/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_CONTROL_H__
#define __MD_CONTROL_H__


#include "monitoring/common/include/md_types.h"
#include "common/include/exa_error.h"
#include "examsg/include/examsg.h"
#include <stdbool.h>


exa_error_code md_client_control_start(ExamsgHandle local_mh,
			     exa_nodeid_t node_id,
			     const char *node_name,
			     const char *master_agentx_host,
			     uint32_t master_agentx_port);

exa_error_code md_client_control_status(ExamsgHandle local_mh,
			      md_service_status_t *status);

exa_error_code md_client_control_stop(ExamsgHandle local_mh);



#endif /* __MD_CONTROL_H__ */
