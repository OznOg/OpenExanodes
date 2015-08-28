/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdlib.h>
#include <errno.h>

#include "os/include/os_stdio.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_string.h"
#include "os/include/strlcpy.h"

#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_tools.h"
#include "exaperf/src/exaperf_config.h"

void exaperf_config_get_err_str(exaperf_config_err_t err,
				char *err_str, size_t err_str_size)
{
    switch(err)
    {
    case EXAPERF_CONFIG_SUCCESS:
	strlcpy(err_str, "success", err_str_size);
	break;

    case EXAPERF_CONFIG_NAME_BAD_FORMAT:
	strlcpy(err_str, "bad parameter name format",err_str_size);
	break;

    case EXAPERF_CONFIG_TOO_LONG_TOKEN:
	strlcpy(err_str, "too long token", err_str_size);
	break;

    case EXAPERF_CONFIG_PARAM_BAD_FORMAT:
	strlcpy(err_str, "bad parameter format", err_str_size);
	break;

    case EXAPERF_CONFIG_PARAM_TOO_MANY_VALUES:
	strlcpy(err_str, "too many values for parameter", err_str_size);
	break;

    case EXAPERF_CONFIG_TOO_SMALL_BUFFER:
	strlcpy(err_str, "too small buffer", err_str_size);
	break;

    default:
	os_snprintf(err_str, err_str_size, "unknown error (%d)", err);
    }
}

exaperf_config_err_t
exaperf_config_is_empty_line(char *str, bool *result)
{
    if (strlen(str) == 0)
	*result = true;
    else
	*result = false;

    return EXAPERF_CONFIG_SUCCESS;
}

exaperf_config_err_t
exaperf_config_is_comment(char *str, bool *result)
{
    if (*str == '#')
    	*result = true;
    else
	*result = false;

    return EXAPERF_CONFIG_SUCCESS;
}

exaperf_config_err_t
exaperf_config_is_template_declaration(const char *str,
				       char *template_name,
				       size_t template_name_size,
				       bool *result)
{
    const char *in_str = str;
    const char *in_str_last = last_character(str);
    char *ptr_in;
    uint32_t size;

    if (*in_str != '[')
    {
	*result = false;
	return EXAPERF_CONFIG_SUCCESS;
    }

    if (*in_str_last != ']')
	return EXAPERF_CONFIG_NAME_BAD_FORMAT;

    /* FIXME: test if TOKEN_TOO_LONG */
    in_str++; /* We don't want to copy the first '[' */
    size = in_str_last - in_str; /* including the '\0' */
    in_str_last--; /* We don't want to copy the ']' */

    if (size <= template_name_size)
    {
	memcpy(template_name, in_str, in_str_last - in_str + 1);
	ptr_in = template_name + size;
	*ptr_in = '\0';
    }
    else
	return EXAPERF_CONFIG_TOO_SMALL_BUFFER;

    *result = true;
    return EXAPERF_CONFIG_SUCCESS;
}

exaperf_config_err_t
exaperf_config_is_param_declaration(char *str,
				    char *key, size_t key_size,
				    char **values, size_t value_size,
				    uint32_t *nb_values, uint32_t nbmax_values)
{
    char *ptr_value, *token;
    char *save_ptr;
    uint32_t nb_v = 0;
    size_t ret;

    token = os_strtok(str, "=", &save_ptr);
    if (token == NULL)
	return EXAPERF_CONFIG_PARAM_BAD_FORMAT;

    remove_whitespace(token);

    if (strlen(token) > EXAPERF_MAX_TOKEN_LEN)
	return EXAPERF_CONFIG_TOO_LONG_TOKEN;

    ret = strlcpy(key, token, key_size);
    if (ret >= key_size)
	return EXAPERF_CONFIG_TOO_SMALL_BUFFER;

    /* XXX Should be rewritten as while (token != NULL) {} */
    do
    {
	token = os_strtok(NULL, " ", &save_ptr);
	if (token == NULL)
	    break;

	if (nb_v > nbmax_values - 1)
	    return EXAPERF_CONFIG_PARAM_TOO_MANY_VALUES;

	remove_whitespace(token);
	if (strlen(token) > EXAPERF_MAX_TOKEN_LEN)
	    return EXAPERF_CONFIG_TOO_LONG_TOKEN;

	ptr_value = values[nb_v];
	ret = strlcpy(ptr_value, token, value_size);
	if (ret >= value_size)
	    return EXAPERF_CONFIG_TOO_SMALL_BUFFER;

	nb_v++;

    } while(true);

    *nb_values = nb_v;

    return EXAPERF_CONFIG_SUCCESS;
}
