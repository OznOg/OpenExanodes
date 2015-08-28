/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_TYPES_H__
#define __MD_TYPES_H__

#include "os/include/os_inttypes.h"
#include "common/include/exa_nodeset.h"
#include "common/include/exa_constants.h"
#include "common/include/uuid.h"
#include "monitoring/common/include/md_constants.h"



/** Agent messages (md <=> agent) */
typedef enum {
    MD_MSG_AGENT_TRAP,
    MD_MSG_AGENT_REQ,
    MD_MSG_AGENT_ALIVE
} md_msg_agent_type_t;

typedef enum {
    MD_NODE_TRAP,
    MD_DISK_TRAP,
    MD_DISKGROUP_TRAP,
    MD_VOLUME_TRAP,
    MD_FILESYSTEM_TRAP
} md_trap_type_t;


/** Generic structure containing
    trap information. */
typedef struct {
    md_trap_type_t type;
    exa_uuid_str_t id;
    char name[MD_MAXSIZE_TRAP_NAME];
    uint32_t status;
} md_msg_agent_trap_t;


/** Structure containing snmp request information */
typedef struct {
    uint32_t code;
} md_msg_agent_req_t;


/** Empty structure for alive messages */
typedef struct {
} md_msg_agent_alive_t;


/** Union of all possible payloads for agent messages */
typedef struct {
    md_msg_agent_type_t type;
    union {
	md_msg_agent_trap_t trap;
	md_msg_agent_req_t req;
	md_msg_agent_alive_t alive;
    } msg;
} md_msg_agent_t;



/** Event messages. (exanodes => md) */
typedef enum {
    MD_MSG_EVENT_TRAP,
    MD_MSG_EVENT_ENDOFCOMMAND,
} md_msg_event_type_t;

typedef md_msg_agent_trap_t md_msg_event_trap_t;


/** Message payload marking end of command */
typedef struct {
} md_msg_event_endofcommand_t;


/** Union of all possible payloads for event messages */
typedef struct {
    md_msg_event_type_t type;
    union {
	md_msg_event_trap_t trap;
	md_msg_event_endofcommand_t endofcommand;
    } content;
} md_msg_event_t;



/** Control messages. (exanodes => md) */
typedef enum {
    MD_MSG_CONTROL_START = 381,
    MD_MSG_CONTROL_STOP,
    MD_MSG_CONTROL_STATUS,
} md_msg_control_type_t;


/** Control message used to start monitoring service (ie local subagent).
 *  Master agentx host and port must be provided so that local subagent can
 *  register.
 */
typedef struct {
    exa_nodeid_t node_id;
    char node_name[EXA_MAXSIZE_HOSTNAME+1];
    char master_agentx_host[EXA_MAXSIZE_HOSTNAME+1];
    uint32_t master_agentx_port;
} md_msg_control_start_t;


/** Control message used to stop monitoring service (ie local subagent). */
typedef struct {
} md_msg_control_stop_t;


/** Control message used get status of monitoring service (ie local subagent). */
typedef struct {
} md_msg_control_status_t;


/** Union of all possible payloads for control messages */
typedef struct {
    md_msg_control_type_t type;
    union {
	md_msg_control_start_t start;
	md_msg_control_stop_t stop;
	md_msg_control_status_t status;
    } content;
} md_msg_control_t;



typedef enum {
    MD_SERVICE_STOPPED = 0,
    MD_SERVICE_STARTED,
} md_service_status_t;


typedef enum {
    MD_DISKGROUP_STATUS_STOPPED = 1,
    MD_DISKGROUP_STATUS_OK,
    MD_DISKGROUP_STATUS_USINGSPARE,
    MD_DISKGROUP_STATUS_DEGRADED,
    MD_DISKGROUP_STATUS_OFFLINE,
    MD_DISKGROUP_STATUS_REBUILDING,
} md_diskgroup_status_type_t;


typedef enum {
    MD_DISK_STATUS_UP = 1,
    MD_DISK_STATUS_OK,
    MD_DISK_STATUS_DOWN,
    MD_DISK_STATUS_BROKEN,
    MD_DISK_STATUS_MISSING,
    MD_DISK_STATUS_UPDATING,
    MD_DISK_STATUS_REPLICATING,
    MD_DISK_STATUS_OUTDATED,
    MD_DISK_STATUS_BLANK,
    MD_DISK_STATUS_ALIEN,
} md_disk_status_type_t;


typedef enum {
    MD_VOLUME_STATUS_STARTED = 1,
    MD_VOLUME_STATUS_INUSE,
    MD_VOLUME_STATUS_WILLSTART,
    MD_VOLUME_STATUS_WILLSTOP,
    MD_VOLUME_STATUS_READONLY,
    MD_VOLUME_STATUS_OFFLINE,
} md_volume_status_type_t;


typedef enum {
    MD_FILESYSTEM_STATUS_STARTED = 1,
    MD_FILESYSTEM_STATUS_INUSE,
    MD_FILESYSTEM_STATUS_WILLSTART,
    MD_FILESYSTEM_STATUS_WILLSTOP,
    MD_FILESYSTEM_STATUS_READONLY,
    MD_FILESYSTEM_STATUS_OFFLINE,
} md_filesystem_status_type_t;


typedef enum {
    MD_NODE_STATUS_DOWN = 1,
    MD_NODE_STATUS_UP,
} md_node_status_type_t;





#endif /* __MD_TYPES_H__ */
