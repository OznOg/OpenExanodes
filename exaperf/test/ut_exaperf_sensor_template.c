/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "exaperf/include/exaperf.h"
#include "exaperf/src/exaperf_sensor_template.h"

/*************************************************************/
UT_SECTION(str2param)

exaperf_err_t err;
exaperf_sensor_param_t param;

ut_test(params_ok)
{
    err = exaperf_sensor_str2param("flushing_period", EXAPERF_MAX_TOKEN_LEN + 1,
				   &param);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(param == EXAPERF_PARAM_FLUSHING_PERIOD);

    err = exaperf_sensor_str2param("sampling_period", EXAPERF_MAX_TOKEN_LEN + 1,
				   &param);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(param == EXAPERF_PARAM_SAMPLING_PERIOD);

    err = exaperf_sensor_str2param("sample_size", EXAPERF_MAX_TOKEN_LEN + 1,
				   &param);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(param == EXAPERF_PARAM_SAMPLE_SIZE);

    err = exaperf_sensor_str2param("flushing_filter", EXAPERF_MAX_TOKEN_LEN + 1,
				   &param);
    UT_ASSERT(err == EXAPERF_SUCCESS);
    UT_ASSERT(param == EXAPERF_PARAM_FLUSHING_FILTER);
}

ut_test(param_nok)
{
    err = exaperf_sensor_str2param("flushing_p", EXAPERF_MAX_TOKEN_LEN + 1,
				   &param);
    UT_ASSERT(err == EXAPERF_INVALID_PARAM);
}

/*************************************************************/
UT_SECTION(template_new_free)

exaperf_err_t err;
exaperf_sensor_template_t *template;
bool ret;

ut_test(new_free_ok)
{
    template = exaperf_sensor_template_new("TEMPLATE", &err);
    UT_ASSERT(template != NULL && err == EXAPERF_SUCCESS);
    exaperf_sensor_template_free(template);
}

/*************************************************************/
UT_SECTION(template_name_cmp)

exaperf_err_t err;
exaperf_sensor_template_t *template;
bool ret;

ut_setup()
{
    template = exaperf_sensor_template_new("TEMPLATE", &err);
    UT_ASSERT(template != NULL && err == EXAPERF_SUCCESS);
}

ut_cleanup()
{
    exaperf_sensor_template_free(template);
}

ut_test(name_cmp_ok)
{
    ret = exaperf_sensor_template_name_cmp(template, "TEMPLATE", EXAPERF_MAX_TOKEN_LEN + 1);
    UT_ASSERT(ret == true);
}

ut_test(name_cmp_nok)
{
    ret = exaperf_sensor_template_name_cmp(template, "BADNAME", EXAPERF_MAX_TOKEN_LEN + 1);
    UT_ASSERT(ret == false);
}

/*************************************************************/
UT_SECTION(template_param_set)

exaperf_sensor_template_t *template;

ut_setup()
{
    template = exaperf_sensor_template_new("TEMPLATE", &err);
    UT_ASSERT(template != NULL && err == EXAPERF_SUCCESS);
}

ut_cleanup()
{
    exaperf_sensor_template_free(template);
}

ut_test(param_set)
{
    exaperf_err_t ret;

    ret = exaperf_sensor_template_param_set(template,
					    EXAPERF_PARAM_FLUSHING_PERIOD,
					    10);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_sensor_template_param_set(template,
					    EXAPERF_PARAM_SAMPLING_PERIOD,
					    1000);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_sensor_template_param_set(template,
					    EXAPERF_PARAM_SAMPLE_SIZE,
					    1000);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_sensor_template_param_set(template,
					    EXAPERF_PARAM_FLUSHING_FILTER,
					    10);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_sensor_template_param_set(template,
					    EXAPERF_PARAM_NONE,
					    10);
    UT_ASSERT(ret == EXAPERF_INVALID_PARAM);
}

