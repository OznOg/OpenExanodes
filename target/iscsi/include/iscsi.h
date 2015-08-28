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

#ifndef ISCSI_H
#define ISCSI_H

#include "target/iscsi/include/scsi.h" /* for SCSI_CDB_MAX_FIXED_LENGTH */
#include "target/iscsi/include/lun.h"

#define ISCSI_VERSION 0

/*
 * Constants
 */

#define CONFIG_ISCSI_MAX_AHS_LEN 128

/*
 * Parameters
 */

#define ISCSI_IMMEDIATE_DATA_DFLT            1
#define ISCSI_INITIAL_R2T_DFLT               1
#define ISCSI_USE_PHASE_COLLAPSED_READ_DFLT  1
#define ISCSI_HEADER_LEN                     48
#define ISCSI_OPCODE(HEADER)                 (HEADER[0] & 0x3f)

/*
 * Opcodes
 */

#define ISCSI_NOP_OUT       0x00
#define ISCSI_SCSI_CMD      0x01
#define ISCSI_TASK_CMD      0x02
#define ISCSI_LOGIN_CMD     0x03
#define ISCSI_TEXT_CMD      0x04
#define ISCSI_WRITE_DATA    0x05
#define ISCSI_LOGOUT_CMD    0x06
#define ISCSI_NOP_IN        0x20
#define ISCSI_SCSI_RSP      0x21
#define ISCSI_TASK_RSP      0x22
#define ISCSI_LOGIN_RSP     0x23
#define ISCSI_TEXT_RSP      0x24
#define ISCSI_READ_DATA     0x25
#define ISCSI_LOGOUT_RSP    0x26
#define ISCSI_R2T           0x31
#define ISCSI_REJECT        0x3f
#define ISCSI_SNACK         0x10        /* not implemented */
#define ISCSI_ASYNC         0x32

/*
 * Login Phase
 */

#define ISCSI_LOGIN_STATUS_SUCCESS          0
#define ISCSI_LOGIN_STATUS_REDIRECTION      1
#define ISCSI_LOGIN_STATUS_INITIATOR_ERROR  2
#define ISCSI_LOGIN_STATUS_TARGET_ERROR     3
#define ISCSI_LOGIN_STAGE_SECURITY          0
#define ISCSI_LOGIN_STAGE_NEGOTIATE         1
#define ISCSI_LOGIN_STAGE_FULL_FEATURE      3

/*
 * Logout Phase
 */

#define ISCSI_LOGOUT_CLOSE_SESSION      0
#define ISCSI_LOGOUT_CLOSE_CONNECTION   1
#define ISCSI_LOGOUT_CLOSE_RECOVERY     2
#define ISCSI_LOGOUT_STATUS_SUCCESS     0
#define ISCSI_LOGOUT_STATUS_NO_CID      1
#define ISCSI_LOGOUT_STATUS_NO_RECOVERY 2
#define ISCSI_LOGOUT_STATUS_FAILURE     3

/*
 * Task Command
 */

#define ISCSI_TASK_CMD_ABORT_TASK           1
#define ISCSI_TASK_CMD_ABORT_TASK_SET       2
#define ISCSI_TASK_CMD_CLEAR_ACA            3
#define ISCSI_TASK_CMD_CLEAR_TASK_SET       4
#define ISCSI_TASK_CMD_LOGICAL_UNIT_RESET   5
#define ISCSI_TASK_CMD_TARGET_WARM_RESET    6
#define ISCSI_TASK_CMD_TARGET_COLD_RESET    7
#define ISCSI_TASK_CMD_TARGET_REASSIGN      8

/* Reject Reasons
 *
 */
#define ISCSI_REJECT_RESERVED                   0x01
#define ISCSI_REJECT_DATA_DIGEST_ERROR          0x02
#define ISCSI_REJECT_SNACK_REJECT               0x03
#define ISCSI_REJECT_PROTOCOL_ERROR             0x04
#define ISCSI_REJECT_COMMAND_NOT_SUPPORTED      0x05
#define ISCSI_REJECT_IMMEDIATE_COMMAND_REJECT   0x06
#define ISCSI_REJECT_TASK_IN_PROGRESS           0x07
#define ISCSI_REJECT_INVALID_DATA_ACK           0x08
#define ISCSI_REJECT_INVALID_PDU_FIELD          0x09
#define ISCSI_REJECT_LONG_OPERATIN_REJECT       0x0A
#define ISCSI_REJECT_NEGOTIATION_REJECT         0x0B
#define ISCSI_REJECT_WAITING_LOGOUT             0x0C

typedef struct
{
    int           immediate;
    unsigned char function;
    lun_t         lun;
    unsigned      tag;
    unsigned      ref_tag;
    unsigned      CmdSN;
    unsigned      ExpStatSN;
    unsigned      RefCmdSN;
    unsigned      ExpDataSN;
} ISCSI_TASK_CMD_T;

int iscsi_task_cmd_decap(const unsigned char *header, ISCSI_TASK_CMD_T *cmd);

/*
 * Asynchronous message
 */
typedef struct
{
    unsigned       length;
    lun_t          lun;
    unsigned char  AsyncEvent; /* 0 for SCSI event */
    unsigned       StatSN;
    unsigned       ExpCmdSN;
    unsigned       MaxCmdSN;
    unsigned char  AHSlength;
    unsigned char *data;
    unsigned int   Parameter1;
    unsigned int   Parameter2;
    unsigned int   Parameter3;
} ISCSI_ASYNCHRONOUS_MESSAGE;

int iscsi_async_event_encap(unsigned char *header,
                            const ISCSI_ASYNCHRONOUS_MESSAGE *rsp);

/*
 * Task Response
 */

#define ISCSI_TASK_RSP_FUNCTION_COMPLETE  0
#define ISCSI_TASK_RSP_NO_SUCH_TASK       1
#define ISCSI_TASK_RSP_NO_SUCH_LUN        2
#define ISCSI_TASK_RSP_STILL_ALLEGIANT    3
#define ISCSI_TASK_RSP_NO_FAILOVER        4
#define ISCSI_TASK_RSP_NO_SUPPORT         5
#define ISCSI_TASK_RSP_AUTHORIZED_FAILED  6

#define ISCSI_TASK_RSP_REJECTED           255

#define ISCSI_TASK_QUAL_FUNCTION_EXECUTED  0
#define ISCSI_TASK_QUAL_NOT_AUTHORIZED     1

typedef struct
{
    unsigned char response;
    unsigned      length;
    unsigned char AHSlength;
    unsigned      tag;
    unsigned      StatSN;
    unsigned      ExpCmdSN;
    unsigned      MaxCmdSN;
} ISCSI_TASK_RSP_T;

int iscsi_task_rsp_encap(unsigned char *header, const ISCSI_TASK_RSP_T *rsp);

/*
 * NOP-Out
 */

typedef struct
{
    int            immediate;
    unsigned       length;
    lun_t          lun;
    unsigned       tag;
    unsigned       transfer_tag;
    unsigned       CmdSN;
    unsigned       ExpStatSN;
    unsigned char *data;
} ISCSI_NOP_OUT_T;

int iscsi_nop_out_decap(const unsigned char *header, ISCSI_NOP_OUT_T *cmd);

/*
 * NOP-In
 */

typedef struct
{
    unsigned length;
    lun_t    lun;
    unsigned tag;
    unsigned transfer_tag;
    unsigned StatSN;
    unsigned ExpCmdSN;
    unsigned MaxCmdSN;
} ISCSI_NOP_IN_T;

int iscsi_nop_in_encap(unsigned char *header, const ISCSI_NOP_IN_T *cmd);


/*
 * Text Command
 */

typedef struct
{
    int      immediate;
    int      final;
    int      cont;
    unsigned length;
    lun_t    lun;
    unsigned tag;
    unsigned transfer_tag;
    unsigned CmdSN;
    unsigned ExpStatSN;
    char    *text;
} ISCSI_TEXT_CMD_T;

int iscsi_text_cmd_decap(const unsigned char *header, ISCSI_TEXT_CMD_T *cmd);

/*
 * Text Response
 */

typedef struct
{
    int      final;
    int      cont;
    unsigned length;
    lun_t    lun;
    unsigned tag;
    unsigned transfer_tag;
    unsigned StatSN;
    unsigned ExpCmdSN;
    unsigned MaxCmdSN;
} ISCSI_TEXT_RSP_T;

int iscsi_text_rsp_encap(unsigned char *header, const ISCSI_TEXT_RSP_T *rsp);


/*
 * Login Command
 */

typedef struct
{
    int transit;
    int cont;
    unsigned char      csg;
    unsigned char      nsg;
    char               version_max;
    char               version_min;
    unsigned char      AHSlength;
    unsigned           length;
    unsigned long long isid;
    unsigned short     tsih;
    unsigned           tag;
    unsigned short     cid;
    unsigned           CmdSN;
    unsigned           ExpStatSN;
    char              *text;
} ISCSI_LOGIN_CMD_T;

int iscsi_login_cmd_encap(unsigned char *header, const ISCSI_LOGIN_CMD_T *cmd);
int iscsi_login_cmd_decap(const unsigned char *header, ISCSI_LOGIN_CMD_T *cmd);

/*
 * Login Response
 */

typedef struct
{
    int transit;
    int cont;
    unsigned char      csg;
    unsigned char      nsg;
    char               version_max;
    char               version_active;
    unsigned char      AHSlength;
    unsigned           length;
    unsigned long long isid;
    unsigned short     tsih;
    unsigned           tag;
    unsigned           StatSN;
    unsigned           ExpCmdSN;
    unsigned           MaxCmdSN;
    unsigned char      status_class;
    unsigned char      status_detail;
} ISCSI_LOGIN_RSP_T;

int iscsi_login_rsp_encap(unsigned char *header, const ISCSI_LOGIN_RSP_T *rsp);

/*
 * Logout Command
 */

typedef struct
{
    int            immediate;
    unsigned char  reason;
    unsigned       tag;
    unsigned short cid;
    unsigned       CmdSN;
    unsigned       ExpStatSN;
} ISCSI_LOGOUT_CMD_T;

int iscsi_logout_cmd_decap(const unsigned char *header, ISCSI_LOGOUT_CMD_T *cmd);

/*
 * Logout Response
 */

typedef struct
{
    unsigned char  response;
    unsigned       length;
    unsigned       tag;
    unsigned       StatSN;
    unsigned       ExpCmdSN;
    unsigned       MaxCmdSN;
    unsigned short Time2Wait;
    unsigned short Time2Retain;
} ISCSI_LOGOUT_RSP_T;

int iscsi_logout_rsp_encap(unsigned char *header, const ISCSI_LOGOUT_RSP_T *rsp);

/*
 * SCSI Command
 */

typedef struct
{
    int immediate;
    int final;
    int fromdev;
    unsigned       bytes_fromdev;
    int            todev;
    unsigned       bytes_todev;
    unsigned char  attr;
    unsigned       length;
    lun_t          lun;
    unsigned       tag;
    unsigned       trans_len;
    unsigned       bidi_trans_len;
    unsigned       CmdSN;
    unsigned       ExpStatSN;
    unsigned char  cdb[SCSI_CDB_MAX_FIXED_LENGTH]; /* Command Descriptor Block ie. the SCSI command itself
                                                    * FIXME: We should use a proper SCSI structure (or pointer) instead
                                                    */
    unsigned char  ahs[CONFIG_ISCSI_MAX_AHS_LEN];
    unsigned char  ahs_len;
    unsigned char  status;
} ISCSI_SCSI_CMD_T;

int iscsi_scsi_cmd_encap(unsigned char *header, const ISCSI_SCSI_CMD_T *cmd);
int iscsi_scsi_cmd_decap(const unsigned char *header, ISCSI_SCSI_CMD_T *cmd);

/*
 * SCSI Response
 */

typedef struct
{
    int           bidi_overflow;
    int           bidi_underflow;
    int           overflow;
    int           underflow;

    unsigned char response;
    unsigned char status;
    unsigned      ahs_len;
    unsigned      length;
    unsigned      tag;
    unsigned      StatSN;
    unsigned      ExpCmdSN;
    unsigned      MaxCmdSN;
    unsigned      ExpDataSN;
    unsigned      bidi_res_cnt;
    unsigned      basic_res_cnt;
} ISCSI_SCSI_RSP_T;

int iscsi_scsi_rsp_encap(unsigned char *header, const ISCSI_SCSI_RSP_T *rsp);

/*
 * Ready To Transfer (R2T)
 */

typedef struct
{
    unsigned AHSlength;
    lun_t    lun;
    unsigned tag;
    unsigned transfer_tag;
    unsigned StatSN;
    unsigned ExpCmdSN;
    unsigned MaxCmdSN;
    unsigned R2TSN;
    unsigned offset;
    unsigned length;
} ISCSI_R2T_T;

int iscsi_r2t_encap(unsigned char *header, const ISCSI_R2T_T *cmd);


/*
 * SCSI Write Data
 */

typedef struct
{
    int      final;
    unsigned length;
    lun_t    lun;
    unsigned tag;
    unsigned transfer_tag;
    unsigned ExpStatSN;
    unsigned DataSN;
    unsigned offset;
} ISCSI_WRITE_DATA_T;

int iscsi_write_data_decap(const unsigned char *header, ISCSI_WRITE_DATA_T *cmd);

/*
 * SCSI Read Data
 */

typedef struct
{
    int           final;
    int           ack;
    int           overflow;
    int           underflow;
    int           S_bit;
    unsigned char status;
    unsigned      length;
    lun_t         lun;
    unsigned      task_tag;
    unsigned      transfer_tag;
    unsigned      StatSN;
    unsigned      ExpCmdSN;
    unsigned      MaxCmdSN;
    unsigned      DataSN;
    unsigned      offset;
    unsigned      res_count;
} ISCSI_READ_DATA_T;

int iscsi_read_data_encap(unsigned char *header, const ISCSI_READ_DATA_T *cmd);

/*
 * Reject
 */

typedef struct
{
    unsigned char reason;
    unsigned      length;
    unsigned      StatSN;
    unsigned      ExpCmdSN;
    unsigned      MaxCmdSN;
    unsigned      DataSN;
} ISCSI_REJECT_T;

int iscsi_reject_encap(unsigned char *header, const ISCSI_REJECT_T *cmd);



#endif /* ISCSI_H */
