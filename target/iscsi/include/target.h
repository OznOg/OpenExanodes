/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By downloading, copying, installing or
 * using the software you agree to this license. If you do not agree to this license, do not download, install,
 * copy or use the software.
 *
 * Intel License Agreement
 *
 * Copyright (c) 2000, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * -Redistributions of source code must retain the above copyright notice, this list of conditions and the
 *  following disclaimer.
 *
 * -Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *  following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * -The name of Intel Corporation may not be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TARGET_H_
#define _TARGET_H_

#include "target/iscsi/include/iqn.h"
#include "target/iscsi/include/iscsi.h"
#include "target/iscsi/include/parameters.h"
#include "lum/export/include/export.h"
#include "os/include/os_network.h"

#define RESET_ALL_LUNS (MAX_LUNS + 1)

#define CONFIG_TARGET_MAX_SESSIONS  32
#define CONFIG_TARGET_MAX_QUEUE     32
#define CONFIG_TARGET_MAX_bv_len    32
#define CONFIG_TARGET_MAX_IMMEDIATE 262144
#define CONFIG_DISK_MAX_BURST       262144

/* CHAP settings */
#define CONFIG_TARGET_USER_IN   "exanodes"      /* initiator user */
#define CONFIG_TARGET_PASS_IN   "exanodes..."  /* initiator pass */
#define CONFIG_TARGET_USER_OUT  "exanodes"      /* target user */
#define CONFIG_TARGET_PASS_OUT  "exanodes..."  /* target's secret */

struct target_cmd_t;

enum command_state
{
    COMMAND_READ_NEED_READ,
    COMMAND_WRITE_NEED_WRITE,
    COMMAND_WRITE_SUCCESS,
    COMMAND_WRITE_FAILED,
    COMMAND_READ_SUCCESS,
    COMMAND_READ_FAILED,
    COMMAND_PERSISTENT_RESERVE_SUCCESS,
    COMMAND_PERSISTENT_RESERVE_FAILED,
    COMMAND_ABORT,
    COMMAND_NOT_STARTED
};

typedef struct session_t TARGET_SESSION_T;

/**
 * return the session id of a given target session
 */
int sess_get_id(const TARGET_SESSION_T *sess);

typedef struct target_cmd_t
{
    /* used to link cmd target to the same lun by
     * scsi_command.c */
    struct target_cmd_t *lun_cmd_next;
    struct target_cmd_t *lun_cmd_prev;
    /* used to link cmd of the same session/nexus by
     * scsi_transport_iscsi */
    struct target_cmd_t *cmd_next;
    struct target_cmd_t *cmd_prev;
    TARGET_SESSION_T *sess;
    ISCSI_SCSI_CMD_T  scsi_cmd;
    int (*callback)(void *arg);
    void *callback_arg;
    void * data;
    int data_len;
    int r2t_flag;
    enum command_state status;
#ifdef WITH_PERF
    uint64_t submit_date;
#endif
} TARGET_CMD_T;

int target_init(size_t queue_depth, size_t buffer_size, const iqn_t *target_iqn,
                in_addr_t listen_address);
int target_shutdown(void);
void target_listen(void *);
int target_transfer_data(TARGET_CMD_T *cmd);
int target_reset_bus(void);
size_t target_get_buffer_size(void);
int iscsi_start_target(void);
int iscsi_stop_target(void);
/**
 * @brief Get the IQN of the Nth initiator connected to the target that can use
 * the LUN.
 *
 * The Nth initiator is not the initiator at index 'n' in the session array or
 * whatever the structure we use in the underlying components.  It is the Nth
 * initiator that can effectively use the export (if the function returns a
 * valid IQN for n, we are sure it would have returned a valid IQN for n-1).
 *
 * @param[in]  lun  logical unit number
 * @param[in]  n    number of the initiator
 * @param[out] iqn  iniator's IQN
 *
 * @return EXA_SUCCESS or a negative error code
 */
int target_get_nth_connected_iqn(lun_t lun, unsigned int idx, iqn_t *iqn);

void target_set_addresses(int num_addrs, const in_addr_t addrs[]);

void iscsi_thread(void *data);

#define ISCSI_AHS_EXTENDED_CDB 1
#define ISCSI_AHS_BIDI_READ_DATA_LENGTH 2

#endif /* _TARGET_H_ */
