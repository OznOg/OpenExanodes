/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>

#include "exaperf/src/exaperf_core.h"


exaperf_t *eh;

/*************************************************************/
UT_SECTION(sensor_template)

ut_setup()
{
    eh = exaperf_alloc();
}

ut_cleanup()
{
    exaperf_free(eh);
}

ut_test(remove_unknown_template)
{
    exaperf_err_t ret;

    ret = exaperf_remove_sensor_template(eh, "TEMPLATE");
    UT_ASSERT(ret == EXAPERF_UNDEFINED_SENSOR_TEMPLATE);
}

ut_test(add_lookup_print_remove_two_templates)
{
    exaperf_err_t ret;
    bool b;
    exaperf_sensor_template_t *template = NULL;

    ret = exaperf_add_sensor_template(eh, "TEMPLATE1");
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_add_sensor_template(eh, "TEMPLATE2");
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    b = exaperf_lookup_sensor_template(eh, "TEMPLATE1", &template);
    UT_ASSERT(b == true);
    UT_ASSERT_EQUAL_STR("TEMPLATE1", template->name);

    b = exaperf_lookup_sensor_template(eh, "TEMPLATE2", &template);
    UT_ASSERT(b == true);
    UT_ASSERT_EQUAL_STR("TEMPLATE2", template->name);

    ret = exaperf_remove_sensor_template(eh, "TEMPLATE2");
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    b = exaperf_lookup_sensor_template(eh, "TEMPLATE2", &template);
    UT_ASSERT(b == false);

    b = exaperf_lookup_sensor_template(eh, "TEMPLATE1", &template);
    UT_ASSERT(b == true);
    UT_ASSERT_EQUAL_STR("TEMPLATE1", template->name);

    ret = exaperf_remove_sensor_template(eh, "TEMPLATE1");
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    b = exaperf_lookup_sensor_template(eh, "TEMPLATE1", &template);
    UT_ASSERT(b == false);
}

ut_test(get_first_template)
{
    exaperf_sensor_template_t *template;
    exaperf_err_t ret;

    ret = exaperf_add_sensor_template(eh, "TEMPLATE1");
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_add_sensor_template(eh, "TEMPLATE2");
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    template = exaperf_get_first_sensor_template(eh);
    UT_ASSERT_EQUAL_STR("TEMPLATE1", template->name);

    ret = exaperf_remove_sensor_template(eh, "TEMPLATE1");
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    template = exaperf_get_first_sensor_template(eh);
    UT_ASSERT_EQUAL_STR("TEMPLATE2", template->name);

    ret = exaperf_remove_sensor_template(eh, "TEMPLATE2");
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    template = exaperf_get_first_sensor_template(eh);
    UT_ASSERT(template == NULL);
}

ut_test(add_already_defined_template)
{
    exaperf_err_t ret;

    ret = exaperf_add_sensor_template(eh, "TEMPLATE1");
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_add_sensor_template(eh, "TEMPLATE1");
    UT_ASSERT(ret == EXAPERF_ALREADY_DEFINED_SENSOR_TEMPLATE);
}

ut_test(init_from_file_ok)
{
    exaperf_err_t ret;
    char *srcdir = getenv("srcdir");
    char path[512];

    sprintf(path, "%s/configfiles/config_core.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret == EXAPERF_SUCCESS);
}

ut_test(init_from_file_nok)
{
    exaperf_err_t ret;
    char *srcdir = getenv("srcdir");
    char path[512];

    sprintf(path, "%s/configfiles/config_core_bad.exaperf", srcdir ? srcdir : ".");
    ret = exaperf_init(eh, path, ut_printf);
    UT_ASSERT(ret != EXAPERF_SUCCESS);
}

/*************************************************************/
UT_SECTION(sensor_list)

exaperf_sensor_template_t template = { "TEMPLATE", 0., 1., 1000, 0};

ut_setup()
{
    eh = exaperf_alloc();
    eh->exaperf_print = ut_printf;
}

ut_cleanup()
{
    exaperf_free(eh);
}

ut_test(remove_unknown_sensor)
{
    exaperf_err_t ret;

    ret = exaperf_remove_sensor(eh, "SENSOR", "TEMPLATE");
    UT_ASSERT(ret == EXAPERF_UNDEFINED_SENSOR);
}

ut_test(add_lookup_remove_two_sensors)
{
    exaperf_err_t ret;
    bool b;
    exaperf_sensor_t *sensor = NULL;

    ret = exaperf_add_sensor(eh, "SENSOR1", &template);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_add_sensor(eh, "SENSOR2", &template);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    exaperf_print_sensor_list(eh);

    b = exaperf_lookup_sensor(eh, "SENSOR1", template.name, &sensor);
    UT_ASSERT(b == true);
    UT_ASSERT_EQUAL_STR("SENSOR1", sensor->name);

    b = exaperf_lookup_sensor(eh, "SENSOR2", template.name, &sensor);
    UT_ASSERT(b == true);
    UT_ASSERT_EQUAL_STR("SENSOR2", sensor->name);

    ret = exaperf_remove_sensor(eh, "SENSOR2", template.name);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    b = exaperf_lookup_sensor(eh, "SENSOR2", template.name, &sensor);
    UT_ASSERT(b == false);

    b = exaperf_lookup_sensor(eh, "SENSOR1", template.name, &sensor);
    UT_ASSERT(b == true);
    UT_ASSERT_EQUAL_STR("SENSOR1", sensor->name);

    ret = exaperf_remove_sensor(eh, "SENSOR1", template.name);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    b = exaperf_lookup_sensor(eh, "SENSOR1", template.name, &sensor);
    UT_ASSERT(b == false);
}

ut_test(get_first_sensor)
{
    exaperf_sensor_t *sensor;
    exaperf_err_t ret;

    ret = exaperf_add_sensor(eh, "SENSOR1", &template);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_add_sensor(eh, "SENSOR2", &template);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    sensor = exaperf_get_first_sensor(eh);
    UT_ASSERT_EQUAL_STR("SENSOR1", sensor->name);

    ret = exaperf_remove_sensor(eh, "SENSOR1", template.name);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    sensor = exaperf_get_first_sensor(eh);
    UT_ASSERT_EQUAL_STR("SENSOR2", sensor->name);

    ret = exaperf_remove_sensor(eh, "SENSOR2", template.name);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    sensor = exaperf_get_first_sensor(eh);
    UT_ASSERT(sensor == NULL);
}

ut_test(add_already_defined_sensor)
{
    exaperf_err_t ret;

    ret = exaperf_add_sensor(eh, "SENSOR1", &template);
    UT_ASSERT(ret == EXAPERF_SUCCESS);

    ret = exaperf_add_sensor(eh, "SENSOR1", &template);
    UT_ASSERT(ret == EXAPERF_ALREADY_DEFINED_SENSOR);
}
