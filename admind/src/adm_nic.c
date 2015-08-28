/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/adm_nic.h"

#include <string.h>

#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_network.h"
#include "os/include/strlcpy.h"

struct adm_nic
{
    char hostname[EXA_MAXSIZE_HOSTNAME + 1];

    struct in_addr address;
};

int adm_nic_new(const char *hostname, struct adm_nic **_nic)
{
    int err;
    struct adm_nic *nic;
    EXA_ASSERT(_nic);

    nic = os_malloc(sizeof(struct adm_nic));
    if (nic == NULL)
	return -ENOMEM;

    strlcpy(nic->hostname, hostname, sizeof(nic->hostname));

    err = os_host_addr(hostname, &nic->address);
    if (err != 0)
    {
        os_free(nic);
        return -NET_ERR_INVALID_HOST;
    }

    *_nic = nic;

    return EXA_SUCCESS;
}

void
__adm_nic_free(struct adm_nic *nic)
{
  EXA_ASSERT(nic);
  os_free(nic);
}

const char *adm_nic_ip_str(const struct adm_nic *nic)
{
    return os_inet_ntoa(nic->address);
}

const char *adm_nic_get_hostname(const struct adm_nic *nic)
{
    return nic->hostname;
}

