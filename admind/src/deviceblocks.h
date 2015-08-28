/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __DEVICEBLOCKS_H
#define __DEVICEBLOCKS_H

#include "os/include/os_inttypes.h"


struct adm_volume;

/**
 * Set the readahead value of the block device corresponding to the
 * given volume.
 *
 * @param[in,out] volume     Volume whose readahead value must be set
 * @param[in]     readahead  Readahead value in KB
 *
 * @return EXA_SUCCESS or a negative error code
 */
int admind_voldeviceblock_set_readahead(struct adm_volume *volume,
                                        uint32_t readahead);

#endif
