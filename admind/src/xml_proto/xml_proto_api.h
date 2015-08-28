/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __XML_PROTO_API_H
#define __XML_PROTO_API_H

#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"
#include "common/include/uuid.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_error.h"

bool xml_buffer_is_partial(const char *buffer);

void xml_command_parse(const char *buffer, adm_command_code_t *cmd_code,
                       exa_uuid_t *cluster_uuid, void **data, size_t *data_size,
		       cl_error_desc_t *err_desc);

char *xml_command_end(const cl_error_desc_t *err_desc);

char *xml_payload_str_new(const char *str);

void xml_inprogress(char *buf, size_t buf_size, const char *src_node,
                    const char *description, const cl_error_desc_t *err_desc);

#endif
