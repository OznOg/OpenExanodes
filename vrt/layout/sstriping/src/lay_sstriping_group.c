/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h> /* for memset */

#include "os/include/os_mem.h"
#include "vrt/layout/sstriping/src/lay_sstriping.h"
#include "vrt/layout/sstriping/src/lay_sstriping_group.h"

sstriping_group_t *sstriping_group_alloc(void)
{
    sstriping_group_t *lg;

    lg = os_malloc(sizeof(sstriping_group_t));
    if (lg == NULL)
	return NULL;

    memset(lg, 0, sizeof(sstriping_group_t));

    assembly_group_init(&lg->assembly_group);

    return lg;
}

void __sstriping_group_free(sstriping_group_t *ssg, const storage_t *storage)
{
    if (ssg == NULL)
        return;

    /* FIXME shouldn't we free the subspaces ? */

    assembly_group_cleanup(&ssg->assembly_group);

    os_free(ssg);
}

bool sstriping_group_equals(const sstriping_group_t *ssg1,
                            const sstriping_group_t *ssg2)
{
    if (ssg1->su_size != ssg2->su_size)
        return false;
    if (ssg1->logical_slot_size != ssg2->logical_slot_size)
        return false;

    if (!assembly_group_equals(&ssg1->assembly_group, &ssg2->assembly_group))
        return false;

    return true;
}
