/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_LAYOUT_GROUP_H__
#define __VRT_LAYOUT_GROUP_H__

#include "vrt/assembly/src/assembly_group.h"

/**
 * Structure containing the layout-specific information stored in
 * memory for each group.
 */
typedef struct
{
    /** group assemblies. */
    assembly_group_t assembly_group;

    /** Size of a striping unit (in sectors) */
    uint32_t su_size;

    /** Logical slot size. In the sstriping layout, the logical slot
     *  size is equal to the physical slot size.
     *  @f[
     *  slot\_size = chunk\_size \times slot\_width
     *  @f]
     *  The logical slot size is stored in this structure in order to
     *  avoid to perform the computation every time it is used.
     */
    uint64_t logical_slot_size;
} sstriping_group_t;

#define SSTRIPING_GROUP(group) ((sstriping_group_t *)(group)->layout_data)

sstriping_group_t *sstriping_group_alloc(void);
void __sstriping_group_free(sstriping_group_t *ssg, const storage_t *storage);
#define sstriping_group_free(ssg, storage) \
    (__sstriping_group_free((ssg), (storage)), ssg = NULL)

bool sstriping_group_equals(const sstriping_group_t *ssg1,
                            const sstriping_group_t *ssg2);

#endif /* __VRT_LAYOUT_GROUP_H__ */
