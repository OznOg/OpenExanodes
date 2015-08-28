/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <stdlib.h>

#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"

#include "common/include/exa_conversion.h"
#include "common/include/exa_assert.h"

#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_tools.h"
#include "exaperf/src/exaperf_config.h"
#include "exaperf/src/exaperf_core.h"
#include "exaperf/src/exaperf_filter.h"

static bool exaperf_get_list_template(exaperf_t *eh,
				      const char *template_name,
				      exaperf_list_sensor_template_t **elem)
{
    exaperf_list_sensor_template_t *current;

    if (eh->sensor_templates == NULL)
	return false;

    if (eh->sensor_templates->sensor_template == NULL)
	return false;

    current = eh->sensor_templates;

    while (current != NULL)
    {
	if (current->sensor_template != NULL)
	{
	    bool eq = exaperf_sensor_template_name_cmp(current->sensor_template,
                                                       template_name, strlen(template_name) + 1);
	    if (eq)
	    {
		*elem = current;
		return true;
	    }
	    current = current->next;
	}
    }
    return false;
}

static bool exaperf_get_list_elem(exaperf_t *eh,
				  const char *sensor_name,
				  const char *template_name,
				  exaperf_list_sensor_t **elem)
{
    exaperf_list_sensor_t *current;
    bool ret;

    if (eh->sensors == NULL)
	return false;

    if (eh->sensors->sensor == NULL)
	return false;

    current = eh->sensors;

    while (current != NULL)
    {
	if (current->sensor != NULL)
	{
	    ret = exaperf_sensor_name_cmp(current->sensor,
					  sensor_name, strlen(sensor_name) + 1);
	    if (ret)
	    {
		ret = exaperf_sensor_template_name_cmp(current->sensor->template,
						       template_name, strlen(template_name) + 1);
		if (ret)
		{
		    *elem = current;
		    return true;
		}
	    }
	    current = current->next;
	}
    }
    return false;
}

exaperf_sensor_template_t *exaperf_get_first_sensor_template(exaperf_t *eh)
{
   if (eh->nb_sensor_templates > 0)
	return eh->sensor_templates->sensor_template;
    return NULL;
}

exaperf_sensor_t *exaperf_get_first_sensor(exaperf_t *eh)
{
    if (eh->nb_sensors > 0)
	return eh->sensors->sensor;
    return NULL;
}

exaperf_err_t exaperf_add_sensor_template(exaperf_t *eh,
					  const char *name)
{
    exaperf_list_sensor_template_t *s;
    exaperf_sensor_template_t *sensor_template;
    exaperf_err_t err;

    if (exaperf_lookup_sensor_template(eh, name, &sensor_template))
	return EXAPERF_ALREADY_DEFINED_SENSOR_TEMPLATE;

    sensor_template = exaperf_sensor_template_new(name, &err);
    if (sensor_template == NULL)
	return err;

    s = os_malloc(sizeof(exaperf_list_sensor_template_t));
    if (s == NULL)
	return EXAPERF_MALLOC_FAILED;

    s->sensor_template = sensor_template;
    s->next = NULL;
    s->previous = NULL;

    if (eh->nb_sensor_templates == 0)
    {
	eh->last_template = s;
	eh->sensor_templates = s;
	eh->nb_sensor_templates++;
    }
    else
    {
	s->previous = eh->last_template;
	eh->last_template->next = s;
	eh->last_template = s;
	eh->nb_sensor_templates++;
    }

    return EXAPERF_SUCCESS;
}

exaperf_err_t exaperf_add_sensor(exaperf_t *eh,
				 const char *name,
				 exaperf_sensor_template_t *template)
{
    exaperf_list_sensor_t *s;
    exaperf_sensor_t *sensor;
    exaperf_err_t err;

    if (exaperf_lookup_sensor(eh, name, template->name, &sensor))
	return EXAPERF_ALREADY_DEFINED_SENSOR;

    sensor = exaperf_sensor_new(name, eh->exaperf_print, &err);
    if (sensor == NULL)
	return err;

    sensor->template = template;

    s = os_malloc(sizeof(exaperf_list_sensor_t));
    if (s == NULL)
	return EXAPERF_MALLOC_FAILED;

    s->sensor = sensor;
    s->next = NULL;
    s->previous = NULL;

    if (eh->nb_sensors == 0)
    {
	eh->last = s;
	eh->sensors = s;
	eh->nb_sensors++;
    }
    else
    {
	s->previous = eh->last;
	eh->last->next = s;
	eh->last = s;
	eh->nb_sensors++;
    }

    return EXAPERF_SUCCESS;
}

exaperf_err_t exaperf_remove_sensor_template(exaperf_t *eh,
					     const char *name)
{
    bool ret;
    exaperf_list_sensor_template_t *elem = NULL;

    ret = exaperf_get_list_template(eh, name, &elem);
    if (ret == false)
	return EXAPERF_UNDEFINED_SENSOR_TEMPLATE;

    eh->nb_sensor_templates--;

    if (elem->previous == NULL)
    {
	eh->sensor_templates = elem->next;
	if (eh->sensor_templates != NULL)
	    eh->sensor_templates->previous = NULL;
    }
    else
    {
	elem->previous->next = elem->next;
    }

    if (elem == eh->last_template)
    {
	eh->last_template = elem->previous;
    }

    os_free(elem->sensor_template);
    elem->sensor_template = NULL;

    os_free(elem);
    elem = NULL;

    return EXAPERF_SUCCESS;
}

exaperf_err_t exaperf_remove_sensor(exaperf_t *eh,
				    const char *name,
				    const char *template_name)
{
    bool ret;
    exaperf_list_sensor_t *elem = NULL;

    ret = exaperf_get_list_elem(eh, name, template_name, &elem);
    if (ret == false)
	return EXAPERF_UNDEFINED_SENSOR;

    eh->nb_sensors--;

    if (elem->previous == NULL)
    {
	eh->sensors = elem->next;
	if (eh->sensors != NULL)
	    eh->sensors->previous = NULL;
    }
    else
    {
	elem->previous->next = elem->next;
    }

    if (elem == eh->last)
    {
	eh->last = elem->previous;
    }

    /* We test if the sensor is not null to handle the case where the
     * sensor was not initialized i.e. it is declared in the config file
     * and not initialized in the code. */
    if (elem->sensor != NULL)
	os_free(elem->sensor);
    elem->sensor = NULL;

    os_free(elem);
    elem = NULL;

    return EXAPERF_SUCCESS;
}

bool exaperf_lookup_sensor_template(exaperf_t *eh,
				    const char *name,
				    exaperf_sensor_template_t **sensor_template)
{
    exaperf_list_sensor_template_t *elem = NULL;
    bool found;

    found = exaperf_get_list_template(eh, name, &elem);
    if (!found)
	return false;

    *sensor_template = elem->sensor_template;
    return true;
}

bool exaperf_lookup_sensor(exaperf_t *eh,
			   const char *name,
			   const char *template_name,
			   exaperf_sensor_t **sensor)
{
    exaperf_list_sensor_t *elem = NULL;
    bool ret;

    ret = exaperf_get_list_elem(eh, name, template_name, &elem);

    if (ret == false)
	return false;

    *sensor = elem->sensor;
    return true;
}

static exaperf_err_t exaperf_add_param(exaperf_t *eh,
				       exaperf_sensor_template_t *sensor_template,
				       const char *key,
				       const char **values,
				       uint32_t nb_values)
{
    exaperf_sensor_param_t param_id;
    exaperf_err_t ret;
    uint32_t value;

    ret = exaperf_sensor_str2param(key, EXAPERF_MAX_TOKEN_LEN + 1, &param_id);
    if (ret != EXAPERF_SUCCESS)
    	return ret;

    switch (param_id)
    {
    case EXAPERF_PARAM_FLUSHING_PERIOD:
    case EXAPERF_PARAM_SAMPLING_PERIOD:
    case EXAPERF_PARAM_SAMPLE_SIZE:
    	if (to_uint32(values[0], &value) != 0)
	    return EXAPERF_INVALID_PARAM;

	return exaperf_sensor_template_param_set(sensor_template, param_id, value);
    case EXAPERF_PARAM_FLUSHING_FILTER:
	/* build the filter from values */
	ret = exaperf_filter_build(&value, values, nb_values);
	if (ret != EXAPERF_SUCCESS)
	    return ret;

	return exaperf_sensor_template_param_set(sensor_template, param_id, value);
    case EXAPERF_PARAM_DISTRIBUTION:
    default:
	return EXAPERF_INVALID_PARAM;
    }
}

exaperf_err_t exaperf_init_from_file(exaperf_t *eh,
				     const char *conf_file)
{
    FILE *file;
    char sensor_template_name[EXAPERF_MAX_TOKEN_LEN + 1];
    char line[EXAPERF_CONFIG_MAX_FILE_LINE_LEN + 1];
    char key[EXAPERF_MAX_TOKEN_LEN + 1];
    char values_buf[(EXAPERF_MAX_TOKEN_LEN + 1) * EXAPERF_PARAM_NBMAX_VALUES];
    char *values[EXAPERF_PARAM_NBMAX_VALUES];
    uint32_t nb_values;
    char err_str[EXAPERF_CONFIG_MAX_ERROR_MSG_LEN + 1];
    exaperf_config_err_t ret;
    exaperf_err_t err;
    bool result;
    bool parsing_err = false;
    uint32_t num_line = 0;
    unsigned int i;

    for(i=0;i<EXAPERF_PARAM_NBMAX_VALUES;i++)
	values[i] = &values_buf[i*(EXAPERF_MAX_TOKEN_LEN + 1)];

    file = fopen(conf_file, "rt");
    if (file == NULL)
	return EXAPERF_CONF_FILE_OPEN_FAILED;

    while (fgets(line, EXAPERF_CONFIG_MAX_FILE_LINE_LEN, file) != NULL)
    {
	num_line++;

	remove_whitespace(line);

	ret = exaperf_config_is_empty_line(line, &result);
	if (result == true)
	    continue;

	ret = exaperf_config_is_comment(line, &result);
	if (result == true)
	    continue;

	ret = exaperf_config_is_template_declaration(line,
						     sensor_template_name,
						     sizeof(sensor_template_name),
						     &result);
	if (ret != EXAPERF_CONFIG_SUCCESS)
	{
	    exaperf_config_get_err_str(ret, err_str, sizeof(err_str));
	    exaperf_log(eh, "init from file failed: %s (line %d)",
			err_str, num_line);
	    parsing_err = true;
	}
	else
	{
	    if (result == true)
	    {
		err = exaperf_add_sensor_template(eh, sensor_template_name);
		if (err != EXAPERF_SUCCESS)
		    return err;

		continue;
	    }
	}

	ret = exaperf_config_is_param_declaration(line,
						  key, sizeof(key),
						  values, EXAPERF_MAX_TOKEN_LEN + 1,
						  &nb_values, EXAPERF_PARAM_NBMAX_VALUES);
	if (ret != EXAPERF_CONFIG_SUCCESS)
	{
	    exaperf_config_get_err_str(ret, err_str, sizeof(err_str));
	    exaperf_log(eh, "init from file failed: %s (line %d)",
			err_str, num_line);
	    parsing_err = true;
	}
	else
	{
	    /* The cast is necessary to make gcc compile without warning.  We
	     * still don't understand why it is necessary considering it just
	     * add a const qualifier...
	     */
	    err = exaperf_add_param(eh, eh->last_template->sensor_template,
				    key, (const char**) values, nb_values);
	    if (err != EXAPERF_SUCCESS)
		return err;
	    continue;
	}
    }

    fclose (file);

    if (parsing_err == true)
	return EXAPERF_PARSING_ERROR;

    return EXAPERF_SUCCESS;
}


void exaperf_print_sensor_list(exaperf_t *eh)
{
    exaperf_list_sensor_t *sl;
    uint32_t i = 0;

    sl = eh->sensors;
    while (sl != NULL)
    {
	exaperf_log(eh, "========= sensor %d: %s", i, sl->sensor->name);
	i++;
	sl = sl->next;
    }
}
