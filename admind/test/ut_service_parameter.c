/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/** \file ut_service_parameter.c
 *  \brief This is the test of the service parameter API for Exanodes.
 */

#include <stdio.h>
#include <string.h>

#include "admind/src/service_parameter.h"
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"

#include <unit_testing.h>

UT_SECTION(exa_service_parameter_get)

ut_test(exa_service_parameter_get_NULL_returns_NULL)
{
    UT_ASSERT(exa_service_parameter_get(NULL) == NULL);
}

ut_test(exa_service_parameter_get_unknown_returns_NULL)
{
    UT_ASSERT(exa_service_parameter_get("qsdfkmqksdjfmlskf") == NULL);
}

ut_test(exa_service_parameter_get_default)
{
    const char *str;

    str = exa_service_parameter_get_default("alive_timeout");
    UT_ASSERT(!strcmp(str, "5"));
    str = exa_service_parameter_get_default("scheduler");
    UT_ASSERT(!strcmp(str, "fifo"));

}

ut_test(exa_service_parameter_get_list)
{
    int iterator = 0;
    exa_service_parameter_t *service_param;
    int found = 0;

    while ((service_param = exa_service_parameter_get_list(&iterator)) != NULL)
    {
	found++;
        UT_ASSERT(service_param->name != NULL);
        UT_ASSERT(service_param->description != NULL);
    }

    UT_ASSERT(found > 0);
}

UT_SECTION(exa_service_parameter_check)

ut_test(exa_service_parameter_check_invalid_int_returns_INVALID_VALUE)
{
    exa_service_parameter_t param =
    {
        .name = "dummy",
        .type = EXA_PARAM_TYPE_INT,
        .description = "nothing",
        .choices = { NULL },
        .min = 10,
        .max = 25,
        .default_value = "13"
    };
    const char *value = "5toto";

    UT_ASSERT(exa_service_parameter_check(&param, value)
              == -EXA_ERR_INVALID_VALUE);
}

ut_test(exa_service_parameter_check_int_outofbounds_returns_INVALID_VALUE)
{
    exa_service_parameter_t param =
    {
        .name = "dummy",
        .type = EXA_PARAM_TYPE_INT,
        .description = "nothing",
        .min = 10,
        .max = 25,
        .default_value = "13"
    };
    const char *value = "5";

    UT_ASSERT(exa_service_parameter_check(&param, value)
              == -EXA_ERR_INVALID_VALUE);
}

ut_test(exa_service_parameter_check_valid_int_returns_SUCCESS)
{
    exa_service_parameter_t param =
    {
        .name = "dummy",
        .type = EXA_PARAM_TYPE_INT,
        .description = "nothing",
        .min = 10,
        .max = 25,
        .default_value = "13"
    };
    const char *value = "21";

    UT_ASSERT(exa_service_parameter_check(&param, value)
              == EXA_SUCCESS);
}

ut_test(exa_service_parameter_check_invalid_bool_returns_INVALID_VALUE)
{
    exa_service_parameter_t param =
    {
        .name = "dummy",
        .type = EXA_PARAM_TYPE_BOOLEAN,
        .description = "nothing",
        .default_value = "FALSE"
    };
    const char *value = "TOTO";

    UT_ASSERT(exa_service_parameter_check(&param, value)
              == -EXA_ERR_INVALID_VALUE);
}

ut_test(exa_service_parameter_check_text_returns_SUCCESS)
{
    exa_service_parameter_t param =
    {
        .name = "dummy",
        .type = EXA_PARAM_TYPE_TEXT,
        .description = "nothing",
        .default_value = NULL
    };
    const char *value = "bonjour chez vous";

    UT_ASSERT(exa_service_parameter_check(&param, value)
              == EXA_SUCCESS);
}


ut_test(exa_service_parameter_check_invalid_list_returns_INVALID_VALUE)
{
    exa_service_parameter_t param =
    {
        .name = "dummy",
        .type = EXA_PARAM_TYPE_LIST,
        .description = "nothing",
        .choices = { "one", "two", "three", NULL },
        .default_value = NULL
    };
    const char *value = "some random text";

    UT_ASSERT(exa_service_parameter_check(&param, value)
              == -EXA_ERR_INVALID_VALUE);
}

ut_test(exa_service_parameter_check_valid_list_returns_SUCCESS)
{
    exa_service_parameter_t param =
    {
        .name = "dummy",
        .type = EXA_PARAM_TYPE_LIST,
        .description = "nothing",
        .choices = { "one", "two", "three", NULL },
        .default_value = NULL
    };
    const char *value = "two";

    UT_ASSERT(exa_service_parameter_check(&param, value)
              == EXA_SUCCESS);
}

/* ut_test(exa_service_parameter_check_invalid_nodelist_returns_INVALID_VALUE) */
/* { */
/*     ut_printf("Can't unit test: depends on adm_cluster side effects"); */
/*     UT_FAIL(); */
/* } */

/* ut_test(exa_service_parameter_check_valid_nodelist_returns_SUCCESS) */
/* { */
/*     ut_printf("Can't unit test: depends on adm_cluster side effects"); */
/*     UT_FAIL(); */
/* } */

ut_test(exa_service_parameter_check_invalid_ipaddress_returns_INVALID_VALUE)
{
    exa_service_parameter_t param =
    {
        .name = "dummy",
        .type = EXA_PARAM_TYPE_IPADDRESS,
        .description = "nothing",
        .default_value = NULL
    };
    const char *value = "192.168.0.42toto";

    UT_ASSERT(exa_service_parameter_check(&param, value)
              == -EXA_ERR_INVALID_VALUE);
}

ut_test(exa_service_parameter_check_valid_ipaddress_returns_SUCCESS)
{
    exa_service_parameter_t param =
    {
        .name = "dummy",
        .type = EXA_PARAM_TYPE_IPADDRESS,
        .description = "nothing",
        .default_value = NULL
    };
    const char *value = "192.168.0.42";

    UT_ASSERT(exa_service_parameter_check(&param, value)
              == EXA_SUCCESS);
}
