/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __ADM_WORKTHREAD_H__
#define __ADM_WORKTHREAD_H__

#include "os/include/os_thread.h"

#include "admind/src/adm_command.h"
#include "admind/src/adm_error.h"
#include "admind/src/rpc.h"

#include "examsg/include/examsg.h"

/* There are:
 * - 2 working threads for the CLI:
 *   - info (for clinfo only)
 *   - cli (for all other commands)
 * - 1 for the event manager
 *
 * Yes, just an anonymous enum. We're in C here, if you want C++, you
 * know where it is. ;-)
 */
typedef enum {
#define FIRST_THREAD_ID CLINFO_THR_ID
  CLINFO_THR_ID,
  CLICOMMAND_THR_ID,
  RECOVERY_THR_ID
#define LAST_THREAD_ID RECOVERY_THR_ID
} worker_thread_id_t;

#define THREAD_ID_IS_VALID(thr_nb) \
  ((thr_nb) >= FIRST_THREAD_ID && (thr_nb) <= LAST_THREAD_ID)

/** Max size of the payload sent to the inbox and barrier mailboxes
    by each node: 640 bytes (total payload: 80 KB) */
#define ADM_MAILBOX_PAYLOAD_PER_NODE 640

#define CMD_UID_INVALID 0 /**< invalid command uid */

typedef unsigned int cmd_uid_t;

cmd_uid_t get_new_cmd_uid(void);

typedef struct command_end
{
  cmd_uid_t cuid; /* uid of the command we are responding to */
  cl_error_desc_t err_desc;
} command_end_t;

typedef struct command
{
  unsigned int uid;        /**< uid of the command */
  adm_command_code_t code; /**< command code */
  exa_uuid_t cluster_uuid; /**< cluster uuid contained in the command received */
  size_t data_size;        /**< size of the data buffer */
  char data[];             /**< data for the command */
} command_t;

typedef struct t_work t_work;

int launch_worker_thread(t_work **thr_ptr, worker_thread_id_t thr_id,
                         ExamsgID id_mine, ExamsgID id_local,
                         ExamsgID id_barrier_odd, ExamsgID id_barrier_even);

void stop_worker_thread(t_work *thr);

int make_worker_thread_exec_command(ExamsgHandle mh, cmd_uid_t uid,
                                    adm_command_code_t cmd_code,
				    const void *cmd_data,
				    size_t data_size);

ExamsgHandle adm_wt_get_inboxmb(void);

ExamsgHandle adm_wt_get_localmb(void);

ExamsgHandle adm_wt_get_barmb(bool even);

admwrk_ctx_t *adm_wt_get_admwrk_ctx(void);

static inline admwrk_ctx_t *admwrk_ctx(void) { return adm_wt_get_admwrk_ctx(); }

const char *adm_wt_get_name(void);


char *adm_cli_ip(void);

void adm_write_inprogress(const char *src_node, const char *desc,
	                  int err, const char *str);

void send_payload_str(const char *str);

void work_thread_handle_msg(Examsg *msg, ExamsgMID *from);
#endif
