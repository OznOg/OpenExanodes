/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "nbd/clientd/src/nbd_clientd_perf_private.h"

#include "log/include/log.h"

#include "common/include/exa_env.h"

#include "os/include/os_error.h"
#include "os/include/os_stdio.h"
#include "os/include/os_time.h"

#include <stdlib.h>
#include <stdarg.h>

static exaperf_t *eh = NULL;

#define __READ  0
#define __WRITE 1

static void clientd_perf_print(const char *fmt, ...)
{
    va_list ap;
    char log[EXALOG_MSG_MAX + 1];

    va_start(ap, fmt);
    os_vsnprintf(log, EXALOG_MSG_MAX + 1, fmt, ap);
    va_end(ap);

    exalog_info("%s", log);
}

int __clientd_perf_init(void)
{
    const char *perf_config;
    exaperf_err_t err;

    /* XXX Refactor exaperf initialization: all components will load the
           same config file (as set by EXANODES_PERF_CONFIG). Add ability
           to handle included files in exaperf. */
    eh = exaperf_alloc();
    if (eh == NULL)
    {
        exalog_error("Failed initializing exaperf");
        return -ENOMEM;
    }

    perf_config = exa_env_perf_config();
    if (perf_config == NULL)
    {
        exalog_debug("No perf config set");
        return 0;
    }

    err = exaperf_init(eh, perf_config, clientd_perf_print);
    switch (err)
    {
    case EXAPERF_SUCCESS:
        exalog_info("Loaded perf config '%s'", perf_config);
        return 0;

    case EXAPERF_CONF_FILE_OPEN_FAILED:
        exalog_warning("Perf config '%s' not found, ignored", perf_config);
        exaperf_free(eh);
        eh = NULL;
        return 0;

    default:
        /* FIXME Use error string instead of error code */
        exalog_error("Failed loading perf config '%s' (%d)", perf_config, err);
    }

    /* XXX Return more meaningful error */
    return -EINVAL;
}

void __clientd_perf_cleanup(void)
{
    exaperf_free(eh);
    eh = NULL;
}

exaperf_t *nbd_clientd_get_exaperf(void)
{
    return eh;
}

void __clientd_perf_dev_init(struct ndev_perf *perf_infos, const exa_uuid_t *uuid)
{
    exa_uuid_str_t ndev_uuid_str;

    uuid2str(uuid, ndev_uuid_str);

    perf_infos->clientd_dur[__READ] =
	exaperf_duration_init_from_template(eh, "NBD_CLIENT_DUR_READ",
					    ndev_uuid_str, true);

    perf_infos->clientd_dur[__WRITE] =
	exaperf_duration_init_from_template(eh, "NBD_CLIENT_DUR_WRITE",
					    ndev_uuid_str, true);
}

void __clientd_perf_make_request(header_t *req_header)
{
    if (eh == NULL)
        return;

    req_header->submit_date = os_gettimeofday_msec();
}

void __clientd_perf_end_request(struct ndev_perf *perf_infos, header_t *req_header)
{
    double elapsed;
    int rw = -1;

    EXA_ASSERT(NBD_REQ_TYPE_IS_VALID(req_header->request_type));
    switch (req_header->request_type)
    {
    case NBD_REQ_TYPE_READ:
        rw = __READ;
        break;
    case NBD_REQ_TYPE_WRITE:
        rw = __WRITE;
        break;
    case NBD_REQ_TYPE_LOCK:
    case NBD_REQ_TYPE_UNLOCK:
        EXA_ASSERT(false);
        /* FIXME: formerly this case was not handled */
    }

    if (eh == NULL)
        return;

    elapsed = (double)(os_gettimeofday_msec() - req_header->submit_date);
    exaperf_duration_record(perf_infos->clientd_dur[rw], elapsed);
}

