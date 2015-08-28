/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __COMMAND_COMMON_H__
#define __COMMAND_COMMON_H__

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_error.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_license.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_workthread.h"
#include "common/include/exa_error.h"
#include "log/include/log.h"

/** @brief Common treatment of the license status
 *
 *  @param[out] err_desc  Error descriptor used by the commands
 *                        if null use 'adm_write_inprogress' to signal the problem
 *
 *  @return true if the check is successful
 */
static inline void cmd_check_license_status(void)
{
    bool error = false;
    bool warning = false;
    char msg[EXA_MAXSIZE_ERR_MESSAGE + 1];
    adm_license_status_t status = adm_license_get_status(exanodes_license);
    uint32_t remaining_days;

    switch(status)
    {
    case ADM_LICENSE_STATUS_NONE:
	os_snprintf(msg, sizeof(msg), "No license available for Exanodes");
	error = true;
	break;
    case ADM_LICENSE_STATUS_EXPIRED:
	os_snprintf(msg, sizeof(msg), "Exanodes' license has expired");
	error = true;
	break;
    case ADM_LICENSE_STATUS_GRACE:
	remaining_days = adm_license_get_remaining_days(exanodes_license, true);
	os_snprintf(msg, sizeof(msg), "Exanodes' license has recently expired. "
		    "The service will be interrupted in %u %s.",
		   remaining_days, remaining_days > 1 ? "days":"day" );
	warning = true;
	break;
    case ADM_LICENSE_STATUS_EVALUATION:
	remaining_days = adm_license_get_remaining_days(exanodes_license, false);
	os_snprintf(msg, sizeof(msg), "Exanodes is in evaluation mode. "
		    "The service will be interrupted in %u %s.",
		    remaining_days, remaining_days > 1 ? "days":"day" );
	warning = true;
	break;
    case ADM_LICENSE_STATUS_OK:
	break;
    default:
	EXA_ASSERT_VERBOSE(false, "adm_license_get_status returned unexpected status %i", status);
    }

    if (error)
    {
	exalog_error("%s", msg);
	adm_write_inprogress(adm_nodeid_to_name(adm_myself()->id),
			     "Checking license status",
			     -ADMIND_ERR_LICENSE, msg);
    }
    else if (warning)
    {
	exalog_warning("%s", msg);
	adm_write_inprogress(adm_nodeid_to_name(adm_myself()->id),
			     "Checking license status",
			     -ADMIND_WARN_LICENSE, msg);
    }

}

#endif
