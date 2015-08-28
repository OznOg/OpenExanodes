/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef _NBD_SERVERD_NBD_DISK_THREAD_H
#define _NBD_SERVERD_NBD_DISK_THREAD_H

#include "nbd/serverd/nbd_serverd.h"

#define TD_START_NOTHING	0
#define TD_START_WAITING_REQ	1
#define TD_START_PROCESSING	3


extern int exa_disk_lock_zone(device_t *dev, long first_sector,
			      long size_in_sector);
extern int exa_disk_unlock_zone(device_t *dev, long first_sector,
			      long size_in_sector);

extern void exa_td_main(void *p);


#endif /* _NBD_SERVERD_NBD_DISK_THREAD_H */
