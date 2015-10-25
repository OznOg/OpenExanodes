/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**
 * Select with no memory allocation.
 */

#ifndef _EXA_SELECT_H
#define _EXA_SELECT_H

#include "os/include/os_network.h" /* for fd_set */

typedef struct exa_select_handle exa_select_handle_t;

/**
 * Get a handle that can be used with exa_select_in()
 * and exa_select_out().
 *
 * @return a handle on the exa_select framework.
 */
exa_select_handle_t *exa_select_new_handle(void);

/**
 * Delete a handle previously allocated with exa_select_new_handle()
 * @param[in]     h    handle allocated with exa_select_new_handle()
 */
void exa_select_delete_handle(exa_select_handle_t *h);

/**
 * Like a normal select but with no kmalloc in kernel mode.
 * Only works with sockets.
 * exa_select_in must be used for socket that expect incoming data (recv).
 *
 * @param[in]     h    handle allocated with exa_select_new_handle()
 * @param[in]     nfds the highest-numbered file descriptor in any of the three
 *                     sets, plus 1.
 * @param[in,out] set in : set of file descriptors to watch
 *                    out: set of file descriptors changed
 *
 * @return 0 if successfull, a negative error code otherwise
 *           There is an implicit timeout for this function, when it is
 *           reached and no fd has input data, -EFAULT is returned, in
 *           that case, 'set' is afforded to be empty.
 */
int exa_select_in(exa_select_handle_t *h, int nfds, fd_set *set);

/**
 * Same than \@exa_select_in but for sockets that are used for output (send).
 */
int exa_select_out(exa_select_handle_t *h, int nfds, fd_set *set);

#endif /* _EXA_SELECT_H */
