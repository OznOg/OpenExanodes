/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXAPERF_STATS_H
#define EXAPERF_STATS_H

typedef struct
{
    double min;		/**< Minimum value 	*/
    double max;		/**< Maximum value 	*/
    double mean;	/**< Mean value 	*/
    double median;	/**< Median value 	*/
    double std_dev;	/**< Standard deviation	*/
} exaperf_basic_stat_t;

exaperf_basic_stat_t
exaperf_compute_basic_stat(const double values[], unsigned int nb_values);

#endif /* EXAPERF_STATS_H */
