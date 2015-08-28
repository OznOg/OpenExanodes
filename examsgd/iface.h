/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef H_IFACE
#define H_IFACE

#include "os/include/os_network.h"

int examsgIfaceConfig(int sockfd, const char *hostname, struct in_addr *addr);

#endif /* H_IFACE */
