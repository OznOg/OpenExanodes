#ifndef EXAPERF_SENSOR_H
#define EXAPERF_SENSOR_H

#include "os/include/os_thread.h"
#include "os/include/os_inttypes.h"

#include "exaperf/include/exaperf.h"
#include "exaperf/src/exaperf_sensor_template.h"
#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_timeframe.h"
#include "exaperf/src/exaperf_sample.h"
#include "exaperf/src/exaperf_distribution.h"
#include "exaperf/src/exaperf_counter.h"
#include "exaperf/src/exaperf_repart.h"
#include "exaperf/src/exaperf_duration.h"

#ifdef WIN32
#define exaperf_sensor_log(sh, fmt, ...)			\
{								\
    if (strcmp((sh)->template->name, (sh)->name) == 0)		\
    {								\
	(sh)->print(exaperf_log_prefix "[%s]: " fmt,		\
		    (sh)->name, __VA_ARGS__);			\
    }								\
    else							\
    {								\
	(sh)->print(exaperf_log_prefix "[%s-%s]: " fmt,		\
		    (sh)->template->name,			\
		    (sh)->name, __VA_ARGS__);			\
    }								\
}
#else
#define exaperf_sensor_log(sh, fmt, ...)			\
{								\
    if (strcmp((sh)->template->name, (sh)->name) == 0)		\
    {								\
	(sh)->print(exaperf_log_prefix "[%s]: " fmt,		\
		    (sh)->name, ## __VA_ARGS__);		\
    }								\
    else							\
    {								\
	(sh)->print(exaperf_log_prefix "[%s-%s]: " fmt,		\
		    (sh)->template->name,			\
		    (sh)->name, ## __VA_ARGS__);		\
    }								\
}
#endif

typedef enum exaperf_sensor_kind
{
    EXAPERF_SENSOR_COUNTER = 0,
    EXAPERF_SENSOR_REPART,
    EXAPERF_SENSOR_DURATION
} exaperf_sensor_kind_t;

/** Sensor structure */
struct exaperf_sensor
{
    os_thread_mutex_t lock;			/**< Lock */
    char name[EXAPERF_MAX_TOKEN_LEN + 1];	/**< Name of the sensor */
    exaperf_sensor_kind_t kind;			/**< The kind of sensor */
    exaperf_timeframe_t sampling_timeframe;	/**< The minimum time between two values are added to the sample */
    exaperf_timeframe_t flushing_timeframe;	/**< The minimum time between two flush */
    exaperf_sample_t sample;			/**< The sample of data collected by the sensor */
    exaperf_distribution_t distribution;	/**< The distribution of data collected by the sensor */
    uint32_t flushing_filter;			/**< Filter for the informationto flush or not */
    void (*print)(const char *fmt, ...);	/**< Pointer to the flushing function */
    exaperf_sensor_template_t *template;

    /** Specific information  depending on the sensor kind */
    union {
	exaperf_counter_t counter;
	exaperf_repart_t repart;
	exaperf_duration_t duration;
    } specific;
};


exaperf_sensor_t *
exaperf_sensor_new(const char* name,
		   void (*print)(const char *fmt, ...),
		   exaperf_err_t *err);

exaperf_err_t exaperf_sensor_param_set(exaperf_sensor_t *sensor,
				       exaperf_sensor_param_t key,
				       uint32_t value);
void exaperf_sensor_init(exaperf_sensor_t *sensor,
			 exaperf_sensor_kind_t kind);

exaperf_sensor_t *
exaperf_sensor_retrieve(exaperf_t *eh,
			const char *template_name,
			const char *name);

/** Record a new value.
 *  The sample can be flushed at the end of this function
 */
void exaperf_sensor_set(exaperf_sensor_t *sensor, double value);

/** Record several values cummulated.
 *  The sample can be flushed at the end of this function
 */
void exaperf_sensor_set_multiple(exaperf_sensor_t *sensor,
				 double sum_values, unsigned int nb_values);

void exaperf_sensor_lock(exaperf_sensor_t *sensor);
void exaperf_sensor_unlock(exaperf_sensor_t *sensor);

bool exaperf_sensor_name_cmp(exaperf_sensor_t *sensor,
			     const char *name, size_t size);
void __exaperf_sensor_free(exaperf_sensor_t *sensor);
#define exaperf_sensor_free(sensor) (__exaperf_sensor_free(sensor), sensor = NULL)


exaperf_counter_t * exaperf_sensor_counter(exaperf_sensor_t *sensor);

exaperf_repart_t * exaperf_sensor_repart(exaperf_sensor_t *sensor);

exaperf_duration_t * exaperf_sensor_duration(exaperf_sensor_t *sensor);

#endif /* EXAPERF_SENSOR_H */
