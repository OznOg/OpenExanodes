/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef SYS_BLOCKDEV_H
#define SYS_BLOCKDEV_H

#include "blockdevice/include/blockdevice.h"

/**
 * Open a system blockdevice.
 *
 * @param bdev      The resulting blockdevice
 * @param path      The system path
 * @param access    The access mode
 *
 * @Note: Warning: this allocates on I/O path.
 *
 * @return 0 if successful.
 */
int sys_blockdevice_open(blockdevice_t **bdev, const char *path,
                         blockdevice_access_t access);

#endif /* SYS_BLOCKDEV_H */
