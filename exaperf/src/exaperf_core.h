/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef EXAPERF_CORE_H
#define EXAPERF_CORE_H

#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_sensor.h"
#include "exaperf/src/exaperf_sensor_template.h"

#ifdef WIN32
#define exaperf_log(eh, fmt, ...)			\
    (eh)->exaperf_print(exaperf_log_prefix fmt, __VA_ARGS__)
#else
#define exaperf_log(eh, fmt, ...)			\
    (eh)->exaperf_print(exaperf_log_prefix fmt, ## __VA_ARGS__)
#endif

/* XXX Badly named. Should be exaperf_template_list_item. */
struct exaperf_list_sensor_template
{
    exaperf_sensor_template_t *sensor_template;
    struct exaperf_list_sensor_template *next;
    struct exaperf_list_sensor_template *previous;
};

/* XXX Badly named. Should be exaperf_sensor_list_item. */
struct exaperf_list_sensor
{
    exaperf_sensor_t *sensor;
    struct exaperf_list_sensor *next;
    struct exaperf_list_sensor *previous;
};

typedef struct exaperf_list_sensor exaperf_list_sensor_t;
typedef struct exaperf_list_sensor_template exaperf_list_sensor_template_t;

struct exaperf
{
    uint32_t nb_sensors;
    exaperf_list_sensor_t *sensors;
    exaperf_list_sensor_t *last;
    uint32_t nb_sensor_templates;
    exaperf_list_sensor_template_t *sensor_templates;
    exaperf_list_sensor_template_t *last_template;
    void (*exaperf_print)(const char *fmt, ...);
};

exaperf_err_t exaperf_add_sensor_template(exaperf_t *eh,
					  const char *name);

exaperf_err_t exaperf_remove_sensor_template(exaperf_t *eh,
					     const char *name);

bool exaperf_lookup_sensor_template(exaperf_t *eh,
				    const char *name,
				    exaperf_sensor_template_t **sensor_template);

exaperf_sensor_template_t *exaperf_get_first_sensor_template(exaperf_t *eh);

exaperf_err_t exaperf_add_sensor(exaperf_t *eh,
				 const char *name,
				 exaperf_sensor_template_t *template);

exaperf_err_t exaperf_remove_sensor(exaperf_t *eh,
				    const char *name,
				    const char *template_name);


bool exaperf_lookup_sensor(exaperf_t *eh,
			   const char *name,
			   const char *template_name,
			   exaperf_sensor_t **sensor);

exaperf_sensor_t *exaperf_get_first_sensor(exaperf_t *eh);

exaperf_err_t exaperf_init_from_file(exaperf_t *eh,
				     const char *conf_file);

void exaperf_print_sensor_list(exaperf_t *eh);

#endif /* EXAPERF_CORE_H */
