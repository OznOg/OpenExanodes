/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef SCSI_PERSISTENT_RESERVATIONS_H
#define SCSI_PERSISTENT_RESERVATIONS_H

#include "target/iscsi/include/lun.h"

#define SCSI_PERSISTENT_RESERVE_OUT_CDB_SIZE 40

struct pr_context;
typedef struct pr_context pr_context_t;

typedef void (*pr_send_sense_data_fn_t)(int session_id, lun_t lun, int status, int sense, int asc_ascq);

/** alloc data for persistent reservation */
pr_context_t *pr_context_alloc(pr_send_sense_data_fn_t send_sense_data);

/** free data for persistent reservation */
void __pr_context_free(pr_context_t *pr_context);
#define pr_context_free(pr_context) \
    (__pr_context_free(pr_context), pr_context = NULL)

/** data need for packing */
int pr_context_packed_size(const pr_context_t *pr_context);

/** packing in a buffer */
int pr_context_pack(const pr_context_t *pr_context, void *packed_buffer, int size);

/** unpacking a buffer */
int pr_context_unpack(pr_context_t *pr_context, const void *packed_buffer, int size);

/** adding a new session from a target */
void pr_add_session(pr_context_t *pr_context, int session_id);

/** deleting a session from a target */
void pr_del_session(pr_context_t *pr_context, int session_id);

/** Used for spc2 reserve/release */
void pr_reset_lun_reservation(pr_context_t *pr_context, lun_t lun);

/** process persistent reservation int and give response according the
 * persistent_reservations
 */
void pr_reserve_in(const pr_context_t *pr_context, lun_t lun, const unsigned char *cdb,
                   unsigned char *data_out, int session_id,
                   scsi_command_status_t *scsi_status);

/** process persistent reservation out and spc2 reserve/release modify
 * persistent_reservations according the cdb
 */
void pr_reserve_out(pr_context_t *pr_context, lun_t lun,
                    const unsigned char *cdb, int session_id,
                    scsi_command_status_t *scsi_status);

/** say if the cdb can be done according resservation right */
bool pr_check_rights(const pr_context_t *pr_context, lun_t lun,
                     const unsigned char *cdb, int session_id);

#endif
