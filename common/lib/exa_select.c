/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef WIN32
#include <sys/ioctl.h>
#endif

#include "common/include/exa_error.h"
#include "common/include/exa_select.h"
#include "common/lib/exa_common_kernel.h"
#include "common/include/exa_assert.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"

#if WIN32
const static struct timeval select_timeout = { .tv_sec = 0, .tv_usec = 500000 };
#endif

struct exa_select_handle
{
#if WIN32
#define EXA_SELECT_MAGIC 0x12332145
    uint32_t magic;
#else
    int fd;
#endif
};

exa_select_handle_t *exa_select_new_handle(void)
{
    exa_select_handle_t *h = os_malloc(sizeof(exa_select_handle_t));
    EXA_ASSERT(h != NULL);
#if WIN32
    h->magic = EXA_SELECT_MAGIC;
#else
    h->fd = open(EXACOMMON_MODULE_PATH, O_RDWR);
    EXA_ASSERT_VERBOSE(h->fd >= 0, "Cannot bind to exa_common module: %s (%d)",
                       os_strerror(errno), -errno);

    EXA_ASSERT_VERBOSE(ioctl(h->fd, EXA_SELECT_MAL) != -1,
                       "Cannot register to exa_common module: %s (%d)",
                       os_strerror(errno), -errno);
#endif
    return h;
}

void exa_select_delete_handle(exa_select_handle_t *h)
{
    if (h == NULL)
        return;

#if WIN32
    EXA_ASSERT_VERBOSE(h->magic == EXA_SELECT_MAGIC,
                       "Corrupted handle detected %d", h->magic);
#else
    close(h->fd);
#endif

    os_free(h);
}

int exa_select_in(exa_select_handle_t *h, fd_set *set)
{
#if WIN32
    int nb_sock;

    EXA_ASSERT_VERBOSE(h->magic == EXA_SELECT_MAGIC,
                       "Corrupted handle detected %d", h->magic);

    nb_sock = os_select(0 /* ignored */, set, NULL, NULL, &select_timeout);
    if (nb_sock == 0)/* timeout is reached */
    {
        /* reset set because there was actually no event on sockets */
        FD_ZERO(set);
        return -EFAULT;
    }

    return nb_sock > 0 ? 0 : nb_sock;
#else
    if (ioctl(h->fd, EXA_SELECT_IN, set) == -1)
	return -errno;

    return 0;
#endif
}

int exa_select_out(exa_select_handle_t *h, fd_set *set)
{
#if WIN32
    int nb_sock;

    EXA_ASSERT_VERBOSE(h->magic == EXA_SELECT_MAGIC,
                       "Corrupted handle detected %d", h->magic);

    nb_sock = os_select(0 /* ignored */, NULL, set, NULL, &select_timeout);

    if (nb_sock == 0) /* timeout is reached */
    {
        /* reset set because there was actually no event on sockets */
        FD_ZERO(set);
        return -EFAULT;
    }

    return nb_sock > 0 ? 0 : nb_sock;
#else
    if (ioctl(h->fd, EXA_SELECT_OUT, set) == -1)
	return -errno;

    return 0;
#endif
}
