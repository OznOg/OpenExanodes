/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __SERVICE_MONITOR__H
#define __SERVICE_MONITOR__H

struct adm_cluster_monitoring;


int monitor_start(int thr_nb, struct adm_cluster_monitoring *params);

int monitor_stop(int thr_nb);


#endif
