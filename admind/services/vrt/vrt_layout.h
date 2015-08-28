/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef VRT_LAYOUT_H
#define VRT_LAYOUT_H

#include "admind/src/adm_error.h"
#include "common/include/uuid.h"
#include "os/include/os_inttypes.h"
#include "vrt/common/include/spof.h"

typedef enum {
#define VRT_LAYOUT_FIRST VRT_LAYOUT_SSTRIPING
    VRT_LAYOUT_SSTRIPING = 356,
    VRT_LAYOUT_RAIN1,
    VRT_LAYOUT_RAINX,
#define VRT_LAYOUT_LAST VRT_LAYOUT_RAINX
} vrt_layout_t;

#define VRT_LAYOUT_IS_VALID(layout) \
    ((layout) >= VRT_LAYOUT_FIRST && (layout) <= VRT_LAYOUT_LAST)
#define VRT_LAYOUT_INVALID VRT_LAYOUT_LAST + 3

#define VRT_NUM_LAYOUTS (VRT_LAYOUT_LAST - VRT_LAYOUT_FIRST + 1)

/**
 * Get a layout from its name
 *
 * @param[in]   layout_name   The layout name
 *
 * @return The corresponding layout, or VRT_LAYOUT_INVALID if it
 * doesn't exist.
 */
vrt_layout_t vrt_layout_from_name(const char *layout_name);

/**
 * Get a layout name
 *
 * @param[in]   layout_id  The layout id
 *
 * @return The corresponding layout name, or NULL if it doesn't
 * exist.
 */
const char *vrt_layout_get_name(vrt_layout_t layout_id);

/**
 * Validate whether group creation is possible given its topology.
 *
 * @param[in]   group           The group description
 * @param[in]   slot_width      The slot width
 * @param[in]   nb_spare        The number of spares
 * @param[out]  err_desc        The error to send back to the user
 *
 * @return 0 if successful, a negative error code if the group can not be
 * created as is.
 */
int vrt_layout_validate_disk_group_rules(vrt_layout_t layout,
                                    exa_uuid_t disk_uuids[], int num_disks,
                                    uint32_t slot_width, uint32_t nb_spare,
                                    cl_error_desc_t *err_desc);

bool rainX_rule_replication_satisfied(uint32_t slot_width, uint32_t nb_spare,
                                      cl_error_desc_t *err_desc);

bool rainX_rule_administrability_satisfied(const spof_id_t *involved_spof_ids,
                                           unsigned num_involved_spof_ids,
                                           exa_nodeset_t nodes_in_spof[],
                                           unsigned num_nodes_in_spof,
                                           uint32_t nb_spare,
                                           cl_error_desc_t *err_desc);
bool rainX_rule_quorum_satisfied(const spof_id_t *all_spof_ids,
                                 unsigned num_all_spof_ids,
                                 exa_nodeset_t nodes_in_spof[],
                                 unsigned num_nodes_in_spof,
                                 uint32_t nb_spare,
                                 uint32_t num_nodes_in_cluster,
                                 cl_error_desc_t *err_desc);

#endif /* VRT_LAYOUT_H */
