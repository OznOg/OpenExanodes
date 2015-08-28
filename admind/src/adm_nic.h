/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __ADM_NIC_H
#define __ADM_NIC_H

#include "os/include/os_inttypes.h"

struct adm_nic;

int adm_nic_new(const char *hostname, struct adm_nic **nic);

const char *adm_nic_ip_str(const struct adm_nic *nic);
const char *adm_nic_get_hostname(const struct adm_nic *nic);

void __adm_nic_free(struct adm_nic *nic);
#define adm_nic_free(nic) (__adm_nic_free(nic), nic = NULL)

#endif /* __ADM_NIC_H */
