/*
 * Copyright 2002, 2011 Seanodes IT http://www.seanodes.com. All rights
 * reserved and protected by French, U.S. and other countries' copyright laws.
 */

#include "common/include/exa_perf_instance.h"

#include "log/include/log.h"

#include "os/include/os_stdio.h"
#include "os/include/os_error.h"

static exaperf_t *eh = NULL;

exaperf_t *exa_perf_instance_get(void)
{
    return eh;
}

static void exa_perf_instance_print(const char *fmt, ...)
{
    va_list ap;
    char log[EXALOG_MSG_MAX + 1];

    va_start(ap, fmt);
    os_vsnprintf(log, EXALOG_MSG_MAX + 1, fmt, ap);
    va_end(ap);

    exalog_info("%s", log);
}

int exa_perf_instance_static_init(void)
{
    const char *perf_config;
    exaperf_err_t err;

    eh = exaperf_alloc();
    if (eh == NULL)
    {
        exalog_error("Failed initializing exaperf");
        return -ENOMEM;
    }

    perf_config = getenv("EXANODES_PERF_CONFIG");
    if (perf_config == NULL)
    {
        exalog_debug("No perf config set");
        return 0;
    }

    /* initialize the component */
    err = exaperf_init(eh, perf_config, exa_perf_instance_print);
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
        return -EINVAL;
    }
}

void exa_perf_instance_static_clean(void)
{
    exaperf_free(eh);
    eh = NULL;
}

