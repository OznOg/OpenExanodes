/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __TARGET_ADAPTER_H__
#define __TARGET_ADAPTER_H__

#include "lum/export/include/executive_export.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_network.h"

/* FIXME Should be named adapter_settings_t */
typedef struct
{
    size_t iscsi_queue_depth;
    size_t bdev_queue_depth;
    size_t buffer_size;
    iqn_t target_iqn;
    in_addr_t target_listen_address;
} lum_init_params_t;

typedef struct
{
    char addresses[EXA_MAX_NODES_NUMBER][EXA_MAXSIZE_NICADDRESS + 1];
} adapter_peers_t;

typedef struct
{
    int (*static_init)(exa_nodeid_t node_id);
    int (*static_cleanup)(void);
    int (*init)(const lum_init_params_t *params);
    int (*cleanup)(void);
    int (*signal_new_export)(lum_export_t *export, uint64_t size);
    int (*signal_remove_export)(const lum_export_t *export);
    int (*signal_export_update_iqn_filters)(const lum_export_t *lum_export);
    void (*export_set_size)(lum_export_t *export, uint64_t size_in_sector);
    lum_export_inuse_t (*export_get_inuse)(const lum_export_t *export);
    int (*set_readahead)(const lum_export_t *export, uint32_t readahead);
    void (*set_mship)(const exa_nodeset_t *mship);
    void (*set_peers)(const adapter_peers_t *peers);
    void (*set_addresses)(int num_addr, const in_addr_t addrs[]);
    void (*suspend)(void);
    void (*resume)(void);
    int (*start_target)(void);
    int (*stop_target)(void);
} target_adapter_t;

const target_adapter_t *get_iscsi_adapter(void);
#ifdef WITH_BDEV
const target_adapter_t *get_bdev_adapter(void);
#endif

#endif /* __TARGET_ADAPTER_H__ */
