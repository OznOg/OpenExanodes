/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXAPERF_DISTRIBUTION_H
#define EXAPERF_DISTRIBUTION_H

#define EXAPERF_DISTRIBUTION_NBMAX_LIMITS 32

/** Distributionition sensor specific information */
typedef struct {
    /** Number of segments in the distribution */
    unsigned int nb_limits;
    /** Limits of the segments */
    double limits[EXAPERF_DISTRIBUTION_NBMAX_LIMITS];
    /** Distribution of the sample values */
    unsigned int population[EXAPERF_DISTRIBUTION_NBMAX_LIMITS+1];
    /** Distribution of the sample values cummulated from the initialization */
    unsigned int cumul_population[EXAPERF_DISTRIBUTION_NBMAX_LIMITS+1];
} exaperf_distribution_t;



exaperf_err_t exaperf_distribution_init(exaperf_distribution_t * distribution);

exaperf_err_t exaperf_distribution_add_limit(exaperf_distribution_t * distribution,
					     double new_limit);

void exaperf_distribution_compute(exaperf_distribution_t * distribution,
				  const exaperf_sample_t * sample);


/* Accessors */
unsigned int exaperf_distribution_get_nb_limits(const exaperf_distribution_t * distribution);
double exaperf_distribution_get_limit(const exaperf_distribution_t * distribution, unsigned int i);
unsigned int exaperf_distribution_get_population(const exaperf_distribution_t * distribution, unsigned int i);
unsigned int exaperf_distribution_get_sum_population(const exaperf_distribution_t * distribution);
unsigned int exaperf_distribution_get_cumul_population(const exaperf_distribution_t * distribution, unsigned int i);
unsigned int exaperf_distribution_get_sum_cumul_population(const exaperf_distribution_t * distribution);


#endif /* EXAPERF_DISTRIBUTION_H */
