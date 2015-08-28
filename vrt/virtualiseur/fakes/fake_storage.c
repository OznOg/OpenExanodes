/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/virtualiseur/fakes/fake_storage.h"

#include <stdlib.h>

storage_t *make_fake_storage(uint32_t num_spof_groups, uint32_t chunk_size,
                             struct vrt_realdev *rdevs[], unsigned num_rdevs)
{
    storage_t *sto;
    uint32_t i;

    sto = storage_alloc();
    if (sto == NULL)
        return NULL;

    for (i = 0; i < num_spof_groups; i++)
    {
        spof_id_t spof_id = i + 1;

        if (storage_add_rdev(sto, spof_id, rdevs[i]) != 0)
            goto failed;
    }

    if (storage_cut_in_chunks(sto, chunk_size) < 0)
        goto failed;

    return sto;

failed:
    storage_free(sto);
    return NULL;
}
