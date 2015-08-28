/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/common/include/spof.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_nodeset.h"
#include "common/include/exa_assert.h"
#include "os/include/os_stdio.h"
#include "os/include/os_error.h"

int spof_id_from_str(spof_id_t *spof_id, const char *str)
{
    spof_id_t id;
    int err;

    err = to_uint32(str, &id);

    if (err != 0)
        return err;

    if (!SPOF_ID_IS_VALID(id))
        return -EINVAL;

    *spof_id = id;

    return 0;
}

const char *spof_id_to_str(spof_id_t spof_id)
{
    static __thread char buf[MAXLEN_UINT32 + 1];

    if (!SPOF_ID_IS_VALID(spof_id))
        return NULL;

    if (os_snprintf(buf, sizeof(buf), "%"PRIspof_id, spof_id) >= sizeof(buf))
        return NULL;

    return buf;
}

void spof_init(spof_t *spof)
{
    EXA_ASSERT(spof != NULL);

    spof->id = SPOF_ID_NONE;

    exa_nodeset_reset(&spof->nodes);
}

int spof_set_id(spof_t *spof, spof_id_t id)
{
    EXA_ASSERT(spof != NULL);

    if (!SPOF_ID_IS_VALID(id))
        return -EINVAL;

    spof->id = id;
    return EXA_SUCCESS;
}

spof_id_t spof_get_id(const spof_t *spof)
{
    EXA_ASSERT(spof != NULL);

    return spof->id;
}

unsigned int spof_get_num_nodes(const spof_t *spof)
{
    EXA_ASSERT(spof != NULL);

    return exa_nodeset_count(&spof->nodes);
}

int spof_get_nodes(const spof_t *spof, exa_nodeset_t *nodes)
{
    EXA_ASSERT(spof != NULL);

    if (nodes == NULL)
        return -EINVAL;

    exa_nodeset_copy(nodes, &spof->nodes);
    return 0;
}

void spof_add_node(spof_t *spof, const exa_nodeid_t node)
{
    EXA_ASSERT(spof != NULL);

    exa_nodeset_add(&spof->nodes, node);
}

void spof_remove_node(spof_t *spof, const exa_nodeid_t node)
{
    EXA_ASSERT(spof != NULL);

    exa_nodeset_del(&spof->nodes, node);
}

bool spof_contains_node(const spof_t *spof, const exa_nodeid_t node)
{
    EXA_ASSERT(spof != NULL);

    return exa_nodeset_contains(&spof->nodes, node);
}

void spof_copy(spof_t *dest, const spof_t *src)
{
    EXA_ASSERT(dest != NULL);
    EXA_ASSERT(src != NULL);

    spof_init(dest);

    dest->id = src->id;
    exa_nodeset_copy(&dest->nodes, &src->nodes);
}

const spof_t *spof_lookup(const spof_t *spofs, unsigned num_spofs, spof_id_t id)
{
    unsigned i;

    EXA_ASSERT(spofs != NULL);
    EXA_ASSERT(id != SPOF_ID_NONE);

    for (i = 0; i < num_spofs; i++)
        if (spofs[i].id == id)
            return &spofs[i];

    return NULL;
}
