/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include <string.h>
#include "os/include/os_inttypes.h"

#include "os/include/strlcpy.h"
#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_config.h"

/**************************************************************/
UT_SECTION(exaperf_config_empty_line)
char str[EXAPERF_CONFIG_MAX_FILE_LINE_LEN + 1];
exaperf_config_err_t ret;
bool result;

ut_test(empty_line)
{
    strncpy(str, "", sizeof(str));
    ret = exaperf_config_is_empty_line(str, &result);
    UT_ASSERT(ret == EXAPERF_CONFIG_SUCCESS
	      && result == true);
}

ut_test(not_empty_line)
{
    strncpy(str, "hello", sizeof(str));
    ret = exaperf_config_is_empty_line(str, &result);
    UT_ASSERT(ret == EXAPERF_CONFIG_SUCCESS
	      && result == false);
}

/**************************************************************/
UT_SECTION(exaperf_config_empty_line)
char str[EXAPERF_CONFIG_MAX_FILE_LINE_LEN + 1];
exaperf_config_err_t ret;
bool result;

ut_test(no_comment)
{
    strncpy(str, "[SENSOR]", sizeof(str));
    ret = exaperf_config_is_comment(str, &result);
    UT_ASSERT(ret == EXAPERF_CONFIG_SUCCESS
	      && result == false);
}

ut_test(no_comment_empty_line)
{
    strncpy(str, "", sizeof(str));
    ret = exaperf_config_is_comment(str, &result);
    UT_ASSERT(ret == EXAPERF_CONFIG_SUCCESS
	      && result == false);
}

ut_test(a_comment)
{
    strncpy(str, "# comment", sizeof(str));
    ret = exaperf_config_is_comment(str, &result);
    UT_ASSERT(ret == EXAPERF_CONFIG_SUCCESS
	      && result == true);
}

/**************************************************************/
UT_SECTION(exaperf_config_sensor_declaration)
char str[EXAPERF_CONFIG_MAX_FILE_LINE_LEN + 1];
char sensor_name[EXAPERF_MAX_TOKEN_LEN + 1];
exaperf_config_err_t ret;
bool result;

ut_test(not_a_sensor)
{
    strncpy(str, "NOT_A_SENSOR]", sizeof(str));
    ret = exaperf_config_is_template_declaration(str,
						 sensor_name, sizeof(sensor_name),
						 &result);
    UT_ASSERT(ret == EXAPERF_CONFIG_SUCCESS);
    UT_ASSERT(result == false);
}

ut_test(a_valid_sensor)
{
    strncpy(str, "[A_VALID_SENSOR]", EXAPERF_CONFIG_MAX_FILE_LINE_LEN);
    ret = exaperf_config_is_template_declaration(str,
					       sensor_name, sizeof(sensor_name),
					       &result);
    UT_ASSERT(ret == EXAPERF_CONFIG_SUCCESS);
    UT_ASSERT(result == true);
    UT_ASSERT_EQUAL_STR("A_VALID_SENSOR", sensor_name);
}

ut_test(a_bad_formatted_sensor)
{
    strncpy(str, "[A_SENSOR", EXAPERF_CONFIG_MAX_FILE_LINE_LEN);
    ret = exaperf_config_is_template_declaration(str,
					       sensor_name, sizeof(sensor_name),
					       &result);
    UT_ASSERT(ret == EXAPERF_CONFIG_NAME_BAD_FORMAT);
}

/* FIXME: perform the "too_long_sensor_name" test */

/**************************************************************/
UT_SECTION(exaperf_config_param_declaration)

ut_test(param_one_value)
{
    char str[EXAPERF_CONFIG_MAX_FILE_LINE_LEN + 1];
    char key[EXAPERF_MAX_TOKEN_LEN + 1];
    char values_buf[EXAPERF_PARAM_NBMAX_VALUES * (EXAPERF_MAX_TOKEN_LEN + 1)];
    char *values[EXAPERF_PARAM_NBMAX_VALUES];
    int i;
    uint32_t nb_values;
    exaperf_config_err_t ret;

    for (i=0; i<EXAPERF_PARAM_NBMAX_VALUES; i++)
	values[i] = &values_buf[i * (EXAPERF_MAX_TOKEN_LEN + 1)];


    strncpy(str, "param = value", EXAPERF_CONFIG_MAX_FILE_LINE_LEN);

    ret = exaperf_config_is_param_declaration(str, key, sizeof(key),
					      values, EXAPERF_MAX_TOKEN_LEN + 1,
					      &nb_values, EXAPERF_PARAM_NBMAX_VALUES);
    UT_ASSERT(ret == EXAPERF_CONFIG_SUCCESS);
    UT_ASSERT_EQUAL_STR("param", key);
    UT_ASSERT(nb_values == 1);
    UT_ASSERT_EQUAL_STR("value", values[0]);
}

ut_test(param_too_many_values)
{
    char str[EXAPERF_CONFIG_MAX_FILE_LINE_LEN + EXAPERF_MAX_TOKEN_LEN + 1];
    char key[EXAPERF_MAX_TOKEN_LEN + 1];
    /* FIXME: must try this with an array of size max and not max +1 */
    char values_buf[EXAPERF_PARAM_NBMAX_VALUES * (EXAPERF_MAX_TOKEN_LEN + 1)];
    char *values[EXAPERF_PARAM_NBMAX_VALUES];
    int i;
    uint32_t nb_values;
    char c[3] = " a";
    exaperf_config_err_t ret;

    for (i=0; i<EXAPERF_PARAM_NBMAX_VALUES; i++)
	values[i] = &values_buf[i * (EXAPERF_MAX_TOKEN_LEN + 1)];

    strncpy(str, "param =", EXAPERF_CONFIG_MAX_FILE_LINE_LEN);

    for (i = 0; i < EXAPERF_PARAM_NBMAX_VALUES + 1; i++)
    {
	strncat(str, c, EXAPERF_CONFIG_MAX_FILE_LINE_LEN
		+ EXAPERF_MAX_TOKEN_LEN);
    }

    ret = exaperf_config_is_param_declaration(str, key, sizeof(key),
					      values, EXAPERF_MAX_TOKEN_LEN + 1,
					      &nb_values, EXAPERF_PARAM_NBMAX_VALUES);
    UT_ASSERT(ret == EXAPERF_CONFIG_PARAM_TOO_MANY_VALUES);
}

ut_test(param_max_values)
{
    char str[EXAPERF_CONFIG_MAX_FILE_LINE_LEN + 1];
    char key[EXAPERF_MAX_TOKEN_LEN + 1];
    char values_buf[EXAPERF_PARAM_NBMAX_VALUES * (EXAPERF_MAX_TOKEN_LEN + 1)];
    char *values[EXAPERF_PARAM_NBMAX_VALUES];
    int i;
    uint32_t nb_values;
    char c[3] = " a";
    char d[2] = "a";
    exaperf_config_err_t ret;

    for (i=0; i<EXAPERF_PARAM_NBMAX_VALUES; i++)
	values[i] = &values_buf[i * (EXAPERF_MAX_TOKEN_LEN + 1)];

    strncpy(str, "param =", EXAPERF_CONFIG_MAX_FILE_LINE_LEN);

    for (i = 0; i < EXAPERF_PARAM_NBMAX_VALUES; i++)
    {
	strncat(str, c, EXAPERF_CONFIG_MAX_FILE_LINE_LEN);
    }

    ret = exaperf_config_is_param_declaration(str, key, sizeof(key),
					      values, EXAPERF_MAX_TOKEN_LEN + 1,
					      &nb_values, EXAPERF_PARAM_NBMAX_VALUES);
    UT_ASSERT(ret == EXAPERF_CONFIG_SUCCESS);
    UT_ASSERT_EQUAL_STR("param", key);

    for (i = 0; i < nb_values; i++)
    {
	UT_ASSERT_EQUAL_STR(d, values[i]);
    }
}
