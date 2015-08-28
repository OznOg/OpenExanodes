/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <stdlib.h>

#include "os/include/os_mem.h"

#include "common/include/exa_assert.h"

#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"

#include "exaperf/src/exaperf_core.h"
#include "exaperf/src/exaperf_sensor.h"
#include "exaperf/src/exaperf_time.h"
#include "exaperf/src/exaperf_config.h"


/* FIXME Merge exaperf_alloc() into exaperf_init() => no need for the user to
         call two functions when calling one is enough. */
exaperf_t *
exaperf_alloc(void)
{
    exaperf_t *eh;

    eh = (exaperf_t *)os_malloc(sizeof(exaperf_t));
    if (eh == NULL)
	return NULL;

    eh->last = NULL;
    eh->sensors = NULL;
    eh->nb_sensors = 0;

    eh->last_template = NULL;
    eh->sensor_templates = NULL;
    eh->nb_sensor_templates = 0;

    return eh;
}

/* FIXME Return type of print_fct should be int so that printf can be used. */
exaperf_err_t
exaperf_init(exaperf_t *eh, const char *conf_file,
	     void (*print_fct)(const char *fmt, ...))
{
    exaperf_err_t ret;

    if (eh == NULL || conf_file == NULL || print_fct == NULL)
        return EXAPERF_INVALID_PARAM;

    eh->exaperf_print = print_fct;
    ret = exaperf_init_from_file(eh, conf_file);

    return ret;
}

void __exaperf_free(exaperf_t *eh)
{
    exaperf_sensor_t *current_sensor;
    exaperf_sensor_template_t *current_template;
    exaperf_err_t ret;

    if (eh == NULL)
        return;

    while ((current_sensor = exaperf_get_first_sensor(eh)) != NULL)
    {
	ret = exaperf_remove_sensor(eh, current_sensor->name, current_sensor->template->name);
	EXA_ASSERT(ret == EXAPERF_SUCCESS);
    }

    while ((current_template = exaperf_get_first_sensor_template(eh)) != NULL)
    {
	ret = exaperf_remove_sensor_template(eh, current_template->name);
	EXA_ASSERT(ret == EXAPERF_SUCCESS);
    }

    os_free(eh);
}
