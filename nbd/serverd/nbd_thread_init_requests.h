/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef _NBD_SERVERD_NBD_THREAD_INIT_REQUESTS_H
#define _NBD_SERVERD_NBD_THREAD_INIT_REQUESTS_H

#include "common/include/exa_constants.h"

#define TI_MAX_ALGO_HEADER 128

#define TI_MAX_SEQ	16
#define TI_MAX_SEEK	16

struct ti_seq_stat
  {
    int tot_iter;
    unsigned long long last_sector[NBMAX_DISKS_PER_NODE];
    int seq_size[NBMAX_DISKS_PER_NODE];
    char disk_used[NBMAX_DISKS_PER_NODE];
    int seq_size_tab[NBMAX_DISKS_PER_NODE][TI_MAX_SEQ];
    int seq_seek_tab[NBMAX_DISKS_PER_NODE][TI_MAX_SEEK];
  };

extern void exa_ti_main(void *p);

#endif /* _NBD_SERVERD_NBD_THREAD_INIT_REQUESTS_H */
