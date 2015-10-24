/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef NBD_CLIENTD_PERF_PRIVATE_H
#define NBD_CLIENTD_PERF_PRIVATE_H

#include "nbd/common/nbd_common.h"

#include "exaperf/include/exaperf.h"

#include "common/include/uuid.h"

struct ndev_perf
{
    /* Indexed by __READ and __WRITE */
    exaperf_sensor_t *clientd_dur[2];
};

typedef struct {
    bool     read;        /**< is IO a read operation ? */
    uint64_t submit_date; /**< Date of the request reception in clientd */
} perf_data_t;

#ifdef WITH_PERF

int __clientd_perf_init(void);
void __clientd_perf_cleanup(void);

void __clientd_perf_dev_init(struct ndev_perf *perf_info, const exa_uuid_t *uuid);

void __clientd_perf_make_request(perf_data_t *io_perf, bool read);
void __clientd_perf_end_request(struct ndev_perf *dev_perf, const perf_data_t *io_perf);

#define clientd_perf_init()     __clientd_perf_init()
#define clientd_perf_cleanup()  __clientd_perf_cleanup()

#define clientd_perf_dev_init(perf_info, uuid)  __clientd_perf_dev_init(perf_info, uuid)

#define clientd_perf_make_request(io, read)  __clientd_perf_make_request(io, read)
#define clientd_perf_end_request(dev_perf, io_perf)   __clientd_perf_end_request(dev_perf, io_perf)

#else /* WITH_PERF */

#define clientd_perf_init()     0
#define clientd_perf_cleanup()

#define clientd_perf_dev_init(perf_info, uuid)

#define clientd_perf_make_request(io, read)
#define clientd_perf_end_request(dev_perf, io_perf)

#endif  /* WITH_PERF */

#endif /* NBD_CLIENTD_PERF_PRIVATE_H */
