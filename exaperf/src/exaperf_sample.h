/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef EXAPERF_SAMPLE_H
#define EXAPERF_SAMPLE_H

#include "os/include/os_inttypes.h"

#include "exaperf/include/exaperf.h"
#include "exaperf/include/exaperf_constants.h"

typedef struct exaperf_sample
{
    double *values;

    double sum;
    double min;
    double max;

    uint32_t size;
    uint32_t nb_total;
    uint32_t nb_elem;
    uint32_t nb_lost;
} exaperf_sample_t;

exaperf_err_t exaperf_sample_init(exaperf_sample_t *sample, uint32_t sample_size);
void exaperf_sample_clear(exaperf_sample_t *sample);

void exaperf_sample_add(exaperf_sample_t *sample, double value);
void exaperf_sample_add_multiple(exaperf_sample_t *sample, double sum_values, unsigned int nb_values);

void exaperf_sample_reset(exaperf_sample_t *sample);

bool exaperf_sample_is_empty(const exaperf_sample_t *sample);
bool exaperf_sample_is_full(const exaperf_sample_t *sample);

/* Accessors */
uint32_t exaperf_sample_get_nb_elem(const exaperf_sample_t *sample);
uint32_t exaperf_sample_get_nb_lost(const exaperf_sample_t *sample);
double exaperf_sample_get_mean(const exaperf_sample_t *sample);
double exaperf_sample_get_min(const exaperf_sample_t *sample);
double exaperf_sample_get_max(const exaperf_sample_t *sample);
const double * exaperf_sample_get_values(const exaperf_sample_t *sample);

#endif /* EXAPERF_SAMPLE_H */
