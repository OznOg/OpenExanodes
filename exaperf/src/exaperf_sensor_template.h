#ifndef EXAPERF_SENSOR_TEMPLATE_H
#define EXAPERF_SENSOR_TEMPLATE_H

#include "os/include/os_inttypes.h"
#include "os/include/os_thread.h"

#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"
#include "exaperf/src/exaperf_timeframe.h"
#include "exaperf/src/exaperf_sample.h"
#include "exaperf/src/exaperf_distribution.h"

#include "exaperf/src/exaperf_counter.h"
#include "exaperf/src/exaperf_repart.h"
#include "exaperf/src/exaperf_duration.h"

typedef enum
{
    EXAPERF_PARAM_FLUSHING_PERIOD,
    EXAPERF_PARAM_SAMPLING_PERIOD,
    EXAPERF_PARAM_SAMPLE_SIZE,
    EXAPERF_PARAM_FLUSHING_FILTER,
    EXAPERF_PARAM_DISTRIBUTION,
    EXAPERF_PARAM_NONE
} exaperf_sensor_param_t;

/** Sensor template structure */
struct exaperf_sensor_template
{
    char name[EXAPERF_MAX_TOKEN_LEN + 1];	/**< Name of the sensor template */
    double sampling_period;			/**< The minimum time between two values are added to the sample */
    double flushing_period;			/**< The minimum time between two flush */
    uint32_t sample_size;
    uint32_t flushing_filter;			/**< Filter for the information to flush or not */
};

typedef struct exaperf_sensor_template exaperf_sensor_template_t;

exaperf_err_t exaperf_sensor_str2param(const char *str, size_t size,
				       exaperf_sensor_param_t *param);

exaperf_sensor_template_t *
exaperf_sensor_template_new(const char* name,
			    exaperf_err_t *err);

exaperf_err_t
exaperf_sensor_template_param_set(exaperf_sensor_template_t *sensor_template,
				  exaperf_sensor_param_t key,
				  uint32_t value);

bool
exaperf_sensor_template_name_cmp(exaperf_sensor_template_t *sensor_template,
				 const char *name, size_t size);

void __exaperf_sensor_template_free(exaperf_sensor_template_t *sensor_template);
#define exaperf_sensor_template_free(tmpl) \
    (__exaperf_sensor_template_free(tmpl), tmpl = NULL)

#endif /* EXAPERF_SENSOR_TEMPLATE_H */
