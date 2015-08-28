/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __CLI_SERVER_H__
#define __CLI_SERVER_H__

#include "common/include/exa_error.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_error.h"

char *cli_get_peername(unsigned int uid);

void cli_server_stop(void);
int cli_server_start(void);

void cli_payload_str(unsigned int uid, const char *string);

void cli_write_inprogress(unsigned int uid, const char *src_node,
                          const char *description, int err,
			  const char *str);
#endif
