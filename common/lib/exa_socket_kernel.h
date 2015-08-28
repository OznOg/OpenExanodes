/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef _EXA_SOCKET_KERNEL_H
#define _EXA_SOCKET_KERNEL_H

#include <net/sock.h>

struct socket *exa_getsock(int fd);
int exa_socket_set_atomic_kernel(int fd);
void exa_socket_tweak_emergency_pool_kernel(int size);

#endif /* _EXA_SOCKET_KERNEL_H */
