
#include <stdlib.h>

#include "os/include/os_mem.h"
#include "os/include/os_inttypes.h"
#include "os/include/strlcpy.h"
#include "os/include/os_string.h"
#include "os/include/os_thread.h"
#include "os/include/os_stdio.h"

#include "common/include/exa_assert.h"

#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_timeframe.h"
#include "exaperf/src/exaperf_sensor.h"
#include "exaperf/src/exaperf_core.h"
#include "exaperf/src/exaperf_stats.h"
#include "exaperf/src/exaperf_filter.h"

#define EXAPERF_FLUSH_STRING_MAX_SIZE 256
#define EXAPERF_FLUSH_RAW_MAX 10

static void
exaperf_sensor_flush_raw(exaperf_sensor_t *sensor)
{
    uint32_t nb;
    char stats_str[EXAPERF_FLUSH_STRING_MAX_SIZE+1];
    int offset = 0;
    const double *values = exaperf_sample_get_values(&sensor->sample);

    offset += os_snprintf(stats_str + offset,
		       EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
		       "values = [\n");

    for (nb = 0 ; nb < exaperf_sample_get_nb_elem(&sensor->sample) ; nb++)
    {
	offset += os_snprintf(stats_str + offset,
			   EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
			   "%g,", values[nb]);

	if (nb != 0 && nb % EXAPERF_FLUSH_RAW_MAX)
	{
	    exaperf_sensor_log(sensor,"%s",stats_str);
	    offset = 0;
	}
    }
    if (!(nb % EXAPERF_FLUSH_RAW_MAX))
    {
	offset += os_snprintf(stats_str + offset,
			   EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
			   "\n]");
    }
    else
    {
	offset += os_snprintf(stats_str + offset,
			   EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
			   "]");
    }
    exaperf_sensor_log(sensor,"%s",stats_str);
}

static void
exaperf_sensor_flush_stats(exaperf_sensor_t *sensor)
{
    exaperf_basic_stat_t stats;
    char stats_str[EXAPERF_FLUSH_STRING_MAX_SIZE+1];
    int offset;

    stats = exaperf_compute_basic_stat(
	exaperf_sample_get_values(&sensor->sample),
	exaperf_sample_get_nb_elem(&sensor->sample));

    offset = 0;

    if (exaperf_filter_contains(sensor->flushing_filter, EXAPERF_FILTER_RAW))
    {
	exaperf_sensor_flush_raw(sensor);
    }

    if (exaperf_filter_contains(sensor->flushing_filter, EXAPERF_FILTER_MEAN))
	offset = os_snprintf(stats_str + offset, EXAPERF_FLUSH_STRING_MAX_SIZE,
			  "mean=%.5f ", exaperf_sample_get_mean(&sensor->sample));

    if (exaperf_filter_contains(sensor->flushing_filter, EXAPERF_FILTER_MIN))
	offset += os_snprintf(stats_str + offset, EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
		      "min=%.5f ", exaperf_sample_get_min(&sensor->sample));

    if (exaperf_filter_contains(sensor->flushing_filter, EXAPERF_FILTER_MAX))
	offset += os_snprintf(stats_str + offset, EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
		       "max=%.5f ", exaperf_sample_get_max(&sensor->sample));

    if (exaperf_filter_contains(sensor->flushing_filter, EXAPERF_FILTER_MEDIAN))
	offset += os_snprintf(stats_str + offset, EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
		       "median=%.5f ", stats.median);

    if (exaperf_filter_contains(sensor->flushing_filter, EXAPERF_FILTER_STDEV))
	offset += os_snprintf(stats_str + offset, EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
		       "std_dev=%.5f ", stats.std_dev);

    if (exaperf_filter_contains(sensor->flushing_filter, EXAPERF_FILTER_DETAIL))
    {
	offset += os_snprintf(stats_str + offset, EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
			   "sampling_duration=%.2f ", exaperf_timeframe_get_duration(&sensor->sampling_timeframe));

	offset += os_snprintf(stats_str + offset, EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
			   "flushing_duration=%.2f ", exaperf_timeframe_get_duration(&sensor->flushing_timeframe));

	offset += os_snprintf(stats_str + offset, EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
			   "measures=%d ", exaperf_sample_get_nb_elem(&sensor->sample));

	offset += os_snprintf(stats_str + offset, EXAPERF_FLUSH_STRING_MAX_SIZE - offset,
			   "nb_lost=%d", exaperf_sample_get_nb_lost(&sensor->sample));
    }

    stats_str[EXAPERF_FLUSH_STRING_MAX_SIZE] = '\0';

    if (offset != 0)
	exaperf_sensor_log(sensor,"%s",stats_str);
}


static void
exaperf_sensor_flush_distribution(exaperf_sensor_t *sensor)
{
    unsigned int i;

    if (exaperf_distribution_get_nb_limits(&sensor->distribution) == 0)
	return;

    exaperf_distribution_compute(&sensor->distribution, &sensor->sample);


    exaperf_sensor_log(sensor,
		       "B_DISTRIB_FLUSH ----- nb_lost=%d -----",
		       exaperf_sample_get_nb_lost(&sensor->sample));

    for (i=0 ; i < exaperf_distribution_get_nb_limits(&sensor->distribution); i++)
    {
	exaperf_sensor_log(sensor,
			   "\t%g\t(<=\t%u",
			   exaperf_distribution_get_limit(&sensor->distribution, i),
			   exaperf_distribution_get_cumul_population(&sensor->distribution, i));
    }

    exaperf_sensor_log(sensor,
		       "\t%g\t(>\t%u",
		       exaperf_distribution_get_limit(&sensor->distribution, i-1 ),
		       exaperf_distribution_get_cumul_population(&sensor->distribution, i));

    exaperf_sensor_log(sensor,"E_DISTRIB_FLUSH ----------------------");
}


void exaperf_sensor_flush(exaperf_sensor_t *sensor)
{
    if (sensor == NULL)
	return;

    if (sensor->sample.values != NULL)
    {
	exaperf_sensor_flush_stats(sensor);
	exaperf_sensor_flush_distribution(sensor);

	exaperf_timeframe_reset(&sensor->flushing_timeframe);
	exaperf_sample_reset(&sensor->sample);
    }
}

static void exaperf_sensor_auto_flush(exaperf_sensor_t *sensor)
{
    EXA_ASSERT(sensor != NULL);

    if (exaperf_timeframe_is_finished(&sensor->flushing_timeframe))
        exaperf_sensor_flush(sensor);
}

exaperf_sensor_t * exaperf_sensor_new(const char* name,
				      void (*print)(const char *fmt, ...),
				      exaperf_err_t *err)
{
    size_t size_ret;
    exaperf_sensor_t *sensor;
    *err = EXAPERF_SUCCESS;

    sensor = os_malloc(sizeof(exaperf_sensor_t));
    if (sensor == NULL)
    {
	*err = EXAPERF_MALLOC_FAILED;
	return NULL;
    }

    size_ret = strlcpy(sensor->name, name, sizeof(sensor->name));
    if (size_ret >= sizeof(sensor->name))
    {
	*err = EXAPERF_PARSING_ERROR;
	goto error_free_sensor;
    }

    os_thread_mutex_init(&sensor->lock);
    sensor->print = print;

    *err = exaperf_sample_init(&sensor->sample, 0);
    if (*err != EXAPERF_SUCCESS)
	goto error_free_sensor;

    *err = exaperf_distribution_init(&sensor->distribution);
    if (*err != EXAPERF_SUCCESS)
	goto error_free_sensor;

    exaperf_timeframe_init(&sensor->sampling_timeframe, 0.0);
    exaperf_timeframe_init(&sensor->flushing_timeframe, 0.0);

    return sensor;

error_free_sensor:
    os_free(sensor);
    return NULL;
}

exaperf_err_t
exaperf_sensor_param_set(exaperf_sensor_t *sensor,
			 exaperf_sensor_param_t key,
			 uint32_t value)
{
    EXA_ASSERT(sensor != NULL);

    switch (key)
    {
    case EXAPERF_PARAM_FLUSHING_PERIOD:
	exaperf_timeframe_init(&sensor->flushing_timeframe, value);
	break;
    case EXAPERF_PARAM_SAMPLING_PERIOD:
	exaperf_timeframe_init(&sensor->sampling_timeframe, value);

	break;
    case EXAPERF_PARAM_FLUSHING_FILTER:
	sensor->flushing_filter = value;
	break;
    case EXAPERF_PARAM_SAMPLE_SIZE:
	return exaperf_sample_init(&sensor->sample, value);
    case EXAPERF_PARAM_DISTRIBUTION:
    case EXAPERF_PARAM_NONE:
	return EXAPERF_INVALID_PARAM;
    }

    return EXAPERF_SUCCESS;
}

exaperf_sensor_t *
exaperf_sensor_retrieve(exaperf_t *eh,
			const char *template_name,
			const char *name)
{
    exaperf_sensor_t *sensor;
    exaperf_err_t err;

    if (exaperf_lookup_sensor(eh, name, template_name, &sensor))
	return sensor;
    else
    {
	exaperf_sensor_template_t *template;

	if (exaperf_lookup_sensor_template(eh, template_name, &template) == false)
	    return NULL;

	err = exaperf_add_sensor(eh, name, template);
	EXA_ASSERT(err == EXAPERF_SUCCESS);

	if (exaperf_lookup_sensor(eh, name, template_name, &sensor) == false)
	    return NULL;

	err = exaperf_sensor_param_set(sensor,
				       EXAPERF_PARAM_FLUSHING_PERIOD,
				       template->flushing_period);

	if (err != EXAPERF_SUCCESS)
	    goto error;

	err = exaperf_sensor_param_set(sensor,
				       EXAPERF_PARAM_SAMPLING_PERIOD,
				       template->sampling_period);

	if (err != EXAPERF_SUCCESS)
	    goto error;

	err = exaperf_sensor_param_set(sensor,
				       EXAPERF_PARAM_FLUSHING_FILTER,
				       template->flushing_filter);

	if (err != EXAPERF_SUCCESS)
	    goto error;

	err = exaperf_sensor_param_set(sensor,
				       EXAPERF_PARAM_SAMPLE_SIZE,
				       template->sample_size);

	if (err != EXAPERF_SUCCESS)
	    goto error;

    }

    return sensor;

error:
    exaperf_sensor_free(sensor);
    return NULL;
}

void
exaperf_sensor_init(exaperf_sensor_t *sensor, exaperf_sensor_kind_t kind)
{
    EXA_ASSERT(sensor != NULL);

    sensor->kind = kind;

    /* FIXME: this should be done in function of the kind of the sensor */
    exaperf_sample_reset(&sensor->sample);
    exaperf_timeframe_reset(&sensor->sampling_timeframe);
    exaperf_timeframe_reset(&sensor->flushing_timeframe);
}

void
exaperf_sensor_set(exaperf_sensor_t *sensor, double value)
{
    EXA_ASSERT(sensor != NULL);

    if (exaperf_timeframe_is_finished(&sensor->sampling_timeframe))
    {
	exaperf_sample_add(&sensor->sample, value);
	/* FIXME: we should store/print the time switch between this call
	 * and the end of the timeframe
	 */
	exaperf_timeframe_reset(&sensor->sampling_timeframe);
    }

    exaperf_sensor_auto_flush(sensor);
}

void
exaperf_sensor_set_multiple(exaperf_sensor_t *sensor, double sum_values, unsigned int nb_values)
{
    EXA_ASSERT(sensor != NULL);

    if (exaperf_timeframe_is_finished(&sensor->sampling_timeframe))
    {
	exaperf_sample_add_multiple(&sensor->sample, sum_values, nb_values);

	/* FIXME: we should store/print the time switch between this call
	 * and the end of the timeframe
	 */
	exaperf_timeframe_reset(&sensor->sampling_timeframe);
    }

    exaperf_sensor_auto_flush(sensor);
}

char *
exaperf_sensor_get_name(exaperf_sensor_t *sensor)
{
    EXA_ASSERT(sensor != NULL);

    return sensor->name;
}

void
exaperf_sensor_lock(exaperf_sensor_t *sensor)
{
    EXA_ASSERT(sensor != NULL);
    os_thread_mutex_lock(&sensor->lock);
}

void
exaperf_sensor_unlock(exaperf_sensor_t *sensor)
{
    EXA_ASSERT(sensor != NULL);
    os_thread_mutex_unlock(&sensor->lock);
}

exaperf_err_t
exaperf_sensor_init_validation(exaperf_sensor_t *sensor)
{
    EXA_ASSERT(sensor != NULL);

    return EXAPERF_SUCCESS;
}

bool
exaperf_sensor_name_cmp(exaperf_sensor_t *sensor, const char *name, size_t size)
{
    EXA_ASSERT(sensor != NULL);

    if (strncmp(name, sensor->name, size) == 0)
	return true;
    else
	return false;
}

void
__exaperf_sensor_free(exaperf_sensor_t *sensor)
{
    EXA_ASSERT(sensor != NULL);

    exaperf_sample_clear(&sensor->sample);

    os_thread_mutex_destroy(&sensor->lock);

    os_free(sensor);
    sensor = NULL;
}

exaperf_counter_t *
exaperf_sensor_counter(exaperf_sensor_t *sensor)
{
    EXA_ASSERT(sensor != NULL);
    EXA_ASSERT(sensor->kind == EXAPERF_SENSOR_COUNTER);

    return &sensor->specific.counter;
}

exaperf_duration_t *
exaperf_sensor_duration(exaperf_sensor_t *sensor)
{
    EXA_ASSERT(sensor != NULL);
    EXA_ASSERT(sensor->kind == EXAPERF_SENSOR_DURATION);

    return &sensor->specific.duration;
}

exaperf_repart_t *
exaperf_sensor_repart(exaperf_sensor_t *sensor)
{
    EXA_ASSERT(sensor != NULL);
    EXA_ASSERT(sensor->kind == EXAPERF_SENSOR_REPART);

    return &sensor->specific.repart;
}
