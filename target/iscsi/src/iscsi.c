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

#include <stdlib.h>
#include <string.h>

#include "common/include/exa_assert.h"
#include "target/iscsi/include/iscsi.h"
#include "target/iscsi/include/endianness.h"
#include "target/iscsi/include/lun.h"
#include "log/include/log.h"

#ifdef WITH_TRACE

#ifdef WIN32
#define ISCSI_TRACE(...) exalog_trace(## __VA_ARGS__)
#else
#define ISCSI_TRACE(...) exalog_trace(__VA_ARGS__)
#endif

#else

#define ISCSI_TRACE(...) do {} while (0)

#endif

#define RETURN_NOT_EQUAL(NAME, V1, V2, RC)                             \
    if ((V1) != (V2)) {                                                \
        exalog_error("Bad \"%s\": Got %u expected %u.", NAME, V1, V2); \
        return RC;                                                     \
    }

/*
 * Task Command
 */
int iscsi_task_cmd_decap(const unsigned char *header, ISCSI_TASK_CMD_T *cmd)
{
    EXA_ASSERT_VERBOSE(ISCSI_OPCODE(header) == ISCSI_TASK_CMD,
                       "Opcode mismatch ISCSI_TASK_CMD");

    cmd->immediate = ((header[0] & 0x40) == 0x40);              /* Immediate bit */
    cmd->function = header[1] & 0x7f;                           /* Function */

    /* only for LUN specific tasks (see rfc3720 §10.5.3)  */
    if (cmd->function == ISCSI_TASK_CMD_ABORT_TASK
        || cmd->function == ISCSI_TASK_CMD_CLEAR_TASK_SET
        || cmd->function == ISCSI_TASK_CMD_ABORT_TASK_SET
        || cmd->function == ISCSI_TASK_CMD_CLEAR_ACA
        || cmd->function == ISCSI_TASK_CMD_LOGICAL_UNIT_RESET)
        cmd->lun = lun_get_bigendian(header + 8);               /* LUN */
    else
        cmd->lun = LUN_NONE;

    cmd->tag = get_bigendian32(header + 16);             /* Tag */
    cmd->ref_tag = get_bigendian32(header + 20);         /* Reference Tag */
    cmd->CmdSN = get_bigendian32(header + 24);           /* CmdSN */
    cmd->ExpStatSN = get_bigendian32(header + 28);       /* ExpStatSN */
    cmd->RefCmdSN = get_bigendian32(header + 32);        /* RefCmdSN */
    cmd->ExpDataSN = get_bigendian32(header + 36);       /* ExpDataSN */

    RETURN_NOT_EQUAL("Byte 1, bit 0 in command header", header[1] & 0x80, 0x80, 1);
    RETURN_NOT_EQUAL("Byte 2 in command header", header[2], 0, 1);
    RETURN_NOT_EQUAL("Byte 3 in command header", header[3], 0, 1);
    RETURN_NOT_EQUAL("Bytes 4-7 in command header", *((unsigned *)(header + 4)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 40-43 in command header", *((unsigned *)(header + 40)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 44-47 in command header", *((unsigned *)(header + 44)), 0, 1);

    ISCSI_TRACE("iSCSI 'task' message decapsulation");
    ISCSI_TRACE(" - Immediate: %i", cmd->immediate);
    ISCSI_TRACE(" - Function:  %u", cmd->function);
    ISCSI_TRACE(" - LUN:       %"PRIlun"", cmd->lun);
    ISCSI_TRACE(" - Tag:       0x%x", cmd->tag);
    ISCSI_TRACE(" - Ref Tag:   0x%x", cmd->ref_tag);
    ISCSI_TRACE(" - CmdSN:     %u", cmd->CmdSN);
    ISCSI_TRACE(" - ExpStatSN: %u", cmd->ExpStatSN);
    ISCSI_TRACE(" - RefCmdSN:  %u", cmd->RefCmdSN);
    ISCSI_TRACE(" - ExpDataSN: %u", cmd->ExpDataSN);

    return 0;
}


/*
 * Task Response
 */
int iscsi_task_rsp_encap(unsigned char *header, const ISCSI_TASK_RSP_T *rsp)
{
    ISCSI_TRACE("iSCSI 'task' reponse encapsulation");
    ISCSI_TRACE(" - Response:  %u", rsp->response);
    ISCSI_TRACE(" - Length:    %u", rsp->length);
    ISCSI_TRACE(" - Tag:       0x%x", rsp->tag);
    ISCSI_TRACE(" - StatSN:    %u", rsp->StatSN);
    ISCSI_TRACE(" - ExpCmdSN:  %u", rsp->ExpCmdSN);
    ISCSI_TRACE(" - MaxCmdSN:  %u", rsp->MaxCmdSN);

    memset(header, 0, ISCSI_HEADER_LEN);

    header[0] |= ISCSI_TASK_RSP;                                   /* Opcode */
    header[1] |= 0x80;                                             /* Byte 1 bit 0, FIXME: what does it mean ? */

    header[2] = rsp->response;                                     /* Response, FIXME: why there is no convertion here ? */
    set_bigendian32(rsp->length & 0x00ffffff, header + 4);         /* Length */
    set_bigendian32(rsp->tag, header + 16);                        /* Tag */
    set_bigendian32(rsp->StatSN, header + 24);                     /* StatSN */
    set_bigendian32(rsp->ExpCmdSN, header + 28);                   /* ExpCmdSN */
    set_bigendian32(rsp->MaxCmdSN, header + 32);                   /* MaxCmdSN */

    return 0;
}


int iscsi_async_event_encap(unsigned char *header,
                            const ISCSI_ASYNCHRONOUS_MESSAGE *rsp)
{
    ISCSI_TRACE("iSCSI 'async event' message encapsulation");

    set_bigendian32(0x800000, header);
    header[0] = 0x32;
    set_bigendian32(rsp->length & 0x00ffffff, header + 4);
    header[4] = rsp->AHSlength;
    lun_set_bigendian(rsp->lun, header + 8);               /* LUN, formerly used set_bigendian64 */
    set_bigendian32(0xffffffff, header + 16);
    set_bigendian32(0, header + 20);
    set_bigendian32(rsp->StatSN, header + 24);
    set_bigendian32(rsp->ExpCmdSN, header + 28);
    set_bigendian32(rsp->MaxCmdSN, header + 32);
    header[36] = rsp->AsyncEvent;                          /* asynch event 0->SCSI event */
    header[37] = 0;                                        /* Vendor specific async v code */
    set_bigendian16(rsp->Parameter1, header + 38);
    set_bigendian16(rsp->Parameter2, header + 40);
    set_bigendian16(rsp->Parameter3, header + 42);
    set_bigendian32(0, header + 44);
    return 0;
}


/*
 * NOP-Out
 */
int iscsi_nop_out_decap(const unsigned char *header, ISCSI_NOP_OUT_T *cmd)
{
    EXA_ASSERT_VERBOSE(ISCSI_OPCODE(header) == ISCSI_NOP_OUT,
                       "Opcode mismatch ISCSI_NOP_OUT");

    cmd->immediate = ((header[0] & 0x40) == 0x40);     /* Immediate bit, FIXME: make it understandable */
    cmd->length = get_bigendian32(header + 4);         /* Length */
    cmd->lun = lun_get_bigendian(header + 8);          /* LUN */
    cmd->tag = get_bigendian32(header + 16);           /* Tag */
    cmd->transfer_tag = get_bigendian32(header + 20);  /* Target Tranfer Tag */
    cmd->CmdSN = get_bigendian32(header + 24);         /* CmdSN */
    cmd->ExpStatSN = get_bigendian32(header + 28);     /* ExpStatSN */

    RETURN_NOT_EQUAL("Byte 1 in nop header", header[1], 0x80, 1);
    RETURN_NOT_EQUAL("Byte 2 in nop header", header[2], 0, 1);
    RETURN_NOT_EQUAL("Byte 3 in nop header", header[3], 0, 1);
    RETURN_NOT_EQUAL("Byte 4 in nop header", header[4], 0, 1);
    RETURN_NOT_EQUAL("Bytes 32-35 in nop header", *((unsigned *)(header + 32)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 36-39 in nop header", *((unsigned *)(header + 36)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 40-43 in nop header", *((unsigned *)(header + 40)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 44-47 in nop header", *((unsigned *)(header + 44)), 0, 1);

    ISCSI_TRACE("iSCSI 'nop-out' message encapsulation");
    ISCSI_TRACE(" - Immediate:    %i", cmd->immediate);
    ISCSI_TRACE(" - Length:       %u", cmd->length);
    ISCSI_TRACE(" - LUN:          %"PRIlun"", cmd->lun);
    ISCSI_TRACE(" - Tag:          0x%x", cmd->tag);
    ISCSI_TRACE(" - Transfer Tag: 0x%x", cmd->transfer_tag);
    ISCSI_TRACE(" - CmdSN:        %u", cmd->CmdSN);
    ISCSI_TRACE(" - ExpStatSN:    %u", cmd->ExpStatSN);

    return 0;
}


/*
 * NOP-In
 */
int iscsi_nop_in_encap(unsigned char *header, const ISCSI_NOP_IN_T *cmd)
{
    ISCSI_TRACE("iSCSI 'nop-in' decapsulation");
    ISCSI_TRACE(" - Length:       %u", cmd->length);
    ISCSI_TRACE(" - LUN:          %"PRIlun"", cmd->lun);
    ISCSI_TRACE(" - Tag:          0x%x", cmd->tag);
    ISCSI_TRACE(" - Transfer Tag: 0x%x", cmd->transfer_tag);
    ISCSI_TRACE(" - StatSN:       %u", cmd->StatSN);
    ISCSI_TRACE(" - ExpCmdSN:     %u", cmd->ExpCmdSN);
    ISCSI_TRACE(" - MaxCmdSN:     %u", cmd->MaxCmdSN);

    memset(header, 0, ISCSI_HEADER_LEN);

    header[0] = 0x00 | ISCSI_NOP_IN;                       /* Opcode */
    header[1] |= 0x80;                                     /* Reserved */

    set_bigendian32(cmd->length & 0x00ffffff, header + 4); /* Length */
    lun_set_bigendian(cmd->lun, header + 8);               /* LUN */
    set_bigendian32(cmd->tag, header + 16);                /* Tag */
    set_bigendian32(cmd->transfer_tag, header + 20);       /* Target Transfer Tag */
    set_bigendian32(cmd->StatSN, header + 24);             /* StatSN */
    set_bigendian32(cmd->ExpCmdSN, header + 28);           /* ExpCmdSN */
    set_bigendian32(cmd->MaxCmdSN, header + 32);           /* MaxCmdSN */

    return 0;
}


/*
 * Text Command
 */
int iscsi_text_cmd_decap(const unsigned char *header, ISCSI_TEXT_CMD_T *cmd)
{
    EXA_ASSERT_VERBOSE(ISCSI_OPCODE(header) == ISCSI_TEXT_CMD,
                       "Opcode mismatch ISCSI_TEXT_CMD");

    cmd->immediate = ((header[0] & 0x40) == 0x40);       /* Immediate bit */
    cmd->final = ((header[1] & 0x80) == 0x80);           /* Final bit */
    cmd->cont = ((header[1] & 0x40) == 0x40);            /* Continue bit */
    cmd->length = get_bigendian32(header + 4);           /* Length */
    cmd->lun = lun_get_bigendian(header + 8);            /* LUN */
    cmd->tag = get_bigendian32(header + 16);             /* Tag */
    cmd->transfer_tag = get_bigendian32(header + 20);    /* Transfer Tag */
    cmd->CmdSN = get_bigendian32(header + 24);           /* CmdSN */
    cmd->ExpStatSN = get_bigendian32(header + 28);       /* ExpStatSN */

    RETURN_NOT_EQUAL("Byte 1, Bits 2-7", header[1] & 0x00, 0, 1);
    RETURN_NOT_EQUAL("Byte 2", header[2], 0, 1);
    RETURN_NOT_EQUAL("Byte 3", header[3], 0, 1);
    RETURN_NOT_EQUAL("Byte 4", header[4], 0, 1);
    RETURN_NOT_EQUAL("Bytes 8-11", *((unsigned *)(header + 8)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 12-15", *((unsigned *)(header + 12)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 32-35", *((unsigned *)(header + 32)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 36-39", *((unsigned *)(header + 36)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 40-43", *((unsigned *)(header + 40)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 44-47", *((unsigned *)(header + 44)), 0, 1);

    ISCSI_TRACE("iSCSI 'text' message decapsulation");
    ISCSI_TRACE(" - Immediate:    %i", cmd->immediate);
    ISCSI_TRACE(" - Final:        %i", cmd->final);
    ISCSI_TRACE(" - Continue:     %i", cmd->cont);
    ISCSI_TRACE(" - Length:       %u", cmd->length);
    ISCSI_TRACE(" - LUN:          %"PRIlun"", cmd->lun);
    ISCSI_TRACE(" - Tag:          0x%x", cmd->tag);
    ISCSI_TRACE(" - Transfer Tag: 0x%x", cmd->transfer_tag);
    ISCSI_TRACE(" - CmdSN:        %u", cmd->CmdSN);
    ISCSI_TRACE(" - ExpStatSN:    %u", cmd->ExpStatSN);

    return 0;
}


/*
 * Text Response
 */

int iscsi_text_rsp_encap(unsigned char *header, const ISCSI_TEXT_RSP_T *rsp)
{
    ISCSI_TRACE("iSCSI 'text' response encapsulation");
    ISCSI_TRACE(" - Final:        %i", rsp->final);
    ISCSI_TRACE(" - Continue:     %i", rsp->cont);
    ISCSI_TRACE(" - Length:       %u", rsp->length);
    ISCSI_TRACE(" - LUN:          %"PRIlun"", rsp->lun);
    ISCSI_TRACE(" - Tag:          0x%x", rsp->tag);
    ISCSI_TRACE(" - Transfer Tag: 0x%x", rsp->transfer_tag);
    ISCSI_TRACE(" - StatSN:       %u", rsp->StatSN);
    ISCSI_TRACE(" - ExpCmdSN:     %u", rsp->ExpCmdSN);
    ISCSI_TRACE(" - MaxCmdSN:     %u", rsp->MaxCmdSN);

    memset(header, 0, ISCSI_HEADER_LEN);
    header[0] |= 0x00 | ISCSI_TEXT_RSP;                    /* Opcode */
    if (rsp->final)
        header[1] |= 0x80;                                 /* Final bit */

    if (rsp->cont)
        header[1] |= 0x40;                                 /* Continue */

    set_bigendian32(rsp->length & 0x00ffffff, header + 4); /* Length */
    lun_set_bigendian(rsp->lun, header + 8);               /* LUN */
    set_bigendian32(rsp->tag, header + 16);                /* Tag */
    set_bigendian32(rsp->transfer_tag, header + 20);       /* Transfer Tag */
    set_bigendian32(rsp->StatSN, header + 24);             /* StatSN */
    set_bigendian32(rsp->ExpCmdSN, header + 28);           /* ExpCmdSN */
    set_bigendian32(rsp->MaxCmdSN, header + 32);           /* MaxCmdSN */

    return 0;
}


/*
 * Login Command
 */
int iscsi_login_cmd_decap(const unsigned char *header, ISCSI_LOGIN_CMD_T *cmd)
{
    EXA_ASSERT_VERBOSE(ISCSI_OPCODE(header) == ISCSI_LOGIN_CMD,
                       "Opcode mismatch ISCSI_LOGIN_CMD");

    cmd->transit = (header[1] & 0x80) ? 1 : 0;               /* Transit */
    cmd->cont = (header[1] & 0x40) ? 1 : 0;                  /* Continue */
    cmd->csg = (header[1] & 0x0c) >> 2;                      /* CSG */
    cmd->nsg = header[1] & 0x03;                             /* NSG */
    cmd->version_max = header[2];                            /* Version-Max */
    cmd->version_min = header[3];                            /* Version-Min */
    cmd->AHSlength = header[4];                              /* TotalAHSLength */
    cmd->length = get_bigendian32(header + 4) & 0x00ffffff;
    cmd->isid = get_bigendian64(header + 8) >> 16;
    cmd->tsih = get_bigendian16(header + 14);
    cmd->tag = get_bigendian32(header + 16);
    cmd->cid = get_bigendian16(header + 20);
    cmd->CmdSN = get_bigendian32(header + 24);               /* CmdSN */
    cmd->ExpStatSN = get_bigendian32(header + 28);           /* ExpStatSN */

    ISCSI_TRACE("iSCSI 'login' message decapsulation");
    ISCSI_TRACE(" - Transit:           %i", cmd->transit);
    ISCSI_TRACE(" - Continue:          %i", cmd->cont);
    ISCSI_TRACE(" - CSG:               %u", cmd->csg);
    ISCSI_TRACE(" - NSG:               %u", cmd->nsg);
    ISCSI_TRACE(" - Version_min:       %u", cmd->version_min);
    ISCSI_TRACE(" - Version_max:       %u", cmd->version_max);
    ISCSI_TRACE(" - TotalAHSLength:    %u", cmd->AHSlength);
    ISCSI_TRACE(" - DataSegmentLength: %u", cmd->length);
    ISCSI_TRACE(" - ISID:              %llx", cmd->isid);
    ISCSI_TRACE(" - TSIH:              %x", cmd->tsih);
    ISCSI_TRACE(" - Task Tag:          0x%x", cmd->tag);
    ISCSI_TRACE(" - CID:               %hu", cmd->cid);
    ISCSI_TRACE(" - CmdSN:             %u", cmd->CmdSN);
    ISCSI_TRACE(" - ExpStatSN:         %u", cmd->ExpStatSN);

    RETURN_NOT_EQUAL("Byte 1, bits 2-3", (header[1] & 0x30) >> 4, 0, 1);
    RETURN_NOT_EQUAL("Bytes 22-23", *((unsigned short *)(header + 22)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 32-35", *((unsigned *)(header + 32)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 36-39", *((unsigned *)(header + 36)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 40-43", *((unsigned *)(header + 40)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 44-47", *((unsigned *)(header + 44)), 0, 1);

    if (cmd->transit)
    {
        if (cmd->nsg <= cmd->csg)
             return -1;

        if ((cmd->nsg != 0) && (cmd->nsg != 1) && (cmd->nsg != 3))
            return -1;
    }

    return 0;
}


/*
 * Login Response
 */
int iscsi_login_rsp_encap(unsigned char *header, const ISCSI_LOGIN_RSP_T *rsp)
{
    ISCSI_TRACE("iSCSI 'login' response encapsulation");
    ISCSI_TRACE(" - Transit:           %i", rsp->transit);
    ISCSI_TRACE(" - Continue:          %i", rsp->cont);
    ISCSI_TRACE(" - CSG:               %u", rsp->csg);
    ISCSI_TRACE(" - NSG:               %u", rsp->nsg);
    ISCSI_TRACE(" - Version_max:       %u", rsp->version_max);
    ISCSI_TRACE(" - Version_active:    %u", rsp->version_active);
    ISCSI_TRACE(" - TotalAHSLength:    %u", rsp->AHSlength);
    ISCSI_TRACE(" - DataSegmentLength: %u", rsp->length);
    ISCSI_TRACE(" - ISID:              0x%llx", rsp->isid);
    ISCSI_TRACE(" - TSIH:              0x%x", rsp->tsih);
    ISCSI_TRACE(" - Task Tag:          0x%x", rsp->tag);
    ISCSI_TRACE(" - StatSN:            %u", rsp->StatSN);
    ISCSI_TRACE(" - ExpCmdSN:          %u", rsp->ExpCmdSN);
    ISCSI_TRACE(" - MaxCmdSN:          %u", rsp->MaxCmdSN);
    ISCSI_TRACE(" - Status-Class:      %u", rsp->status_class);
    ISCSI_TRACE(" - Status-Detail:     %u", rsp->status_detail);

    memset(header, 0, ISCSI_HEADER_LEN);

    header[0] |= 0x00 | ISCSI_LOGIN_RSP;                   /* Opcode */
    if (rsp->transit)
        header[1] |= 0x80;                                 /* Transit */

    if (rsp->cont)
        header[1] |= 0x40;                                 /* Continue */

    header[1] |= ((rsp->csg) << 2) & 0x0c;                 /* CSG */
    if (rsp->transit)
        header[1] |= (rsp->nsg) & 0x03;                    /* NSG */

    header[2] = rsp->version_max;                          /* Version-max */
    header[3] = rsp->version_active;                       /* Version-active */

    set_bigendian64(rsp->isid, header + 6);                /* header +6 +7 will be overiden by length */
    set_bigendian32(rsp->length & 0x00ffffff, header + 4); /* Length (second) */
    header[4] = rsp->AHSlength;                            /* Totalahslength (third) */
    set_bigendian16(rsp->tsih, header + 14);               /* TSIH */
    set_bigendian32(rsp->tag, header + 16);
    set_bigendian32(rsp->StatSN, header + 24);             /* StatRn */
    set_bigendian32(rsp->ExpCmdSN, header + 28);
    set_bigendian32(rsp->MaxCmdSN, header + 32);
    header[36] = rsp->status_class;                        /* Status-Class */
    header[37] = rsp->status_detail;                       /* Status-Detail */

    return 0;
}


/*
 * Logout Command
 */
int iscsi_logout_cmd_decap(const unsigned char *header, ISCSI_LOGOUT_CMD_T *cmd)
{
    EXA_ASSERT_VERBOSE(ISCSI_OPCODE(header) == ISCSI_LOGOUT_CMD,
                       "Opcode mismatch ISCSI_LOGOUT_CMD");

    cmd->immediate = (header[0] & 0x40) ? 1 : 0;        /* Immediate */
    cmd->reason = header[1] & 0x7f;                     /* Reason */
    cmd->tag = get_bigendian32(header + 16);
    cmd->cid = get_bigendian16(header + 20);
    cmd->CmdSN = get_bigendian32(header + 24);          /* CmdSN */
    cmd->ExpStatSN = get_bigendian32(header + 28);

    ISCSI_TRACE("iSCSI 'logout' message decapsulation");
    ISCSI_TRACE(" - Immediate: %i", cmd->immediate);
    ISCSI_TRACE(" - Reason:    %u", cmd->reason);
    ISCSI_TRACE(" - Task Tag:  0x%x", cmd->tag);
    ISCSI_TRACE(" - CID:       %hu", cmd->cid);
    ISCSI_TRACE(" - CmdSN:     %u", cmd->CmdSN);
    ISCSI_TRACE(" - ExpStatSN: %u", cmd->ExpStatSN);

    RETURN_NOT_EQUAL("Byte 0 bit 0", header[0] >> 7, 0, 1);
    RETURN_NOT_EQUAL("Byte 1 bit 0", header[1] >> 7, 1, 1);
    RETURN_NOT_EQUAL("Byte 2", header[2], 0, 1);
    RETURN_NOT_EQUAL("Byte 3", header[3], 0, 1);
    RETURN_NOT_EQUAL("Bytes 4-7", *((unsigned *)(header + 4)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 8-11", *((unsigned *)(header + 8)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 12-15", *((unsigned *)(header + 12)), 0, 1);

    /* FIXME
     * this test probably aims to replace next commented test, but, one may
     * notice that they are absolutly *NOT* equivalents: unsigned is 4 bytes
     * so the test was probably failing *sometimes*
     * What I do not know is 'should this case happen ?'... for now I do not
     * know... feel free to fix... */
    /* RETURN_NOT_EQUAL("Bytes 22-23", *((unsigned *)(header+22)), 0, 1); */
    if (header[22] != 0 || header[23] != 0)
        exalog_warning("***Bytes 22-23 are not ZERO***");

    RETURN_NOT_EQUAL("Bytes 32-35", *((unsigned *)(header + 32)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 36-39", *((unsigned *)(header + 36)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 40-43", *((unsigned *)(header + 40)), 0, 1);
    RETURN_NOT_EQUAL("Bytes 44-47", *((unsigned *)(header + 44)), 0, 1);

    return 0;
}


/*
 * Logout Response
 */
int iscsi_logout_rsp_encap(unsigned char *header, const ISCSI_LOGOUT_RSP_T *rsp)
{
    ISCSI_TRACE("iSCSI 'lougout' response encapsulation");
    ISCSI_TRACE(" - Response:    %u", rsp->response);
    ISCSI_TRACE(" - Length:      %u", rsp->length);
    ISCSI_TRACE(" - Task Tag:    0x%x", rsp->tag);
    ISCSI_TRACE(" - StatSN:      %u", rsp->StatSN);
    ISCSI_TRACE(" - ExpCmdSN:    %u", rsp->ExpCmdSN);
    ISCSI_TRACE(" - MaxCmdSN:    %u", rsp->MaxCmdSN);
    ISCSI_TRACE(" - Time2Wait:   %hu", rsp->Time2Wait);
    ISCSI_TRACE(" - Time2Retain: %hu", rsp->Time2Retain);

    memset(header, 0, ISCSI_HEADER_LEN);

    header[0] |= 0x00 | ISCSI_LOGOUT_RSP;               /* Opcode */
    header[1] |= 0x80;                                  /* Reserved */
    header[2] = rsp->response;                          /* Response */
    set_bigendian32(rsp->length, header + 4);
    set_bigendian32(rsp->tag, header + 16);
    set_bigendian32(rsp->StatSN, header + 24);
    set_bigendian32(rsp->ExpCmdSN, header + 28);
    set_bigendian32(rsp->MaxCmdSN, header + 32);
    set_bigendian16(rsp->Time2Wait, header + 40);
    set_bigendian16(rsp->Time2Retain, header + 42);
    return 0;
}


/*
 * SCSI Command
 */
int iscsi_scsi_cmd_decap(const unsigned char *header, ISCSI_SCSI_CMD_T *cmd)
{
    EXA_ASSERT_VERBOSE(ISCSI_OPCODE(header) == ISCSI_SCSI_CMD,
                       "Opcode mismatch ISCSI_SCSI_CMD");

    cmd->immediate = (header[0] & 0x40) ? 1 : 0;               /* Immediate */
    cmd->final = (header[1] & 0x80) ? 1 : 0;                   /* Final */
    cmd->fromdev = (header[1] & 0x40) ? 1 : 0;                 /* Input */
    cmd->todev = (header[1] & 0x20) ? 1 : 0;                   /* Output */
    cmd->attr = header[1] & 0x07;                              /* ATTR */
    cmd->ahs_len = header[4] *4;                               /* TotalAHSLength
                                                                * rfc3720 say TotalAHSLength
                                                                * 10.2.1.5. say its given in a quadbytes
                                                                * and not in bytes unit */
    cmd->length = get_bigendian32(header + 4) & 0x00ffffff;    /* DataSegmentLength, FIXME: make a proper conversion */
    cmd->lun = lun_get_bigendian(header + 8);                  /* LUN */
    cmd->tag = get_bigendian32(header + 16);                   /* Task Tag */
    cmd->trans_len = get_bigendian32(header + 20);             /* Expected Transfer Length */
    cmd->CmdSN = get_bigendian32(header + 24);                 /* CmdSN */
    cmd->ExpStatSN = get_bigendian32(header + 28);             /* ExpStatSN */

    /* FIXME: we should use a proper SCSI depacking function instead or treat
     * this section of the header outside this iSCSI depacking function.
     * The same comment applies to all the packing/unpacking functions.
     */
    memcpy(cmd->cdb, header + 32, SCSI_CDB_MAX_FIXED_LENGTH);

    RETURN_NOT_EQUAL("Byte 1, Bits 3-4 in SCSI header", header[1] & 0x18, 0, -1);
    RETURN_NOT_EQUAL("Byte 2 in SCSI header", header[2], 0, 1);
    RETURN_NOT_EQUAL("Byte 3 in SCSI header", header[3], 0, 1);

    ISCSI_TRACE("iSCSI 'scsi cmd' message decapsulation");
    ISCSI_TRACE(" - Immediate:         %i", cmd->immediate);
    ISCSI_TRACE(" - Final:             %i", cmd->final);
    ISCSI_TRACE(" - Input:             %i", cmd->fromdev);
    ISCSI_TRACE(" - Output:            %i", cmd->todev);
    ISCSI_TRACE(" - ATTR:              %i", cmd->attr);
    ISCSI_TRACE(" - TotalAHSLength:    %u", cmd->ahs_len);
    ISCSI_TRACE(" - DataSegmentLength: %u", cmd->length);
    ISCSI_TRACE(" - LUN:               %"PRIlun"", cmd->lun);
    ISCSI_TRACE(" - Task Tag:          0x%x", cmd->tag);
    ISCSI_TRACE(" - Transfer Length:   %u", cmd->trans_len);
    ISCSI_TRACE(" - CmdSN:             %u", cmd->CmdSN);
    ISCSI_TRACE(" - ExpStatSN:         %u", cmd->ExpStatSN);
    ISCSI_TRACE(" - CDB:               0x%x", cmd->cdb[0]);

    return 0;
}


/*
 * SCSI Response
 */
int iscsi_scsi_rsp_encap(unsigned char *header, const ISCSI_SCSI_RSP_T *rsp)
{
    ISCSI_TRACE("iSCSI 'rsp' message encapsulation");
    ISCSI_TRACE(" - Bidi Overflow:       %i", rsp->bidi_overflow);
    ISCSI_TRACE(" - Bidi Underflow:      %i", rsp->bidi_underflow);
    ISCSI_TRACE(" - Overflow:            %i", rsp->overflow);
    ISCSI_TRACE(" - Underflow:           %i", rsp->underflow);
    ISCSI_TRACE(" - iSCSI Response:      %u", rsp->response);
    ISCSI_TRACE(" - SCSI Status:         %u", rsp->status);
    ISCSI_TRACE(" - DataSegmentLength:   %u", rsp->length);
    ISCSI_TRACE(" - Task Tag:            0x%x", rsp->tag);
    ISCSI_TRACE(" - StatSN:              %u", rsp->StatSN);
    ISCSI_TRACE(" - ExpCmdSN:            %u", rsp->ExpCmdSN);
    ISCSI_TRACE(" - MaxCmdSN:            %u", rsp->MaxCmdSN);
    ISCSI_TRACE(" - ExpDataSN:           %u", rsp->ExpDataSN);
    ISCSI_TRACE(" - Bidi Residual Count: %u", rsp->bidi_res_cnt);
    ISCSI_TRACE(" - Residual Count:      %u", rsp->basic_res_cnt);

    memset(header, 0, ISCSI_HEADER_LEN);

    header[0] |= 0x00 | ISCSI_SCSI_RSP;               /* Opcode */
    header[1] |= 0x80;                                /* Byte 1 bit 7, FIXME: what does it mean */

    if (rsp->bidi_overflow)
        header[1] |= 0x10;                            /* Bidi overflow */

    if (rsp->bidi_underflow)
        header[1] |= 0x08;                            /* Bidi underflow */

    if (rsp->overflow)
        header[1] |= 0x04;                            /* Overflow */

    if (rsp->underflow)
        header[1] |= 0x02;                            /* Underflow */

    header[2] = rsp->response;                        /* iSCSI Response */
    header[3] = rsp->status;                          /* SCSI Status */
    set_bigendian32(rsp->length, header + 4);
    set_bigendian32(rsp->tag, header + 16);
    set_bigendian32(rsp->StatSN, header + 24);
    set_bigendian32(rsp->ExpCmdSN, header + 28);
    set_bigendian32(rsp->MaxCmdSN, header + 32);
    set_bigendian32(rsp->ExpDataSN, header + 36);
    set_bigendian32(rsp->bidi_res_cnt, header + 40);   /* Bidi Residual Count */
    set_bigendian32(rsp->basic_res_cnt, header + 44);

    EXA_ASSERT((rsp->ahs_len & 3) == 0);               /* FIXME: what does it check ? */
    header[4] = rsp->ahs_len / 4;                      /* TotalAHSLength (this second) */
    return 0;
}


/*
 * Ready To Transfer
 */
int iscsi_r2t_encap(unsigned char *header, const ISCSI_R2T_T *cmd)
{
    ISCSI_TRACE("iSCSI 'r2t' message encapsulation");
    ISCSI_TRACE(" - TotalAHSLength:  %u", cmd->AHSlength);
    ISCSI_TRACE(" - LUN:             %"PRIlun"", cmd->lun);
    ISCSI_TRACE(" - Tag:             0x%x", cmd->tag);
    ISCSI_TRACE(" - Transfer Tag:    0x%x", cmd->transfer_tag);
    ISCSI_TRACE(" - StatSN:          %u", cmd->StatSN);
    ISCSI_TRACE(" - ExpCmdSN:        %u", cmd->ExpCmdSN);
    ISCSI_TRACE(" - MaxCmdSN:        %u", cmd->MaxCmdSN);
    ISCSI_TRACE(" - R2TSN:           %u", cmd->R2TSN);
    ISCSI_TRACE(" - Offset:          %u", cmd->offset);
    ISCSI_TRACE(" - Length:          %u", cmd->length);

    memset(header, 0, ISCSI_HEADER_LEN);

    header[0] |= 0x00 | ISCSI_R2T;                              /* Opcode */
    header[1] |= 0x80;                                          /* FIXME: what does it mean ? */

    set_bigendian32(cmd->AHSlength & 0x00ffffff, header + 4);   /* AHSLength, FIXME: what does it mean ? */
    lun_set_bigendian(cmd->lun, header + 8);                    /* LUN */
    set_bigendian32(cmd->tag, header + 16);                     /* Tag */
    set_bigendian32(cmd->transfer_tag, header + 20);            /* Transfer Tag */
    set_bigendian32(cmd->StatSN, header + 24);                  /* StatSN */
    set_bigendian32(cmd->ExpCmdSN, header + 28);                /* ExpCmdSN */
    set_bigendian32(cmd->MaxCmdSN, header + 32);                /* MaxCmdSN */
    set_bigendian32(cmd->R2TSN, header + 36);                   /* R2TSN */
    set_bigendian32(cmd->offset, header + 40);                  /* Buffer Offset */
    set_bigendian32(cmd->length, header + 44);                  /* Transfer Length */

    return 0;
}


/*
 * SCSI Write Data
 */
int iscsi_write_data_decap(const unsigned char *header, ISCSI_WRITE_DATA_T *cmd)
{
    EXA_ASSERT_VERBOSE(ISCSI_OPCODE(header) == ISCSI_WRITE_DATA,
                       "Opcode mismatch ISCSI_WRITE_DATA");

    cmd->final = (header[1] & 0x80) ? 1 : 0;             /* Final */
    cmd->length = get_bigendian32(header + 4);           /* Length */
    cmd->lun = lun_get_bigendian(header + 8);            /* LUN */
    cmd->tag = get_bigendian32(header + 16);             /* Tag */
    cmd->transfer_tag = get_bigendian32(header + 20);    /* Transfer Tag */
    cmd->ExpStatSN = get_bigendian32(header + 28);       /* ExpStatSN */
    cmd->DataSN = get_bigendian32(header + 36);          /* DataSN */
    cmd->offset = get_bigendian32(header + 40);          /* Buffer Offset */

    RETURN_NOT_EQUAL("Byte 1, Bits 1-7 in write data header", header[1] & 0x7f, 0, 1); /* -> reserved */
    RETURN_NOT_EQUAL("Byte 2 in write data header", header[2], 0, 1);
    RETURN_NOT_EQUAL("Byte 3 in write data header", header[3], 0, 1);
    RETURN_NOT_EQUAL("Byte 4 in write data header", header[4], 0, 1);
    RETURN_NOT_EQUAL("Byte 24-27 in write data header", *((unsigned *)(header + 24)), 0, 1);
    RETURN_NOT_EQUAL("Byte 32-35 in write data header", *((unsigned *)(header + 32)), 0, 1);
    RETURN_NOT_EQUAL("Byte 44-47 in write data header", *((unsigned *)(header + 44)), 0, 1);

    ISCSI_TRACE("iSCSI 'write data' message decapsulation");
    ISCSI_TRACE(" - Final:              %u", cmd->final);
    ISCSI_TRACE(" - DataSegmentLength:  %u", cmd->length);
    ISCSI_TRACE(" - LUN:                %"PRIlun"", cmd->lun);
    ISCSI_TRACE(" - Task Tag:           0x%x", cmd->tag);
    ISCSI_TRACE(" - Transfer Tag:       0x%x", cmd->transfer_tag);
    ISCSI_TRACE(" - ExpStatSN:          %u", cmd->ExpStatSN);
    ISCSI_TRACE(" - DataSN:             %u", cmd->DataSN);
    ISCSI_TRACE(" - Buffer Offset:      %u", cmd->offset);

    return 0;
}


/*
 * SCSI Read Data
 */
int iscsi_read_data_encap(unsigned char *header, const ISCSI_READ_DATA_T *cmd)
{
    ISCSI_TRACE("iSCSI 'read data' message encapsulation");
    ISCSI_TRACE(" - Final:             %i", cmd->final);
    ISCSI_TRACE(" - Acknowledge:       %i", cmd->ack);
    ISCSI_TRACE(" - Overflow:          %i", cmd->overflow);
    ISCSI_TRACE(" - Underflow:         %i", cmd->underflow);
    ISCSI_TRACE(" - S_bit:             %i", cmd->S_bit);
    ISCSI_TRACE(" - Status:            %u", cmd->status);
    ISCSI_TRACE(" - DataSegmentLength: %u", cmd->length);
    ISCSI_TRACE(" - LUN:               %"PRIlun"", cmd->lun);
    ISCSI_TRACE(" - Task Tag:          0x%x", cmd->task_tag);
    ISCSI_TRACE(" - Transfer Tag:      0x%x", cmd->transfer_tag);
    ISCSI_TRACE(" - StatSN:            %u", cmd->StatSN);
    ISCSI_TRACE(" - ExpCmdSN:          %u", cmd->ExpCmdSN);
    ISCSI_TRACE(" - MaxCmdSN:          %u", cmd->MaxCmdSN);
    ISCSI_TRACE(" - DataSN:            %u", cmd->DataSN);
    ISCSI_TRACE(" - Buffer Offset      %u", cmd->offset);
    ISCSI_TRACE(" - Residual Count:    %u", cmd->res_count);

    memset(header, 0, ISCSI_HEADER_LEN);

    header[0] = 0x00 | ISCSI_READ_DATA;                 /* Opcode */


    /* FIXME: this bunch of booleans looks like an enum ... with booleans */
    if (cmd->final)
        header[1] |= 0x80;                              /* Final */

    if (cmd->ack)
        header[1] |= 0x40;                              /* ACK */

    if (cmd->overflow)
        header[1] |= 0x04;                              /* Overflow */

    if (cmd->underflow)
        header[1] |= 0x02;                              /* Underflow */

    if (cmd->S_bit)
        header[1] |= 0x01;                              /* S Bit */

    if (cmd->S_bit)
        header[3] = cmd->status;                        /* Status */

    set_bigendian32(cmd->length, header + 4);           /* Length */
    lun_set_bigendian(cmd->lun, header + 8);            /* LUN */
    set_bigendian32(cmd->task_tag, header + 16);        /* Task Tag */
    set_bigendian32(cmd->transfer_tag, header + 20);    /* Transfer Tag */

    if (cmd->S_bit)
        set_bigendian32(cmd->StatSN, header + 24);      /* StatSN */

    set_bigendian32(cmd->ExpCmdSN, header + 28);        /* ExpCmdSN */
    set_bigendian32(cmd->MaxCmdSN, header + 32);        /* MaxCmdSN */
    set_bigendian32(cmd->DataSN, header + 36);          /* DataSN */
    set_bigendian32(cmd->offset, header + 40);          /* Buffer Offset */

    if (cmd->S_bit)
        set_bigendian32(cmd->res_count, header + 44);   /* Residual Count */

    return 0;
}


/*
 * Reject
 */
int iscsi_reject_encap(unsigned char *header, const ISCSI_REJECT_T *cmd)
{
    ISCSI_TRACE("iSCSI 'reject' message encapsulation");
    ISCSI_TRACE(" - Reason:   %u", cmd->reason);
    ISCSI_TRACE(" - Length:   %u", cmd->length);
    ISCSI_TRACE(" - StatSN:   %u", cmd->StatSN);
    ISCSI_TRACE(" - ExpCmdSN: %u", cmd->ExpCmdSN);
    ISCSI_TRACE(" - MaxCmdSN: %u", cmd->MaxCmdSN);
    ISCSI_TRACE(" - DataSN:   %u", cmd->DataSN);

    memset(header, 0, ISCSI_HEADER_LEN);

    header[0] |= 0x00 | ISCSI_REJECT;                   /* Opcode */
    header[1] |= 0x80;                                  /* FIXME: what does it mean ? */
    header[2] = cmd->reason;                            /* Reason, FIXME: why there is no conversion ?*/
    set_bigendian32(cmd->length, header + 4);           /* Length */
                                                        /* FIXME: looks like something is not defined between 8 and 20 */
    set_bigendian32(cmd->StatSN, header + 24);          /* StatSN */
    set_bigendian32(cmd->ExpCmdSN, header + 28);        /* ExpCmdSN */
    set_bigendian32(cmd->MaxCmdSN, header + 32);        /* MaxCmdSN */
    set_bigendian32(cmd->DataSN, header + 36);          /* DataSN */

    return 0;
}
