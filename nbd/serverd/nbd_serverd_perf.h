/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef SERVERD_PERF
#define SERVERD_PERF

#include "exaperf/include/exaperf.h"
#include "common/include/exa_constants.h"

typedef struct {
    bool     read;               /**< Is IO a read operation ? */
    uint64_t header_submit_date; /**< Date of the header reception in serverd */
    uint64_t data_submit_date;   /**< Date of the data reception in serverd   */
} serv_perf_t;

#ifdef WITH_PERF

int  __serverd_perf_init(void);
void __serverd_perf_cleanup(void);
void __serverd_perf_sensor_init(void);
void __serverd_perf_make_request(serv_perf_t *serv_perf, bool read,
                                 uint64_t sector, uint64_t sector_nb);
void __serverd_perf_end_request(const serv_perf_t *serv_perf);

#define serverd_perf_init()           __serverd_perf_init()
#define serverd_perf_cleanup()        __serverd_perf_cleanup()
#define serverd_perf_sensor_init()    __serverd_perf_sensor_init()
#define serverd_perf_make_request(serv_perf, read, sector, sector_nb) \
      __serverd_perf_make_request(serv_perf, read, sector, sector_nb)
#define serverd_perf_end_request(serv_perf)  __serverd_perf_end_request(serv_perf)

#else

#define serverd_perf_init() EXA_SUCCESS
#define serverd_perf_cleanup()
#define serverd_perf_sensor_init()
#define serverd_perf_make_request(serv_perf, read, sector, sector_nb)
#define serverd_perf_end_request(serv_perf)
#endif

#endif
