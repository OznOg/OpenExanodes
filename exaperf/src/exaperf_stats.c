/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <math.h>
#include "exaperf/src/exaperf_stats.h"

exaperf_basic_stat_t
exaperf_compute_basic_stat(const double values[], unsigned int nb_values)
{
    exaperf_basic_stat_t ret;
    int i, less, greater, equal;
    double min, max, sum, sum_square, guess, maxltguess, mingtguess;

    if (nb_values == 0)
    {
	ret.min = ret.max = ret.median = ret.mean = ret.std_dev = 0;
	return ret;
    }


    min = values[0];
    max = values[0];
    sum = values[0];
    sum_square = values[0]* values[0];
    for (i=1; i<nb_values; i++)
    {
        if (values[i] < min)
	    min = values[i];
        if (values[i] > max)
	    max = values[i];
	sum += values[i];
	sum_square += values[i]*values[i];
    }

    ret.min = min;
    ret.max = max;
    ret.mean = sum/nb_values;
    ret.std_dev = sqrt( sum_square/nb_values - (sum/nb_values)*(sum/nb_values));

    while (1)
    {
        guess = (min + max) / 2;
        less = 0;
	greater = 0;
	equal = 0;
        maxltguess = min;
        mingtguess = max;

        for (i=0; i < nb_values; i++)
	{
            if (values[i]<guess)
	    {
                less++;
                if (values[i] > maxltguess)
		    maxltguess = values[i];
            }
	    else if (values[i] > guess)
	    {
                greater++;
                if (values[i] < mingtguess)
		    mingtguess = values[i];
            }
	    else
		equal++;
        }

        if (less <= (nb_values+1)/2 && greater <= (nb_values+1)/2)
	    break ;
        else if (less > greater)
	    max = maxltguess;
        else
	    min = mingtguess;
    }

    if (less >= (nb_values+1)/2)
	ret.median = maxltguess;
    else if (less+equal >= (nb_values+1)/2)
	ret.median = guess;
    else
	ret.median = mingtguess;

    return ret;
}
