/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef EXAPERF_CONFIG_H
#define EXAPERF_CONFIG_H

#include "os/include/os_inttypes.h"

#include "exaperf/include/exaperf_constants.h"

/* The config file line format for a parameter declaration is :
*     param = value1 value2 ... valueN
*
*  N is equal to EXAPERF_PARAM_NBMAX_VALUES
*  Each token length is equal to EXAPERF_MAX_TOKEN_LEN
*
* There is 1 + N tokens + the symbol '=' and several white spaces
* So we arbitrary decides that the maximum length of a line is equal to:
*/
#define EXAPERF_CONFIG_MAX_FILE_LINE_LEN \
    (EXAPERF_MAX_TOKEN_LEN * (EXAPERF_PARAM_NBMAX_VALUES + 2))

#define EXAPERF_CONFIG_MAX_ERROR_MSG_LEN 128

typedef enum
{
    EXAPERF_CONFIG_SUCCESS,
    EXAPERF_CONFIG_NAME_BAD_FORMAT,
    EXAPERF_CONFIG_PARAM_BAD_FORMAT,
    EXAPERF_CONFIG_PARAM_TOO_MANY_VALUES,
    EXAPERF_CONFIG_TOO_LONG_TOKEN,
    EXAPERF_CONFIG_TOO_SMALL_BUFFER
} exaperf_config_err_t;

/**
 * This function tests if the string is an empty line or not. An empty line
 * is defined as a string with only white spaces or line feed.
 *
 * @param[in]  str     the string to parse. It MUST be a standardized
 *                     string. Use the exaperf_tools:remove_whitespace()
 *                     function in order to obtain a standardized string.
 * @param[out] result  true if the string contains only white spaces and line
 *                     feeds, false otherwise
 *
 * @return The only possible return code is EXAPERF_CONFIG_SUCCESS
 */
exaperf_config_err_t
exaperf_config_is_empty_line(char *str, bool *result);

/**
 * This function tests if a string is a comment. A comment is defined as a
 * string which first character is a '#' not including white spaces before.
 *
 * @param[in]  str     the string to parse. It MUST be a standardized
 *                     string. Use the exaperf_tools:remove_whitespace()
 *                     function in order to obtain a standardized string.
 * @param[out] result  true if the string is a comment, false otherwise
 *
 * @return The only possible return code is EXAPERF_CONFIG_SUCCESS
 */
exaperf_config_err_t
exaperf_config_is_comment(char *str, bool *result);

/**
 * This function tests if a string is a sensor template declaration.
 * A sensor template declaration is of the following form:
 *        [SENSOR_TEMPLATE_NAME]
 *
 * If it is a well formed sensor template declaration, this function
 * extract the name of the template: SENSOR_TEMPLATE_NAME (without any
 * embrace).
 *
 * @param[in] str		the string to parse. It MUST be a standardized
 *				string. Use the exaperf_tools:remove_whitespace()
 *				function in order to obtain a standardized string.
 * @param[out] sensor_name	the extracted sensor template name
 * @param[in]  sensor_name_size the size of the buffer used to write the sensor template
 *				name. It should be of size EXAPERF_MAX_TOKEN_LEN + 1.
 * @param[out] result		set to true if the line is a sensor template declaration, set
 *                              to false otherwise. If the result is equal to false,
 *				the sensor template name output parameter is not relevant.
 *
 * @return an error status picked up from the exaperf_config_err_t type. If
 *         the error status is different from EXAPERF_CONFIG_SUCCESS, the
 *         output parameters are not relevant.
 */
exaperf_config_err_t
exaperf_config_is_template_declaration(const char *str,
				       char *template_name,
				       size_t template_name_size,
				       bool *result);


/**
 * This function tests if a string is a parameter declaration. Such a
 * declaration is of the following form:
 *     PARAMETER_NAME = VALUE1 VALUE2 ... VALUEN
 *
 * If the string is a parameter declaration and is well formed, this
 * function extracts the parameter name as the 'key' and the values.
 *
 * @param[in]  str          The string to parse.It MUST be a standardized
 *                          string. Use the
 *                          exaperf_tools:remove_whitespace() function in
 *                          order to obtain a standardized string.
 * @param[out] key          the extracted parameter name
 * @param[in]  key_size     the size of the buffer to store the key
 * @param[out] values       a two dimensionnal array that contains the
 *                          values associated to this parameter.
 * @param[in]  value_size   the size of the buffer allocated to each
 *                          value.
 * @param[out] nb_values    The number of values extracted from the
 *                          string. It cannot be greater than
 *                          EXAPERF_PARAM_NBMAX_VALUES.
 * @param[in] nbmax_values  The number of values that could be extracted in the
 *                          array.
 *
 * @return an error status picked up from the exaperf_config_err_t type. If
 *         the error status is different from EXAPERF_CONFIG_SUCCESS, the
 *         output parameters are not relevant.
 */
exaperf_config_err_t
exaperf_config_is_param_declaration(char *str,
				    char *key, size_t key_size,
				    char **values, size_t value_size,
				    uint32_t *nb_values, uint32_t nbmax_values);

/**
 * This function is used to generate error messages related to the
 * exaperf_config_err_t type.
 *
 * @param[in] err           an error status
 * @param[out] err_str      the associated error message
 * @param[in] err_str_size  the size of the buffer used to store the error
 *                          message
 */
void exaperf_config_get_err_str(exaperf_config_err_t err,
				char *err_str, size_t err_str_size);

#endif
