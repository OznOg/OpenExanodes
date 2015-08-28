/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/virtualiseur/fakes/fake_assembly_group.h"
#include "vrt/virtualiseur/fakes/fake_rdev.h"

#include "os/include/os_mem.h"

assembly_group_t *make_fake_ag(const storage_t *sto, uint32_t slot_width)
{
    assembly_group_t *ag;
    int err;

    /* FIXME Should have an assembly_group_alloc() */
    ag = os_malloc(sizeof(assembly_group_t));
    if (ag == NULL)
        return NULL;

    assembly_group_init(ag);

    err = assembly_group_setup(ag, slot_width, sto->chunk_size);
    if (err != 0)
        goto failed;

    return ag;

failed:
    assembly_group_cleanup(ag);
    os_free(ag);
    return NULL;
}
