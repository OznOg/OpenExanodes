/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __LUM_MSG_H
#define __LUM_MSG_H

#include "lum/export/include/export_info.h"

#include "target/include/target_adapter.h"
#include "target/iscsi/include/iqn.h"
#include "common/include/uuid.h"
#include "common/include/exa_constants.h"

/* FIXME Should have two distinct enums, one for each destination thread
         (info and command). */
typedef enum
{
#define LUM_REQUEST_TYPE__FIRST LUM_INFO_GET_NTH_CONNECTED_IQN
  /* Arbitrary value to avoid collisions by chance */
  LUM_INFO_GET_NTH_CONNECTED_IQN = 133,
  LUM_INFO,
  LUM_CMD_SUSPEND,
  LUM_CMD_RESUME,
  LUM_CMD_START_TARGET,
  LUM_CMD_STOP_TARGET,
  LUM_CMD_SET_MSHIP,
  LUM_CMD_SET_PEERS,
  LUM_CMD_SET_TARGETS,
  LUM_CMD_EXPORT_PUBLISH,
  LUM_CMD_EXPORT_UNPUBLISH,
  LUM_CMD_EXPORT_UPDATE_IQN_FILTERS,
  LUM_CMD_INIT,
  LUM_CMD_CLEANUP,
  LUM_CMD_SET_READAHEAD,
  LUM_CMD_EXPORT_RESIZE
#define LUM_REQUEST_TYPE__LAST  LUM_CMD_EXPORT_RESIZE
} lum_request_type_t;

#define LUM_REQUEST_TYPE_IS_VALID(id) \
    ((id) >= LUM_REQUEST_TYPE__FIRST && (id) <= LUM_REQUEST_TYPE__LAST)

typedef struct lum_export_publish {
    size_t buf_size;
    char buf[];
} lum_cmd_publish_t;

typedef struct {
    size_t num_addr;
    in_addr_t addr[];
} lum_target_addresses_t;

typedef struct {
    size_t buf_size;
    char buf[];
} lum_cmd_update_iqn_filters_t;

typedef struct
{
    exa_uuid_t   export_uuid;
    unsigned int iqn_num;
} lum_info_get_nth_iqn_t;

typedef struct
{
    exa_uuid_t  export_uuid;
} lum_cmd_unpublish_t;

typedef struct
{
    exa_uuid_t export_uuid;
    uint32_t readahead;
} lum_cmd_set_readahead_t;

typedef struct
{
    lum_init_params_t init_params;
} lum_cmd_init_t;

typedef struct
{
    exa_uuid_t  export_uuid;
} lum_export_info_t;

typedef struct
{
    exa_uuid_t export_uuid;
    uint64_t size;
} lum_cmd_export_resize_t;

typedef struct
{
    exa_nodeset_t mship;
} lum_cmd_set_mship_t;

typedef struct lum_request
{
    lum_request_type_t type;
    union {
        lum_info_get_nth_iqn_t nth_iqn;
        lum_cmd_set_readahead_t set_readahead;
        lum_cmd_init_t init;
        lum_export_info_t info;
        lum_cmd_export_resize_t export_resize;
        lum_cmd_publish_t publish;
        lum_cmd_unpublish_t unpublish;
        lum_cmd_update_iqn_filters_t update_iqn_filters;
        lum_cmd_set_mship_t set_mship;
        adapter_peers_t set_peers;
        lum_target_addresses_t target_addresses;
    };
} lum_request_t;

typedef struct lum_answer
{
    int error;
    union {
        lum_export_info_reply_t info;
        iqn_t connected_iqn;
    };
} lum_answer_t;

#endif
