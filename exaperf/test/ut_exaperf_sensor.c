/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include "os/include/os_inttypes.h"

#include "exaperf/include/exaperf.h"
#include "exaperf/src/exaperf_filter.h"
#include "exaperf/src/exaperf_sensor.h"
#include "exaperf/src/exaperf_core.h"

/**************************************************************/
UT_SECTION(new_free)

ut_test(new_free)
{
    exaperf_err_t err;
    exaperf_sensor_t *sensor;

    sensor = exaperf_sensor_new("SENSOR", ut_printf, &err);
    UT_ASSERT(sensor != NULL && err == EXAPERF_SUCCESS);
    ut_printf("New ok");
    exaperf_sensor_free(sensor);
    ut_printf("free ok");
}

/**************************************************************/
UT_SECTION(sensor_base)

exaperf_err_t err;
exaperf_sensor_t *sensor;

ut_test(init)
{
    sensor = exaperf_sensor_new("SENSOR", ut_printf, &err);
    UT_ASSERT(sensor != NULL && err == EXAPERF_SUCCESS);

    exaperf_sensor_init(sensor, EXAPERF_SENSOR_COUNTER);
    exaperf_sensor_init(sensor, EXAPERF_SENSOR_REPART);
    exaperf_sensor_init(sensor, EXAPERF_SENSOR_DURATION);

    exaperf_sensor_free(sensor);
}

ut_test(set)
{
    int i = 0;
    int nb_values = 10;
    double values = 0;

    sensor = exaperf_sensor_new("SENSOR", ut_printf, &err);
    UT_ASSERT(sensor != NULL && err == EXAPERF_SUCCESS);

    for (i = 0 ; i < nb_values ; i++)
    {
	exaperf_sensor_set(sensor, i * 2.3);
	values += i * 2.3;
    }

    exaperf_sensor_set_multiple(sensor,
				values, nb_values);

    exaperf_sensor_free(sensor);
}

ut_test(lock_unlock)
{
    sensor = exaperf_sensor_new("SENSOR", ut_printf, &err);
    UT_ASSERT(sensor != NULL && err == EXAPERF_SUCCESS);

    exaperf_sensor_lock(sensor);
    exaperf_sensor_unlock(sensor);

    exaperf_sensor_free(sensor);
}

/*************************************************************/
UT_SECTION(param_set)

exaperf_err_t err;
exaperf_sensor_t *sensor;

ut_setup()
{
    sensor = exaperf_sensor_new("SENSOR", ut_printf, &err);
    UT_ASSERT(sensor != NULL && err == EXAPERF_SUCCESS);
}

ut_cleanup()
{
    exaperf_sensor_free(sensor);
}

ut_test(set_sample)
{
    err = exaperf_sensor_param_set(sensor, EXAPERF_PARAM_SAMPLE_SIZE, 100);
    UT_ASSERT(err == EXAPERF_SUCCESS);
}

ut_test(set_sampling_period)
{
    err = exaperf_sensor_param_set(sensor, EXAPERF_PARAM_SAMPLING_PERIOD, 1000);
    UT_ASSERT(err == EXAPERF_SUCCESS);
}

ut_test(set_flushing_period)
{
    err = exaperf_sensor_param_set(sensor, EXAPERF_PARAM_FLUSHING_PERIOD, 10000);
    UT_ASSERT(err == EXAPERF_SUCCESS);
}

ut_test(set_flushing_filter)
{
    err = exaperf_sensor_param_set(sensor, EXAPERF_PARAM_FLUSHING_FILTER, EXAPERF_FILTER_MEAN);
    UT_ASSERT(err == EXAPERF_SUCCESS && sensor->flushing_filter == EXAPERF_FILTER_MEAN);
}

/*************************************************************/
UT_SECTION(name_cmp)

exaperf_err_t err;
exaperf_sensor_t *sensor;

ut_setup()
{
    sensor = exaperf_sensor_new("SENSOR", ut_printf, &err);
    UT_ASSERT(sensor != NULL && err == EXAPERF_SUCCESS);
}

ut_cleanup()
{
    exaperf_sensor_free(sensor);
}


ut_test(name_cmp_ok)
{
    bool eq = exaperf_sensor_name_cmp(sensor, "SENSOR", EXAPERF_MAX_TOKEN_LEN + 1);
    UT_ASSERT(eq);
}

ut_test(name_cmp_nok)
{
    bool eq = exaperf_sensor_name_cmp(sensor, "BADNAME", EXAPERF_MAX_TOKEN_LEN + 1);
    UT_ASSERT(!eq);
}

/*************************************************************/
UT_SECTION(sensor_retrieve)

static exaperf_t *eh = NULL;
static exaperf_sensor_template_t *template = NULL;

ut_setup()
{
    exaperf_err_t err;

    eh = exaperf_alloc();

    err = exaperf_add_sensor_template(eh, "TEMPLATE");
    UT_ASSERT_EQUAL(EXAPERF_SUCCESS, err);

    UT_ASSERT(exaperf_lookup_sensor_template(eh, "TEMPLATE", &template));
    UT_ASSERT_EQUAL_STR("TEMPLATE", template->name);

}

ut_cleanup()
{
    exaperf_free(eh);
}

ut_test(new)
{
    exaperf_sensor_t *sensor;

    UT_ASSERT_VERBOSE(template != NULL, "BAD");

    sensor = exaperf_sensor_retrieve(eh,
				     template->name,
				     "SENSOR2");

    UT_ASSERT(sensor != NULL);
    UT_ASSERT_EQUAL_STR("SENSOR2", sensor->name);
}

ut_test(already_existing)
{
    exaperf_err_t err;
    exaperf_sensor_t *sensor;

    err = exaperf_add_sensor(eh, "SENSOR", template);
    UT_ASSERT(err == EXAPERF_SUCCESS);

    sensor = exaperf_sensor_retrieve(eh,
				     template->name,
				     "SENSOR");

    UT_ASSERT(sensor != NULL);
    UT_ASSERT_EQUAL_STR("SENSOR", sensor->name);
}
