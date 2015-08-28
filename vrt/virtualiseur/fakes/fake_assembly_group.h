/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef FAKE_ASSEMBLY_GROUP_H
#define FAKE_ASSEMBLY_GROUP_H

#include "vrt/assembly/src/assembly_group.h"
#include "vrt/virtualiseur/include/storage.h"

#include "os/include/os_inttypes.h"

/* XXX Assumes one rdev per spof in the storage */
assembly_group_t *make_fake_ag(const storage_t *sto, uint32_t slot_width);

#endif /* FAKE_ASSEMBLY_GROUP_H */
