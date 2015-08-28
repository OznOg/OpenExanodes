/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**
 * Sockets with atomic memory allocation.
 */

#ifndef _EXA_SOCKET_H
#define _EXA_SOCKET_H

typedef enum socket_operation {
    SOCKET_RECV,
    SOCKET_SEND,
} socket_operation_t;

#ifdef WIN32

#include "common/include/exa_error.h"

/* There is no need to tweak memory allocation behaviour of sockets on Windows. */

#define exa_socket_tweak_emergency_pool(size) EXA_SUCCESS
#define exa_socket_set_atomic(socket) EXA_SUCCESS

#else /* WIN32 */

#include <sys/select.h> /* for fd_set */

/** Tweak the size of the kernel emergency memory pool */
int exa_socket_tweak_emergency_pool(int size);

/** Allocate memory with the GFP_ATOMIC flag for this socket */
int exa_socket_set_atomic(int socket);

#endif /* WIN32 */

#endif /* _EXA_SOCKET_H */
