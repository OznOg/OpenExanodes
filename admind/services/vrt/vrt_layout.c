/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/services/vrt/vrt_layout.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_cluster.h"
#include "common/include/exa_error.h"
#include "common/include/exa_nodeset.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_math.h"
#include "os/include/os_string.h"
#include "os/include/os_stdio.h"
#include "vrt/common/include/spof.h"

struct _layout_names {
  vrt_layout_t  id;
  char          *name;
};

static struct _layout_names layout_names[VRT_NUM_LAYOUTS] = {
    { VRT_LAYOUT_SSTRIPING, "sstriping" },
    { VRT_LAYOUT_RAIN1,     "rain1" },
    { VRT_LAYOUT_RAINX,     "rainX" },
};

vrt_layout_t vrt_layout_from_name(const char *layout_name)
{
    int i;
    for (i = 0; i < VRT_NUM_LAYOUTS; i++)
        if (!os_strcasecmp(layout_name, layout_names[i].name))
            return layout_names[i].id;

    return VRT_LAYOUT_INVALID;
}

const char *vrt_layout_get_name(vrt_layout_t layout_id)
{
    int i;
    for (i = 0; i < VRT_NUM_LAYOUTS; i++)
        if (layout_id == layout_names[i].id)
            return layout_names[i].name;

    return NULL;
}


/** Info needed for sorting SPOFs */
typedef struct
{
    spof_id_t id;        /**< SPOF id */
    uint32_t  nb_nodes;  /**< Number of nodes in the SPOF */
} spof_info_t;

/* Comparison function for ordering SPOF info by decreasing number of nodes */
static int spof_info_gt(const void *a, const void *b)
{
    const spof_info_t *si1 = a;
    const spof_info_t *si2 = b;

    if (si1->nb_nodes > si2->nb_nodes)
        return -1;

    if (si1->nb_nodes < si2->nb_nodes)
        return +1;

    return si1->id - si2->id; /* Make sure the sort will be always the same */
}

/**
 * Build a SPOF info array sorted by decreasing size (in terms of number of
 * nodes).
 *
 * @param[in]  involved_spof_ids      Ids of SPOFs to consider
 * @param[in]  num_involved_spof_ids  Number of SPOFS to consider
 * @param[in]  nodes_in_spof          Nodesets of nodes in a SPOF group
 * @param[in]  num_nodes_in_spof      Size of the nodes_in_spof table
 * @param[out] spof_infos             Resulting sorted SPOF info (must be able to
 *                                    contain num_involved_spof_ids SPOF infos)
 */
static void build_sorted_spof_info_array(const spof_id_t *involved_spof_ids,
                                         unsigned num_involved_spof_ids,
                                         exa_nodeset_t nodes_in_spof[],
                                         unsigned num_nodes_in_spof,
                                         spof_info_t *spof_infos)
{
    uint32_t i;

    for (i = 0; i < num_involved_spof_ids; i++)
    {
        exa_nodeset_t nodes;

        EXA_ASSERT(num_nodes_in_spof > involved_spof_ids[i]);
        nodes = nodes_in_spof[involved_spof_ids[i]];

        spof_infos[i].id = involved_spof_ids[i];
        spof_infos[i].nb_nodes = exa_nodeset_count(&nodes);
    }

    qsort(spof_infos, num_involved_spof_ids, sizeof(spof_info_t), spof_info_gt);
}

/**
 * Check whether the replication rule is satisfied.
 *
 * @param[in]  slot_width  The slot width of the assembly
 * @param[in]  nb_spare    The desired number of spare
 * @param[out] error_msg   Error message
 *
 * @return true if the rule is satisfied, false otherwise
 */
bool rainX_rule_replication_satisfied(uint32_t slot_width, uint32_t nb_spare,
                                             cl_error_desc_t *err_desc)
{
    /* Replication rule: ensure that each slot contains at least
     * "2 + nb_spare" chunks.
     */
    if (slot_width < 2 + nb_spare)
    {
        set_error(err_desc, -VRT_ERR_LAYOUT_CONSTRAINTS_INFRINGED,
                  "Group contains less than %u chunks per slot."
                  " Can't mirror and get %u spare(s) in this situation.",
                  2 + nb_spare, nb_spare);
        return false;
    }

    return true;
}

/**
 * Check whether the administrability rule is satisfied.
 *
 * @param[in]  involved_spof_ids      Ids of SPOFs involved in the group
 *                                    (providing at least one disk)
 * @param[in]  num_involved_spof_ids  Number of SPOF ids involved
 * @param[in]  nodes_in_spof          Nodeset of nodes in the group, by spof ID
 * @param[in]  num_nodes_in_spof      Number of nodesets (nodes_in_spof)
 * @param[in]  nb_spare               Number of spares
 * @param[out] err_desc               Error message in case of error
 *
 * @return true if the rule is satisfied, false otherwise
 */
bool rainX_rule_administrability_satisfied(const spof_id_t *involved_spof_ids,
                                           unsigned num_involved_spof_ids,
                                           exa_nodeset_t nodes_in_spof[],
                                           unsigned num_nodes_in_spof,
                                           uint32_t nb_spare,
                                           cl_error_desc_t *err_desc)
{
    spof_info_t spof_infos[num_involved_spof_ids];
    uint32_t num_nodes_in_group, most_nodes_down;
    uint32_t min;
    uint32_t i;


    /* Sort the SPOF groups by number of nodes involved. */
    build_sorted_spof_info_array(involved_spof_ids, num_involved_spof_ids,
                                 nodes_in_spof, num_nodes_in_spof, spof_infos);

    /* Calculate the total number of nodes in the group (up or not) and the
       highest number of nodes we can lose if nb_spare + 1 spofs are down */
    num_nodes_in_group = 0;
    most_nodes_down = 0;

    for (i = 0; i < num_involved_spof_ids; i++)
    {
        num_nodes_in_group += spof_infos[i].nb_nodes;
        /* We tolerate the loss of N spof groups, N being 1+nb_spare
         * This means we must be able to lose all nodes in N spof groups
         */
        if (i < nb_spare + 1)
            most_nodes_down += spof_infos[i].nb_nodes;
    }

    /* We need at least half of the nodes involved the group to be
     * administrable. */
    min = quotient_ceil64(num_nodes_in_group, 2);

    if (num_nodes_in_group - most_nodes_down < min)
    {
        set_error(err_desc, -VRT_ERR_LAYOUT_CONSTRAINTS_INFRINGED,
                    "Group would be non-administrable in worst degraded case:"
                    " losing %"PRIu32" SPOF groups may leave less than %"PRIu32
                    " nodes in the disk group.",
                    nb_spare + 1, min);
        return false;
    }

    return true;
}


/**
 * Check whether the quorum rule is satisfied.
 *
 * @param[in]  involved_spof_ids      Ids of SPOFs involved in the cluster
 *                                    (whether they provide storage to the
 *                                    group or not !)
 * @param[in]  num_involved_spof_ids  Number of SPOF ids involved
 * @param[in]  all_nodes_in_spof      Nodesets of all nodes in SPOF groups
 *                                    providing storage to the group,
 *                                    sorted by SPOF id
 * @param[in]  num_all_nodes_in_spof  Number of spofs with nodes providing
 *                                    storage to the group
 * @param[in]  nb_spare               Number of spares defined for the group
 * @param[out] error_msg              Error message in case of error
 *
 * @return true if the rule is satisfied, false otherwise
 */
bool rainX_rule_quorum_satisfied(const spof_id_t *all_spof_ids,
                                 unsigned num_all_spof_ids,
                                 exa_nodeset_t all_nodes_in_spof[],
                                 unsigned num_all_nodes_in_spof,
                                 uint32_t nb_spare,
                                 uint32_t num_nodes_in_cluster,
                                 cl_error_desc_t *err_desc)
{
    spof_info_t spof_infos[num_all_spof_ids];
    uint32_t most_nodes_down;
    uint32_t min;
    uint32_t i;

    /* Special case for 2 nodes because we assume that then the cluster
     * is connected to a token manager, which allows the cluster to be
     * quorate even with one node dead */
    if (num_nodes_in_cluster == 2 && nb_spare == 0)
        return true;

    build_sorted_spof_info_array(all_spof_ids, num_all_spof_ids,
                                 all_nodes_in_spof, num_all_nodes_in_spof,
                                 spof_infos);

    /* Calculate the highest number of nodes we may lose if nb_spare + 1
       spofs are down */
    most_nodes_down = 0;
    for (i = 0; i < nb_spare + 1; i++)
        most_nodes_down += spof_infos[i].nb_nodes;

    /* XXX Quorum formula should be gotten from some "official" place */
    min = num_nodes_in_cluster / 2 + 1;

    if (num_nodes_in_cluster - most_nodes_down < min)
    {
        set_error(err_desc, -VRT_ERR_LAYOUT_CONSTRAINTS_INFRINGED,
                    "Can't become degraded without losing quorum in worst case:"
                    " losing %"PRIu32" SPOF groups may leave less than %"PRIu32
                    " nodes in the cluster.",
                    nb_spare + 1, min);
        return false;
    }

    return true;
}

static void __add_id_to_spof_table(spof_id_t spof_ids[], spof_id_t id, unsigned *num_ids)
{
    unsigned i;
    for (i = 0; i < *num_ids; i++)
        if (spof_ids[i] == id)
            return;

    *num_ids = i + 1;
    spof_ids[i] = id;
}

static int check_rainX_rules(exa_uuid_t disk_uuids[], int num_disks,
                              uint32_t slot_width, uint32_t nb_spare,
                              cl_error_desc_t *err_desc)
{
    struct adm_disk *disk;
    const struct adm_node *node;
    int i;

    /* A compact table containing every spof ID involved in the cluster */
    spof_id_t all_involved_spof_ids[SPOFS_MAX];
    unsigned num_all_involved_spof_ids;

    /* A compact table containing every spof ID providing at least one disk in
     * the group */
    spof_id_t involved_with_disk_spof_ids[SPOFS_MAX];
    unsigned num_involved_with_disk_spof_ids;

    /* A sparse table containing nodesets per spof ID. eg, the nodes in
     * spof group ID n are at nodes_with_disks_in_spof[n].
     * The nodesets describe nodes providing disks to the group.
     */
    exa_nodeset_t nodes_with_disks_in_spof[SPOFS_MAX];

    /* A sparse table containing nodesets per spof ID. eg, the nodes in
     * spof group ID n are at all_nodes_in_spof[n].
     * The nodesets describe all nodes in a spof group.
     */
    exa_nodeset_t all_nodes_in_spof[SPOFS_MAX];

    /* The total number of nodes in the cluster */
    unsigned num_nodes_in_cluster;

    for (i = 0; i < SPOFS_MAX; i++)
    {
        exa_nodeset_reset(&nodes_with_disks_in_spof[i]);
        exa_nodeset_reset(&all_nodes_in_spof[i]);
    }

    /* Compute the involved spofs overall (regardless of whether they have a
       disk used in the group or not) */
    num_all_involved_spof_ids = 0;
    num_nodes_in_cluster = 0;

    adm_cluster_for_each_node(node)
    {
        num_nodes_in_cluster++;
        __add_id_to_spof_table(all_involved_spof_ids, node->spof_id,
                               &num_all_involved_spof_ids);
        exa_nodeset_add(&all_nodes_in_spof[node->spof_id], node->id);
    }

    /* Compute spofs involved in the group (that have at least one disk used by
     * the group) */
    num_involved_with_disk_spof_ids = 0;
    for (i = 0; i < num_disks; i++)
    {
        disk = adm_cluster_get_disk_by_uuid(&disk_uuids[i]);
        EXA_ASSERT(disk != NULL);
        node = adm_cluster_get_node_by_id(disk->node_id);
        EXA_ASSERT(node != NULL);

        /* Add the spof group to the table of involved SPOFS */
        __add_id_to_spof_table(involved_with_disk_spof_ids, node->spof_id,
                &num_involved_with_disk_spof_ids);

        /* Update the involved nodeset in SPOF */
        exa_nodeset_add(&nodes_with_disks_in_spof[node->spof_id], node->id);
    }

    if (slot_width == 0)
        slot_width = MIN(num_involved_with_disk_spof_ids, 6);
        /* FIXME this computation should be factorized somewhere */

    if (rainX_rule_replication_satisfied(slot_width, nb_spare, err_desc)
        && rainX_rule_administrability_satisfied(involved_with_disk_spof_ids,
                                                 num_involved_with_disk_spof_ids,
                                                 nodes_with_disks_in_spof,
                                                 SPOFS_MAX, nb_spare,
                                                 err_desc)
        && rainX_rule_quorum_satisfied(all_involved_spof_ids,
                                       num_all_involved_spof_ids,
                                       all_nodes_in_spof, SPOFS_MAX,
                                       nb_spare, num_nodes_in_cluster,
                                       err_desc))
        return EXA_SUCCESS;
    else
        return -VRT_ERR_LAYOUT_CONSTRAINTS_INFRINGED;
}

int vrt_layout_validate_disk_group_rules(vrt_layout_t layout,
                                    exa_uuid_t disk_uuids[], int num_disks,
                                    uint32_t slot_width, uint32_t nb_spare,
                                    cl_error_desc_t *err_desc)
{

    set_error(err_desc, EXA_SUCCESS, NULL);

    switch (layout)
    {
    case VRT_LAYOUT_SSTRIPING:
        return 0;
    case VRT_LAYOUT_RAIN1:
        return check_rainX_rules(disk_uuids, num_disks, slot_width, 0,
                                 err_desc);
    case VRT_LAYOUT_RAINX:
        return check_rainX_rules(disk_uuids, num_disks, slot_width, nb_spare,
                                 err_desc);
    }

    EXA_ASSERT(layout == VRT_LAYOUT_INVALID);
    set_error(err_desc, -VRT_ERR_UNKNOWN_LAYOUT, NULL);
    return 0;
}
