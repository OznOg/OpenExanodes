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

#ifndef _DEVICE_H
#define _DEVICE_H

#include "target/iscsi/include/iscsi.h"
#include "target/iscsi/include/lun.h"
#include "target/iscsi/include/target.h"

#include "lum/export/include/export.h"

#include "common/include/exa_nbd_list.h"

/*
 * Interface from target to device:
 * device_init() initializes the device
 * void scsi_command_submit (cmd) synchronously sends a SCSI command to one of the logical units in the device
 */

int device_init(struct nbd_root_list *cmd_root);
int device_cleanup(struct nbd_root_list *cmd_root);

void scsi_command_submit(TARGET_CMD_T *cmd);
/* called by target when a new session from an initiator was created */
void scsi_new_session(int session_id);
/* called by target when a new session from an initiator was deleted */
void scsi_del_session(int session_id);
/* called by target for a logical unit reset */
void scsi_logical_unit_reset(lun_t lun);

const export_t *scsi_get_export(lun_t lun);

/** What a transport layer above SCSI must provide to the SCSI layer. */
typedef struct {
    int (*update_lun_access_authorizations)(const export_t *export);
    bool (*lun_access_authorized)(TARGET_SESSION_T *session, lun_t lun);
    int (*async_event)(int session_id, lun_t lun, void *sense_data, size_t len);
} scsi_transport_t;

/**
 * Register a transport.
 *
 * @param[in] t  Transport to register.
 *
 * NOTE: The transport given *must* live until scsi_unregister_transport()
 * is called, as the SCSI layer doesn't make a copy of it.
 *
 * @return EXA_SUCCESS if successful, -EINVAL otherwise
 */
int scsi_register_transport(const scsi_transport_t *t);

/**
 * Unregister a transport.
 *
 * @param[in] t  Transport to unregister.
 *
 * NOTE: The transport given *must* be registered.
 */
void scsi_unregister_transport(const scsi_transport_t *t);

#endif /* _DEVICE_H */
