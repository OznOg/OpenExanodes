#ifndef EXAPERF_H
#define EXAPERF_H

#include "os/include/os_inttypes.h"

#define EXAPERF_ENABLED  1

/** Exaperf environment handler */
typedef struct exaperf exaperf_t;

/** Sensor handler */
typedef struct exaperf_sensor exaperf_sensor_t;


typedef enum
{
    EXAPERF_SUCCESS = 0,
    EXAPERF_MALLOC_FAILED,
    EXAPERF_CONF_FILE_OPEN_FAILED,
    EXAPERF_PARSING_ERROR,
    EXAPERF_UNDEFINED_SENSOR,
    EXAPERF_UNDEFINED_SENSOR_TEMPLATE,
    EXAPERF_INVALID_PARAM,
    EXAPERF_ALREADY_DEFINED_SENSOR,
    EXAPERF_ALREADY_DEFINED_SENSOR_TEMPLATE
} exaperf_err_t;


exaperf_t *exaperf_alloc(void);
exaperf_err_t exaperf_init(exaperf_t *eh, const char *conf_file,
			   void (*print_fct)(const char *fmt, ...));
void __exaperf_free(exaperf_t *eh);
#define exaperf_free(eh) (__exaperf_free(eh), eh = NULL)


/**
 * Flush the results of a sensor to the logs
 *
 * @param[in] sensor  The sensor to flush
 */
void exaperf_sensor_flush(exaperf_sensor_t *sensor);

/**
 * Initialize a duration sensor whose name
 *
 * @param[in] eh		exaperf session handler
 * @param[in] name		sensor name and also template name
 * @param[in] parallel		are the events parallel or sequential
 *
 * @return pointer to the duration sensor handler or NULL in case of failure or
 * if the the sensor is not activated
 */
exaperf_sensor_t * exaperf_duration_init(exaperf_t *eh,
					 const char *name,
					 bool interleaved);

/**
 * Initialize a duration sensor form a template
 *
 * @param[in] eh		exaperf session handler
 * @param[in] template_name	sensor template name
 * @param[in] name		sensor name
 * @param[in] parallel		are the events parallel or sequential
 *
 * @return pointer to the duration sensor handler or NULL in case of failure or
 * if the the sensor is not activated
 */
exaperf_sensor_t * exaperf_duration_init_from_template(exaperf_t *eh,
						       const char *template_name,
						       const char *name,
						       bool interleaved);

/**
 * Signal the begining of a new duration.
 *
 * @param[in] duration		duration sensor handler
 */
void exaperf_duration_begin(exaperf_sensor_t * duration);

/**
 * Signal the end of a duration.
 *
 * @param[in] duration		duration sensor handler
 */
void exaperf_duration_end(exaperf_sensor_t * duration);

/**
 * Records a duration.
 *
 * @param[in] duration		duration sensor handler
 */
void exaperf_duration_record(exaperf_sensor_t * duration, double value);


exaperf_sensor_t *exaperf_counter_init(exaperf_t *eh,
				       const char *name);
exaperf_sensor_t *exaperf_counter_init_from_template(exaperf_t *eh,
						     const char *template_name,
						     const char *name);
void exaperf_counter_inc(exaperf_sensor_t *counter);
void exaperf_counter_dec(exaperf_sensor_t *counter);
void exaperf_counter_add(exaperf_sensor_t *counter, double value);
void exaperf_counter_set(exaperf_sensor_t * counter, double value);

exaperf_sensor_t *exaperf_repart_init(exaperf_t *eh,
				      const char *name,
				      unsigned int size,
				      const double *limits);
exaperf_sensor_t *exaperf_repart_init_from_template(exaperf_t *eh,
						    const char *template_name,
						    const char *name,
						    unsigned int size,
						    const double *limits);

void exaperf_repart_add_value(exaperf_sensor_t *sensor, double value);

#endif
