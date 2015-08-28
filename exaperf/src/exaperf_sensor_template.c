
#include <stdlib.h>

#include "os/include/os_mem.h"
#include "os/include/os_string.h"
#include "os/include/os_thread.h"
#include "os/include/os_stdio.h"
#include "os/include/os_inttypes.h"
#include "os/include/strlcpy.h"

#include "common/include/exa_assert.h"

#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_timeframe.h"
#include "exaperf/src/exaperf_sensor.h"
#include "exaperf/src/exaperf_sensor_template.h"
#include "exaperf/src/exaperf_stats.h"
#include "exaperf/src/exaperf_filter.h"

struct param_str
{
    char *str;
    exaperf_sensor_param_t param;
};

static struct param_str params[] =
{
    {"flushing_period", EXAPERF_PARAM_FLUSHING_PERIOD},
    {"sampling_period", EXAPERF_PARAM_SAMPLING_PERIOD},
    {"sample_size", EXAPERF_PARAM_SAMPLE_SIZE},
    {"flushing_filter", EXAPERF_PARAM_FLUSHING_FILTER},
    {"distribution", EXAPERF_PARAM_DISTRIBUTION},
    {NULL, EXAPERF_PARAM_NONE}
};

exaperf_err_t
exaperf_sensor_str2param(const char *str, size_t size,
			 exaperf_sensor_param_t *param)
{
    int i = 0;

    do
    {
	if (strncmp(str, params[i].str, EXAPERF_MAX_TOKEN_LEN + 1) == 0)
	{
	    *param = params[i].param;
	    return EXAPERF_SUCCESS;
	}
	i++;
    } while (params[i].param != EXAPERF_PARAM_NONE);

    return EXAPERF_INVALID_PARAM;
}

exaperf_sensor_template_t *
exaperf_sensor_template_new(const char* name,
			    exaperf_err_t *err)
{
    size_t size_ret;
    exaperf_sensor_template_t *sensor_template;
    *err = EXAPERF_SUCCESS;

    sensor_template = os_malloc(sizeof(exaperf_sensor_template_t));
    if (sensor_template == NULL)
    {
	*err = EXAPERF_MALLOC_FAILED;
	return NULL;
    }

    size_ret = strlcpy(sensor_template->name, name, sizeof(sensor_template->name));
    if (size_ret >= sizeof(sensor_template->name))
    {
	*err = EXAPERF_PARSING_ERROR;
	goto error_free_sensor;
    }

    sensor_template->sampling_period = 0.0;
    sensor_template->flushing_period = 0.0;
    sensor_template->sample_size = 0;
    sensor_template->flushing_filter = 0;

    return sensor_template;

error_free_sensor:
    os_free(sensor_template);
    return NULL;
}

exaperf_err_t
exaperf_sensor_template_param_set(exaperf_sensor_template_t *sensor_template,
				  exaperf_sensor_param_t key,
				  uint32_t value)
{
    EXA_ASSERT(sensor_template != NULL);

    switch (key)
    {
    case EXAPERF_PARAM_FLUSHING_PERIOD:
	sensor_template->flushing_period = value;
	break;
    case EXAPERF_PARAM_SAMPLING_PERIOD:
       	sensor_template->sampling_period = value;
	break;
    case EXAPERF_PARAM_FLUSHING_FILTER:
       	sensor_template->flushing_filter = value;
	break;
    case EXAPERF_PARAM_SAMPLE_SIZE:
       	sensor_template->sample_size = value;
	break;
    case EXAPERF_PARAM_DISTRIBUTION:
    case EXAPERF_PARAM_NONE:
	return EXAPERF_INVALID_PARAM;
    }

    return EXAPERF_SUCCESS;
}

bool
exaperf_sensor_template_name_cmp(exaperf_sensor_template_t *sensor_template,
				 const char *name, size_t size)
{
    EXA_ASSERT(sensor_template != NULL);

    if (strncmp(name, sensor_template->name, size) == 0)
	return true;
    else
	return false;
}

void
__exaperf_sensor_template_free(exaperf_sensor_template_t *sensor_template)
{
    EXA_ASSERT(sensor_template != NULL);

    os_free(sensor_template);
    sensor_template = NULL;
}


