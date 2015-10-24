/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "nbd/clientd/src/nbd_clientd_perf_private.h"

#include "log/include/log.h"

#include "common/include/exa_perf_instance.h"

#include "os/include/os_error.h"
#include "os/include/os_stdio.h"
#include "os/include/os_time.h"

#include <stdlib.h>
#include <stdarg.h>

#define __READ  0
#define __WRITE 1

void __clientd_perf_dev_init(struct ndev_perf *perf_infos, const exa_uuid_t *uuid)
{
    exa_uuid_str_t ndev_uuid_str;
    exaperf_t *eh = exa_perf_instance_get();

    uuid2str(uuid, ndev_uuid_str);

    perf_infos->clientd_dur[__READ] =
	exaperf_duration_init_from_template(eh, "NBD_CLIENT_DUR_READ",
					    ndev_uuid_str, true);

    perf_infos->clientd_dur[__WRITE] =
	exaperf_duration_init_from_template(eh, "NBD_CLIENT_DUR_WRITE",
					    ndev_uuid_str, true);
}

void __clientd_perf_make_request(perf_data_t *io_perf, bool read)
{
    io_perf->read        = read;
    io_perf->submit_date = os_gettimeofday_msec();
}

void __clientd_perf_end_request(struct ndev_perf *dev_perf,
                                const perf_data_t *io_perf)
{
    double elapsed = (double)(os_gettimeofday_msec()) - io_perf->submit_date;
    int rw = io_perf->read ? __READ : __WRITE;

    exaperf_duration_record(dev_perf->clientd_dur[rw], elapsed);
}

