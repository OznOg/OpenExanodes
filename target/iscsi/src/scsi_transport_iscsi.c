/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
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

#include <string.h>
#include <signal.h>
#include <errno.h>

#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"
#include "common/include/exa_socket.h"
#include "common/include/threadonize.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_nbd_list.h"

#include "log/include/log.h"

#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_network.h"
#include "os/include/os_stdio.h"
#include "os/include/os_time.h"
#include "os/include/strlcpy.h"

#include "target/iscsi/include/target.h"
#include "target/iscsi/include/device.h"
#include "target/iscsi/include/parameters.h"
#include "target/iscsi/include/iscsi_negociation.h"
#include "target/iscsi/include/scsi.h"
#include "target/iscsi/include/lun.h"
#include "target/iscsi/include/iqn.h"

#include "lum/export/include/export.h"

#include "target/iscsi/include/endianness.h"

#define ISCSI_PORT 3260

#define ISCSI_SOCK_MSG_BYTE_ALIGN    4

#define ISCSI_THREAD_STACK_SIZE 16384
#define ISCSI_SESSION_THREAD_STACK_SIZE 1048576

static os_thread_t iscsi_thread_tid = 0;

/* Structure for storing negotiated parameters that are
 * frequently accessed on an active session
 */
typedef struct
{
    uint32_t max_burst_length;
    uint32_t first_burst_length;
    uint32_t max_data_seg_length;
    uint8_t  initial_r2t;
    uint8_t  immediate_data;
} iscsi_sess_param_t;

struct session_t
{
    struct target_cmd_t *cmd_next;
    int                id;
    volatile int       cmd_pending;
    int                sock;
    unsigned short     cid;
    unsigned           StatSN, ExpCmdSN, MaxCmdSN;
    int                IsFullFeature;
    int                IsLoggedIn;
    int                LoginStarted;
    unsigned long long isid;
    int                tsih;
    iscsi_parameter_t *params;
    iscsi_sess_param_t sess_params;
    os_thread_mutex_t    slock;
    os_thread_mutex_t    send_lock;
    int                cmd_done_waiter_nb;
    os_sem_t           waiter;
    /** tells if the session can access the different LUNs */
    bool    	       authorized_luns[MAX_LUNS];
};

int sess_get_id(const TARGET_SESSION_T *sess)
{
    EXA_ASSERT(sess != NULL);
    return sess->id;
}

static struct nbd_root_list g_cmd;
static struct nbd_root_list g_session_q;
static struct nbd_root_list g_buffer;
static struct nbd_list g_cmd_defered_send;
static int g_sock;

/* "The target portal group tag is a 16-bit binary-value that uniquely
 *  identifies a portal group within an iSCSI target node.".
 * FIXME Maybe it should not be hardcoded, or maybe it can be, it seems
 * to be used for complicated iSCSI stuff that we don't do.
 */
#define TARGET_PORTAL_GROUP_TAG "1"

static iqn_t g_iqn;
#define target_name() iqn_to_str(&g_iqn)

static size_t config_lun_queue_depth;
static size_t config_lun_buffer_size;
static in_addr_t config_listen_address;

static int num_cluster_listen_addresses = 0;
static char cluster_listen_addresses[10 * EXA_MAX_NODES_NUMBER]
            [OS_NET_ADDR_STR_LEN + 1 + 5 + 2 + 1];
            /* Max IP len + : + 5-digits port + ,1 + \0. */

static volatile bool target_run;

static int update_lun_authorizations(const export_t *export);
static bool session_lun_authorized(TARGET_SESSION_T *session, lun_t lun);
static int async_event(int session_id, lun_t lun, void *sense_data, size_t len);

static scsi_transport_t iscsi_transport = {
    .update_lun_access_authorizations = update_lun_authorizations,
    .lun_access_authorized = session_lun_authorized,
    .async_event = async_event
};

/*
 * Internal functions
 */

static int execute_t(TARGET_SESSION_T *sess, unsigned char *header);
static int login_command_t(TARGET_SESSION_T *sess, unsigned char *header);
static int logout_command_t(TARGET_SESSION_T *sess, unsigned char *header);
static int text_command_t(TARGET_SESSION_T *sess, unsigned char *header);
static int nop_out_t(TARGET_SESSION_T *sess, unsigned char *header);
static int task_command_t(TARGET_SESSION_T *sess, unsigned char *header);
static int scsi_command_t(TARGET_SESSION_T *sess, unsigned char *header);
static int iscsi_write_data(TARGET_SESSION_T *sess, unsigned char *header);
static void reject_t(TARGET_SESSION_T *sess, unsigned char *header,
                    unsigned char reason);
static void worker_proc_t(void *arg);
static void cmd_rsp_worker(void *arg);

void target_set_addresses(int num_addrs, const in_addr_t addrs[])
{
    int i;
    for (i = 0; i < num_addrs; i++)
    {
        struct in_addr addr = { .s_addr = addrs[i] };

        os_snprintf(cluster_listen_addresses[i], sizeof(cluster_listen_addresses[i]),
                    "%s:%"PRIu16","TARGET_PORTAL_GROUP_TAG, os_inet_ntoa(addr),
                    ISCSI_PORT);
    }

    num_cluster_listen_addresses = num_addrs;
}

void set_session_parameters(const iscsi_parameter_t *head,
                            iscsi_sess_param_t *sess_params)
{
    uint32_t val;

    memset(sess_params, 0, sizeof(iscsi_sess_param_t));

    /* These parameters are standard and assuming that they are always */
    /* present in the list (head). */
    if (param_list_get_value(head, "MaxBurstLength") != NULL)
    {
        if (to_uint32(param_list_get_value(head, "MaxBurstLength"), &val) == 0)
            sess_params->max_burst_length = val;
        else
            exalog_error("Wrong value for MaxBurstLength: %s",
                    param_list_get_value(head, "MaxBurstLength"));
    }

    if (param_list_get_value(head, "FirstBurstLength") != NULL)
    {
        if (to_uint32(param_list_get_value(head, "FirstBurstLength"), &val) == 0)
            sess_params->first_burst_length = val;
        else
            exalog_error("Wrong value for FirstBurstLength: %s",
                    param_list_get_value(head, "FirstBurstLength"));
    }

    if (param_list_get_value(head, "MaxRecvDataSegmentLength") != NULL)
    {
        if (to_uint32(param_list_get_value(head, "MaxRecvDataSegmentLength"), &val) == 0)
            sess_params->max_data_seg_length = val;
        else
            exalog_error("Wrong value for MaxRecvDataSegmentLength: %s",
                    param_list_get_value(head, "MaxRecvDataSegmentLength"));
    }

    if (param_list_value_is_equal(head, "InitialR2T", "Yes"))
        sess_params->initial_r2t = 1;
    else
        sess_params->initial_r2t = 0;

    if (param_list_value_is_equal(head, "ImmediateData", "Yes"))
        sess_params->immediate_data = 1;
    else
        sess_params->immediate_data = 0;
}

/*
 * NOTE: iscsi_sock_msg() alters *sg when socket sends and recvs return having only
 * transfered a portion of the iovec.  When this happens, the iovec is modified
 * and resent with the appropriate offsets.
 */
static int iscsi_sock_msg(int sock, socket_operation_t operation, int len, void *data)
{
    int rc = 0;
    unsigned char padding[ISCSI_SOCK_MSG_BYTE_ALIGN];
    unsigned padding_len = 0;
    unsigned char *data_act;
    int len_act;
    int len_reminding;

    data_act = data;
    len_act = len;

    /* Add padding */
    if ((len % ISCSI_SOCK_MSG_BYTE_ALIGN) == 0)
    {
        padding_len = 0;
    }
    else
    {
        padding_len = ISCSI_SOCK_MSG_BYTE_ALIGN -
                      (len % ISCSI_SOCK_MSG_BYTE_ALIGN);
        memset(padding, 0, padding_len);
    }

    len_reminding = len + padding_len;

    while (len_reminding > 0)
    {
        switch (operation)
        {
        case SOCKET_SEND:
            rc = os_send(sock, data_act, len_act);
            break;
        case SOCKET_RECV:
            rc = os_recv(sock, data_act, len_act, 0);
            break;
        }

        if (rc == 0)
            return len - (len_reminding - padding_len);

        if ((rc < 0) && ((rc == -EAGAIN) || (rc == -EINTR)))
            continue;

        if (rc < 0)
            return len - (len_reminding - padding_len);

        len_reminding -= rc;
        len_act -= rc;

        data_act += rc;
        if (len_act == 0)
        {
            len_act = padding_len;
            data_act = padding;
        }
    }

    return len;
}

/**
 * @brief Get a cmd structure from the pool and insert it into the session
 * chained list
 *
 * FIXME: This function should be split in two, the insertion being done after
 * the command has been properly initialized (see 'scsi_command_t')
 */
static TARGET_CMD_T *cmd_get_and_link(TARGET_SESSION_T *sess)
{
    TARGET_CMD_T *cmd;

    cmd = nbd_list_remove(&g_cmd.free, NULL, LISTWAIT);
    EXA_ASSERT(cmd != NULL);

    memset(cmd, 0, sizeof(TARGET_CMD_T));

    cmd->data = nbd_list_remove(&g_buffer.free, NULL, LISTWAIT);
    EXA_ASSERT(cmd->data != NULL);

    os_thread_mutex_lock(&sess->slock);

    /* update chained list */
    cmd->cmd_prev = NULL;
    cmd->cmd_next = sess->cmd_next;
    if (sess->cmd_next != NULL)
        sess->cmd_next->cmd_prev = cmd;

    sess->cmd_next = cmd;

    sess->cmd_pending++;
    cmd->sess = sess;

    os_thread_mutex_unlock(&sess->slock);

    return cmd;
}


static void cmd_put(TARGET_CMD_T *cmd)
{
    TARGET_CMD_T *prev;
    TARGET_CMD_T *next;

    os_thread_mutex_lock(&cmd->sess->slock);

    nbd_list_post(&g_buffer.free,  cmd->data, -1);
    cmd->sess->cmd_pending--;
    prev = cmd->cmd_prev;
    next = cmd->cmd_next;

    if (next != NULL)
        next->cmd_prev = prev;

    if (prev != NULL)
        prev->cmd_next = next;
    else
        cmd->sess->cmd_next = next;

    while (cmd->sess->cmd_done_waiter_nb > 0)
    {
        os_sem_post(&cmd->sess->waiter);
        cmd->sess->cmd_done_waiter_nb--;
    }

    os_thread_mutex_unlock(&cmd->sess->slock);

    cmd->sess = NULL;
    nbd_list_post(&g_cmd.free, cmd, -1);
}


static TARGET_CMD_T *cmd_from_tag(TARGET_SESSION_T *sess, unsigned tag)
{
    TARGET_CMD_T *cmd;

    os_thread_mutex_lock(&sess->slock);
    cmd = sess->cmd_next;
    while (cmd != NULL)
    {
        if (cmd->scsi_cmd.tag == tag)
            break;

        cmd = cmd->cmd_next;
    }
    os_thread_mutex_unlock(&sess->slock);
    return cmd;
}

static void sess_cmd_display(TARGET_SESSION_T * sess)
{
    TARGET_CMD_T *cmd;
    unsigned char * cdb;
    os_thread_mutex_lock(&sess->slock);
    cmd = sess->cmd_next;
    while (cmd != NULL)
    {
	cdb =  &cmd->scsi_cmd.cdb[0];
	exalog_debug("TAG %4d CDB:<%2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x>",
		     cmd->scsi_cmd.tag, cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7],
		     cdb[8], cdb[9], cdb[10], cdb[11], cdb[12], cdb[13], cdb[14], cdb[15]);
        cmd = cmd->cmd_next;
    }
    os_thread_mutex_unlock(&sess->slock);
}


/**
 * @brief tells if a session is authorized to access a given LUN
 *
 * @param[in] session  pointer to the session
 * @param[in] lun      logical unit number
 *
 * @return true if the session is authorized
 */
static bool session_lun_authorized(TARGET_SESSION_T *session, lun_t lun)
{
    EXA_ASSERT(lun < MAX_LUNS);

    return session->authorized_luns[lun];
}


/**
 * @brief Update the table of authorized LUNs
 *
 * @param[inout] session  pointer to the session
 *
 */
static void session_update_authorized_luns(TARGET_SESSION_T *session)
{
    iqn_t initiator_iqn;
    int i;

    iqn_from_str(&initiator_iqn, param_list_get_value(session->params, "InitiatorName"));

    for (i = 0; i < MAX_LUNS; i++)
    {
        /* ugly and infringing all the layer isolation */
        iqn_filter_policy_t policy;
        const export_t *export = scsi_get_export(i);

        if (export == NULL)
            continue;

        policy = export_iscsi_get_policy_for_iqn(export, &initiator_iqn);

        session->authorized_luns[i] = policy == IQN_FILTER_ACCEPT;
    }
}


static bool cmd_wait_tag(TARGET_SESSION_T * sess, unsigned tag)
{
    TARGET_CMD_T *cmd;
    bool found = false;
    do
    {
        os_thread_mutex_lock(&sess->slock);
        sess->cmd_done_waiter_nb++;

	cmd = sess->cmd_next;
        while (cmd != NULL)
        {
            if (cmd->scsi_cmd.tag == tag)
            {
                found = true;
                break;
	    }
	    cmd = cmd->cmd_next;
        }

	/* no cmd with this tag */
	if (cmd == NULL)
	    sess->cmd_done_waiter_nb--;

        os_thread_mutex_unlock(&sess->slock);

	if (cmd != NULL)
            os_sem_wait(&sess->waiter);
    } while (cmd != NULL);
    return found;
}


static void session_init_sn(TARGET_SESSION_T *sess)
{
    sess->MaxCmdSN = 0;
    sess->ExpCmdSN = 0;
    sess->StatSN = 0;
}


static void session_cmd_sn(TARGET_SESSION_T *sess, unsigned CmdSN, unsigned ExpStatSN)
{
    os_thread_mutex_lock(&sess->slock);

    if (sess->StatSN + 1 < ExpStatSN)
    {
        if (sess->StatSN != 0)
            exalog_warning("iSCSI: StatSN %u ExpStatSN %u so reset ExpStatSN",
			   sess->StatSN, ExpStatSN);

	/* FIXME this inititalization done on the only fact that
	 * sess->StatSN == 0 is brain dead and buggy. Such an initialization
	 * sould be explicit. */
	/* sess->StatSN == 0 so the initiator init StatSN */
        sess->StatSN = ExpStatSN - 1;
    }

    if (CmdSN > sess->ExpCmdSN && sess->ExpCmdSN != 0)
        exalog_warning("iSCSI: CmdSN %u ExpCmdSN %u so reset ExpCmdSN",
		       CmdSN, sess->ExpCmdSN);

    /* sess->ExpCmdSN == 0 so the initiator init sess->ExpCmdSN */
    sess->ExpCmdSN = MAX(CmdSN, sess->ExpCmdSN);

    /* Actually computation of MaxCmdSN is useless here: MAxCmdSN can change
     * only if a command is finished and replied. Doing it here is meaningless
     * as the command was not yet performed. As a matter of fact, MaxCmdSN will
     * be recomputed when building the response message. */
    sess->MaxCmdSN = sess->ExpCmdSN - 1 + config_lun_queue_depth - sess->cmd_pending;

    /* FIXME Warning: we don't handle wrapping at UINT_MAX here */
    if (sess->MaxCmdSN + 1 < sess->ExpCmdSN)
        exalog_warning("iSCSI: command sequence number out of range: "
		       "MaxCmdSN %u ExpCmdSN %u CmdSN %u CMD in progress %u",
		       sess->MaxCmdSN, sess->ExpCmdSN, CmdSN, sess->cmd_pending);

    os_thread_mutex_unlock(&sess->slock);
}


static void session_newcmd_sn(TARGET_SESSION_T *sess, unsigned CmdSN, unsigned ExpStatSN)
{
    os_thread_mutex_lock(&sess->slock);

    if (sess->StatSN + 1 < ExpStatSN)
    {
        if (sess->StatSN != 0)
            exalog_error("iSCSI: StatSN %u ExpStatSN %u so reset ExpStatSN",
                         sess->StatSN, ExpStatSN);

	/* FIXME this inititalization done on the only fact that
	 * sess->StatSN == 0 is brain dead and buggy. Such an initialization
	 * sould be explicit. */
	/* sess->StatSN == 0 so the initiator init StatSN */
        sess->StatSN = ExpStatSN - 1;
    }

    if (CmdSN > sess->ExpCmdSN && sess->ExpCmdSN != 0)
        exalog_error("iSCSI: CmdSN %u ExpCmdSN %u so reset ExpCmdSN",
		     CmdSN, sess->ExpCmdSN);

    /* sess->ExpCmdSN == 0 so the initiator init sess->ExpCmdSN */
    sess->ExpCmdSN = MAX(CmdSN + 1, sess->ExpCmdSN);

    /* Actually computation of MaxCmdSN is useless here: MAxCmdSN can change
     * only if a command is finished and replied. Doing it here is meaningless
     * as the command was not yet performed. As a matter of fact, MaxCmdSN will
     * be recomputed when building the response message. */
    sess->MaxCmdSN = sess->ExpCmdSN - 1 + config_lun_queue_depth - sess->cmd_pending;

    /* FIXME Warning: we don't handle wrapping at UINT_MAX here */
    if (sess->MaxCmdSN + 1 < sess->ExpCmdSN)
        exalog_warning("iSCSI: new command sequence number out of range: "
		       "MaxCmdSN %u  ExpCmdSN %u CmdSN %u CMD in progress %d",
		       sess->MaxCmdSN, sess->ExpCmdSN, CmdSN, sess->cmd_pending);

    os_thread_mutex_unlock(&sess->slock);
}


static void session_stat_sn(TARGET_SESSION_T *sess, unsigned *StatSN,
                            unsigned *MaxCmdSN, unsigned *ExpCmdSN)
{
    os_thread_mutex_lock(&sess->slock);

    /* since queue depth can change, we must recalc MaxCmdSN
     * FIXME: this comment is false 'config_lun_queue_depth' is set by 'target_init'
     */
    sess->MaxCmdSN = sess->ExpCmdSN - 1 + config_lun_queue_depth - sess->cmd_pending;

    if (MaxCmdSN != NULL)
        *MaxCmdSN = sess->MaxCmdSN;

    if (ExpCmdSN != NULL)
        *ExpCmdSN = sess->ExpCmdSN;

    if (StatSN != NULL)
        *StatSN = sess->StatSN;

    os_thread_mutex_unlock(&sess->slock);
}


static void session_newstat_sn(TARGET_SESSION_T *sess, unsigned *StatSN,
                               unsigned *MaxCmdSN, unsigned *ExpCmdSN)
{
    os_thread_mutex_lock(&sess->slock);

    /* since queue depth can change, we must recalc MaxCmdSN
     * FIXME: this comment is false 'config_lun_queue_depth' is set by 'target_init'
     */
    sess->MaxCmdSN = sess->ExpCmdSN - 1 + config_lun_queue_depth - sess->cmd_pending;
    sess->StatSN++;

    *MaxCmdSN = sess->MaxCmdSN;
    *ExpCmdSN = sess->ExpCmdSN;
    *StatSN = sess->StatSN;

    os_thread_mutex_unlock(&sess->slock);
}


static void session_newstat_logout_sn(TARGET_SESSION_T *sess, unsigned *StatSN,
                                      unsigned *MaxCmdSN, unsigned *ExpCmdSN)
{
    os_thread_mutex_lock(&sess->slock);
    *MaxCmdSN = sess->ExpCmdSN - 1;
    *ExpCmdSN = sess->ExpCmdSN;
    sess->StatSN++;
    *StatSN = sess->StatSN;
    os_thread_mutex_unlock(&sess->slock);
}

static int __session_sock_send_header_and_data(int sock, void *header,
                                               int header_len, void *data,
                                               int data_len)
{
    int rc;

    /* Make sure data is not NULL if data_len is not 0 */
    EXA_ASSERT(data != NULL || data_len == 0);

    rc = iscsi_sock_msg(sock, SOCKET_SEND, header_len, header);

    if (rc < header_len)
	return -1;

    if (data_len <= 0)
	return header_len;

    rc = iscsi_sock_msg(sock, SOCKET_SEND, data_len, data);

    if (rc < data_len)
	return -1;

    return header_len + data_len;
}

static void session_sock_send_header_and_data(TARGET_SESSION_T *sess,
	                                      void *header,
					      void *data, int data_len)
{
    __session_sock_send_header_and_data(sess->sock, header, ISCSI_HEADER_LEN,
                                        data, data_len);
}


int target_init(size_t queue_depth, size_t buffer_size, const iqn_t *target_iqn,
                in_addr_t listen_address)
{
    int i;
    int index;

    /* FIXME: artificially increase the target queue depth to workaround bug #4308
     *
     * The bug was first patched with r39943 but it infringed RFC 3720 (see 3.2.2.1),
     * so I suppose that there is still a problem somewhere.
     */
    config_lun_queue_depth = queue_depth + 1;
    config_lun_buffer_size = buffer_size;
    config_listen_address = listen_address;

    /* set target iqn */
    iqn_copy(&g_iqn, target_iqn);

    /* create queue of free sessions structures */
    if (nbd_init_root(CONFIG_TARGET_MAX_SESSIONS, sizeof(TARGET_SESSION_T),
		      &g_session_q) < 0)
    {
        exalog_error("Failed to allocate the sessions pool");
        return -1;
    }

    /* allocate target commands pool
     *
     * This sizing intends to support up to 'config_lun_queue_depth'
     * non-immediate commands and 1 immediate command (they are blocking)
     */
    if (nbd_init_root((config_lun_queue_depth + 1) * CONFIG_TARGET_MAX_SESSIONS,
		      sizeof(TARGET_CMD_T), &g_cmd) < 0)
    {
        exalog_error("Failed to allocate the commands pool");
        return -1;
    }
    nbd_init_list(&g_cmd, &g_cmd_defered_send);

    /* allocate target buffers pool */
    if (nbd_init_root((config_lun_queue_depth + 1) * CONFIG_TARGET_MAX_SESSIONS,
                      config_lun_buffer_size, &g_buffer) < 0)
    {
        exalog_error("Failed to allocate the buffers pool");
        return -1;
    }

    /* Careful this initialization loop works *_only_* because nbd_list_remove
     * implicitly takes elements from the list head, AND nbd_list_post put them
     * at the back. (implicit FIFO) */
    for (i = 0; i < CONFIG_TARGET_MAX_SESSIONS; i++)
    {
        TARGET_SESSION_T *temp = nbd_list_remove(&g_session_q.free, &index, LISTWAIT);
        EXA_ASSERT(temp != NULL);

        os_thread_mutex_init(&temp->slock);
        os_thread_mutex_init(&temp->send_lock);

        os_sem_init(&temp->waiter,0);
        temp->cmd_done_waiter_nb = 0;
        temp->id = index;               /* do we need this ? */
	temp->IsLoggedIn = 0;
        temp->cmd_pending = 0;
        temp->cmd_next = NULL;
        nbd_list_post(&g_session_q.free, temp, -1);
    }

    scsi_register_transport(&iscsi_transport);

    /* initialize the device */
    if (device_init(&g_cmd) != 0)
    {
        exalog_error("Failed to initialize the target's SCSI device");
        return -1;
    }

    return 0;
}


int target_shutdown(void)
{
    int rc;

    if (target_run)
        iscsi_stop_target();

    /* Burn all sessions, this actually waits for all worker threads to finish */
    for (rc = 0; rc < CONFIG_TARGET_MAX_SESSIONS; rc++)
    {
        void *dummy = nbd_list_remove(&g_session_q.free, NULL, LISTWAIT);
        EXA_ASSERT(dummy != NULL);
    }

    /* TODO : code this function or remove it */
    device_cleanup(&g_cmd);

    scsi_unregister_transport(&iscsi_transport);

    return 0;
}

int iscsi_start_target(void)
{
    if (target_run)
        return EXA_SUCCESS;

    target_run = true;

    /* Launch the iSCSI target thread to listen connection */
    if (!exathread_create_named(&iscsi_thread_tid, ISCSI_THREAD_STACK_SIZE,
                                target_listen, NULL, "iscsi_thread"))
    {
        target_run = false;
        exalog_error("Failed to create iSCSI reception thread");
        return -1;
    }

    return EXA_SUCCESS;
}

int iscsi_stop_target(void)
{
    if (!target_run)
        return EXA_SUCCESS;

    target_run = false;

    os_shutdown(g_sock, SHUT_RDWR);
    os_closesocket(g_sock);

    if (iscsi_thread_tid != 0)
    {
        os_thread_join(iscsi_thread_tid);
        iscsi_thread_tid = 0;
    }

    return EXA_SUCCESS;
}

void target_listen(void *dummy)
{
    int socket_list[CONFIG_TARGET_MAX_SESSIONS] = { -1 };
    int one = 1, rc;
    int socket_buffer_size;
    socklen_t localAddrLen;
    struct sockaddr_in localAddr;
    os_thread_t rsp_thread;
    char local[16];
    char remote[16];
    struct sockaddr_in laddr;
    struct linger linger;
    const char *step = NULL;
    int err;

    /* Create/Bind/Listen */
    step = "creating socket";
    if ((g_sock = os_socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        err = g_sock;
        goto done;
    }

    step = "setting reuse option";
    err = os_setsockopt(g_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (err < 0)
        goto done;

    step = "setting nodelay option";
    err = os_setsockopt(g_sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    if (err < 0)
        goto done;

    step = "setting send buffer";
    socket_buffer_size = 128 * 1024;
    err = os_setsockopt(g_sock, SOL_SOCKET, SO_SNDBUF,
                        &socket_buffer_size, sizeof(socket_buffer_size));
    if (err < 0)
        goto done;

    step = "setting receive buffer";
    socket_buffer_size = 128 * 1024;
    err = os_setsockopt(g_sock, SOL_SOCKET, SO_RCVBUF,
                        &socket_buffer_size, sizeof(socket_buffer_size));
    if (err < 0)
        goto done;

    memset((char *) &laddr, 0, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_addr.s_addr = config_listen_address;
    laddr.sin_port = htons(ISCSI_PORT);

    step = "binding socket";
    err = os_bind(g_sock, (struct sockaddr *)&laddr, sizeof(laddr));
    if (err < 0)
        goto done;

    step = "listening";
    err = os_listen(g_sock, CONFIG_TARGET_MAX_SESSIONS);
    if (err < 0)
        goto done;

    step = "creating rsp worker thread";
    if (!os_thread_create(&rsp_thread, 0, cmd_rsp_worker, NULL))
    {
        err = -EXA_ERR_THREAD_CREATE;
        goto done;
    }

    step = "in connection acceptation loop";
    while (target_run)
    {
        TARGET_SESSION_T *sess = NULL;
        int index, i;
        os_thread_t thread;
        struct sockaddr_in remoteAddr;
        int remoteAddrLen;
        int fd;
        int r;

        /* Accept connection, spawn session thread, and */
        /* clean up old threads */
        remoteAddrLen = sizeof(remoteAddr);
        memset((char *)&remoteAddr, 0, sizeof(remoteAddr));

        /* Rq: accept is unblocked on stop by the close of the socket */
        fd = os_accept(g_sock, (struct sockaddr *)&remoteAddr, &remoteAddrLen);

        if (fd < 0)
        {
            r = fd;
            /* EINVAL is returned when the socket is broken by the close */
            if (r != -EINVAL)
                exalog_error("Incoming initiator connection failed: %s (%d)",
                             os_strerror(-r), r);
            continue;
        }

        /* Verify if we should still run */
        if (!target_run)
        {
            os_shutdown(fd, SHUT_RDWR);
            os_closesocket(fd);
            break;
        }

        localAddrLen = sizeof(localAddr);
        memset(&localAddr, 0, sizeof(localAddr));

	r = getsockname(fd, (struct sockaddr *)&localAddr, &localAddrLen);
	if (r < 0)
        {
            exalog_error("Cannot get socket name on socket %d: %s (%d)",
		         fd, os_strerror(errno), errno);
            os_shutdown(fd, SHUT_RDWR);
            os_closesocket(fd);
            continue;
        }

        remoteAddrLen = sizeof(remoteAddr);
        memset((char *)&remoteAddr, 0, sizeof(remoteAddr));
        r = os_getpeername(fd, (struct sockaddr *)&remoteAddr, &remoteAddrLen);
	if (r < 0)
        {
            exalog_error("Cannot get peer name on socket %d: %s (%d)", fd,
                         os_strerror(-r), r);
            os_shutdown(fd, SHUT_RDWR);
            os_closesocket(fd);
            continue;
        }

        one = 1;
        r = os_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	if (r < 0)
        {
            exalog_error("Failed setting nodelay option on socket %d: %s (%d)",
                         fd, os_strerror(-r), r);
            os_shutdown(fd, SHUT_RDWR);
            os_closesocket(fd);
            continue;
        }

        /* Fix the delay in socket shutdown */
	linger.l_onoff = 1;
	linger.l_linger = 0;
	r = os_setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
        if (r < 0)
        {
            exalog_error("Failed setting linger option on socket %d: %s (%d)",
                         fd, os_strerror(-r), r);
            os_shutdown(fd, SHUT_RDWR);
            os_closesocket(fd);
	    continue;
        }

        one = 128 * 1024;
        os_setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &one, sizeof(one));

        one = 128 * 1024;
        os_setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &one, sizeof(one));

        /* get a free session structure */
        sess = nbd_list_remove(&g_session_q.free, &index, LISTNOWAIT);
        if (sess == NULL)
        {
            exalog_warning("Failed to open iSCSI connection: too many connections opened");
            os_shutdown(fd, SHUT_RDWR);
            os_closesocket(fd);
            continue;
        }

        /* initialize the session structure */
        sess->sock = fd;
        sess->LoginStarted = 0;
        for (i = 0; i < MAX_LUNS; i++)
            sess->authorized_luns[i] = false;

        sprintf(local, "%s", os_inet_ntoa(localAddr.sin_addr));
        sprintf(remote, "%s", os_inet_ntoa(remoteAddr.sin_addr));

        /* FIXME: I don't think that 'remote' (or 'local') is needed anymore
         * except for traces
         */
        exalog_info("Connection accepted: session %d, local %s, remote %s",
                    sess->id, local, remote);

        if (!exathread_create_named(&thread,
                                    ISCSI_SESSION_THREAD_STACK_SIZE, worker_proc_t, sess,
                                    "iscsi_session"))
        {
            exalog_error("Iscsi session thread creation failed.");
            nbd_list_post(&g_session_q.free, sess, -1);
            os_shutdown(fd, SHUT_RDWR);
            os_closesocket(fd);
            continue;
        }

        /* XXX index is the nbd list index of the session. This index is
         * supposed to be unique for all nbd list elements, so here we use it
         * to index our array. Actually, if a session is being used here,
         * then the former socket value can be overwritten. */
        socket_list[index] = fd;
        os_thread_detach(thread);
    }

    /* Close all sockets that may have been opened to make worker thread finish
     * FIXME This is REALLY REALLY CRAPPY please find a better way to
     * terminate and wait completion of worker threads... */
    for (rc = 0; rc < CONFIG_TARGET_MAX_SESSIONS; rc++)
    {
        /* FIXME Obviously, many sockets that are being closed were already
         * closed by worker threads that finished 'properly', but here we have
         * no way to know which ones, so, we close all of them, just to make
         * sure... */
        os_shutdown(socket_list[rc], SHUT_RDWR);
        os_closesocket(socket_list[rc]);
    }

    os_thread_join(rsp_thread);

done:
    /* At this point, err is assumed to be non-positive */
    if (err != 0)
        exalog_error("Failed%s%s: %s (%d)", step != NULL ? " " : "",
                     step != NULL ? step : "", os_strerror(-err), err);

}


/*********************
 * Private Functions *
 *********************/

static void reject_t(TARGET_SESSION_T *sess, unsigned char *header,
                    unsigned char reason)
{
    ISCSI_REJECT_T reject;
    unsigned char rsp_header[ISCSI_HEADER_LEN];

    exalog_error("Reject iSCSI command with reason: %x", reason);
    reject.reason = reason;
    reject.length = ISCSI_HEADER_LEN;

    os_thread_mutex_lock(&sess->send_lock);

    session_newstat_sn(sess,
                       &reject.StatSN,
                       &reject.MaxCmdSN,
                       &reject.ExpCmdSN);
    reject.DataSN = 0;          /* SNACK not yet implemented */

    iscsi_reject_encap(rsp_header, &reject);

    session_sock_send_header_and_data(sess,
                                      rsp_header,
                                      header,
                                      ISCSI_HEADER_LEN);

    os_thread_mutex_unlock(&sess->send_lock);
}

static int device_command_done(TARGET_CMD_T *cmd)
{
    TARGET_SESSION_T *sess = cmd->sess;
    ISCSI_SCSI_CMD_T *scsi_cmd = &(cmd->scsi_cmd);
    ISCSI_READ_DATA_T data;
    unsigned char rsp_header[ISCSI_HEADER_LEN];
    unsigned DataSN = 0;
    ISCSI_SCSI_RSP_T scsi_rsp;
    int rc = -1;
    if (scsi_cmd->status == SCSI_STATUS_TASK_ABORTED)
    {
        /* Since in control mode the TAS bit is set to 0 (spc3r23 7.4.6) we must not send
	 * the error to the initiator, so scsi_command.c calls this function only to free
	 * associated resources to the command.
         */
	cmd_put(cmd);
	return 0;
    }

    if (!scsi_cmd->status && scsi_cmd->fromdev && scsi_cmd->length)
    {
        unsigned offset;
        if ((scsi_cmd->fromdev) && (scsi_cmd->length > scsi_cmd->trans_len))
        {
            exalog_error("cmd length %d reset to trans_len %d",
                         scsi_cmd->length,
                         scsi_cmd->trans_len);
            scsi_cmd->trans_len = scsi_cmd->length;
        }

        for (offset = 0;
             offset < scsi_cmd->length;
             offset += sess->sess_params.max_data_seg_length)
        {
            memset(&data, 0, sizeof(ISCSI_READ_DATA_T));
            if (sess->sess_params.max_data_seg_length)
                data.length = MIN(scsi_cmd->length - offset,
                                  sess->sess_params.max_data_seg_length);
            else
                data.length = scsi_cmd->length - offset;

            os_thread_mutex_lock(&sess->send_lock);

            if (offset + data.length == scsi_cmd->length)
            {
                data.final = 1;
                data.S_bit = 1;
                data.status = scsi_cmd->status;
                session_newstat_sn(sess,
                                   &data.StatSN,
                                   &data.MaxCmdSN,
                                   &data.ExpCmdSN);

                if (scsi_cmd->length < scsi_cmd->trans_len)
                {
                    data.res_count =  scsi_cmd->trans_len - scsi_cmd->length;
                    data.underflow = 1;
                }
            }
            else
            {
                session_stat_sn(sess, NULL, &data.MaxCmdSN, &data.ExpCmdSN);
                data.StatSN = 0; /* rfc3720 1.7.3 StatSN ... meaningfull if S [status] is set */
            }

            data.task_tag = scsi_cmd->tag;
            data.DataSN = DataSN++;
            data.offset = offset;
            iscsi_read_data_encap(rsp_header, &data);

            session_sock_send_header_and_data(
                sess,
                rsp_header,
                (char *)cmd->data + offset,
                data.length);

            os_thread_mutex_unlock(&sess->send_lock);

            scsi_cmd->bytes_fromdev += data.length;
        }
    }
    /*
     * Send a response PDU if
     *
     * 1) we're not using phase collapsed input (and status was good)
     * 2) we are using phase collapsed input, but there was no input data (e.g., TEST UNIT READY)
     * 3) command had non-zero status and possible sense data
     */

    if ((!scsi_cmd->length) || (scsi_cmd->status))
    {
        memset(&scsi_rsp, 0, sizeof(ISCSI_SCSI_RSP_T));
        scsi_rsp.length = scsi_cmd->status ? scsi_cmd->length : 0;
        scsi_rsp.tag = scsi_cmd->tag;

        os_thread_mutex_lock(&sess->send_lock);

        session_newstat_sn(sess,
                           &scsi_rsp.StatSN,
                           &scsi_rsp.MaxCmdSN,
                           &scsi_rsp.ExpCmdSN);
        scsi_rsp.ExpDataSN = (!scsi_cmd->status
                              && scsi_cmd->fromdev) ? DataSN : 0;
        scsi_rsp.response = 0x00;       /* iSCSI response */
        scsi_rsp.status = scsi_cmd->status;     /* SCSI status */

        if (scsi_cmd->status != 0) /* sense data */
        {
            unsigned char sense_data[cmd->data_len + 2];
            memcpy(sense_data + 2, cmd->data, cmd->data_len);
            set_bigendian16(cmd->data_len, sense_data);
            scsi_rsp.length = cmd->data_len + 2;
            iscsi_scsi_rsp_encap(&rsp_header[0], &scsi_rsp);
            session_sock_send_header_and_data(sess,
                                              rsp_header,
                                              sense_data,
                                              scsi_rsp.length);
        }
        else
        {
            iscsi_scsi_rsp_encap(&rsp_header[0], &scsi_rsp);
            session_sock_send_header_and_data(sess,
                                              rsp_header,
                                              cmd->data,
                                              scsi_rsp.length);
        }

        os_thread_mutex_unlock(&sess->send_lock);

        /* Make sure all data was transferred */
        if ((scsi_cmd->todev) && (scsi_cmd->status == 0))
        {
            if (scsi_cmd->bytes_todev != scsi_cmd->trans_len)
                goto error;

            if ((scsi_cmd->fromdev)
                && (scsi_cmd->bytes_fromdev != scsi_cmd->bidi_trans_len))
                goto error;
        }
        else
        {
            if ((scsi_cmd->fromdev) && (scsi_cmd->status == 0)
                && (scsi_cmd->bytes_fromdev != scsi_cmd->length))
                goto error;
        }
    }

    rc = 0;
    goto noerror;

error:
    rc = -1;
noerror:
    /* free up target command structure */
    cmd_put(cmd);
    return rc;
}


static void cmd_rsp_worker(void *arg)
{
#define RSP_WORKER_SELECT_TIMEOUT 200
    exalog_as(EXAMSG_ISCSI_ID);

    while (target_run)
    {
        struct nbd_list *in[1];
        bool out[1];

        in[0] = &g_cmd_defered_send;

        if (nbd_list_select(in, out, 1, RSP_WORKER_SELECT_TIMEOUT) == 1)
        {
            TARGET_CMD_T *cmd = nbd_list_remove(&g_cmd_defered_send, NULL, LISTNOWAIT);

            EXA_ASSERT(cmd != NULL);

            scsi_command_submit(cmd);
        }
    }
}


static int scsi_command_t(TARGET_SESSION_T *sess, unsigned char *header)
{
    TARGET_CMD_T *cmd;
    ISCSI_SCSI_CMD_T *scsi_cmd = NULL;
    int rc;

    cmd = cmd_get_and_link(sess);

    cmd->callback = NULL;
    cmd->callback_arg = NULL;
    scsi_cmd = &cmd->scsi_cmd;
    memset(scsi_cmd, 0, sizeof(ISCSI_SCSI_CMD_T));
    if (iscsi_scsi_cmd_decap(header, scsi_cmd) != 0)
    {
        exalog_error("iscsi_scsi_cmd_decap() failed");
        cmd_put(cmd);
        return -1;
    }

    /* FIXME: Why this unpacking operation is not done in iscsi_scsi_cmd_decap ? */
    scsi_cmd->tag = get_bigendian32(header + 16);

    if (cmd->scsi_cmd.trans_len > sess->sess_params.max_burst_length)
    {
        /* FIXME this test is legacy and seems wrong, investigate either to fix
         * or remove */
        exalog_warning("iscsi: disconnect initiator. (initiators trans_len (%d)"
                " > negociated MaxBurstLength (%d))",
                cmd->scsi_cmd.trans_len,
                sess->sess_params.max_burst_length);
    }

    cmd->data_len = scsi_cmd->trans_len;

    session_newcmd_sn(sess, scsi_cmd->CmdSN, scsi_cmd->ExpStatSN);

    if (sess->sess_params.first_burst_length
        && (scsi_cmd->length > sess->sess_params.first_burst_length))
    {
        exalog_error("scsi_cmd->length (%u) > FirstBurstLength (%u)",
                     scsi_cmd->length, sess->sess_params.first_burst_length);
        cmd_put(cmd);
        return -1;
    }
    if (sess->sess_params.max_data_seg_length
        && (scsi_cmd->length > sess->sess_params.max_data_seg_length))
    {
        exalog_error("scsi_cmd.length (%u) > MaxRecvDataSegmentLength (%u)",
                     scsi_cmd->length, sess->sess_params.max_data_seg_length);
        cmd_put(cmd);
        return -1;
    }

    /* Read AHS.  Need to optimize/clean this. */
    /* We need to check for properly formated AHS segments. */

    if (scsi_cmd->ahs_len)
    {
        unsigned ahs_len;
        unsigned ahs_offset = 0;
        unsigned char ahs_type;

        if (scsi_cmd->ahs_len > CONFIG_ISCSI_MAX_AHS_LEN)
        {
            exalog_error
		(
		    "scsi_cmd->ahs_len (%u) > CONFIG_ISCSI_MAX_AHS_LEN (%u)",
		    scsi_cmd->ahs_len,
		    CONFIG_ISCSI_MAX_AHS_LEN);
            cmd_put(cmd);
            return -1;
        }

        if (iscsi_sock_msg(sess->sock, SOCKET_RECV, scsi_cmd->ahs_len, scsi_cmd->ahs)
            != scsi_cmd->ahs_len)
        {
            exalog_error("iscsi: connection broken");
            cmd_put(cmd);
            return -1;
        }
        /* two pass with ahs : first pass all ahs */
        ahs_offset = 0;
        while (ahs_offset < scsi_cmd->ahs_len - 2)
        {
            ahs_len = get_bigendian16(scsi_cmd->ahs + ahs_offset);
            ahs_type = *(((unsigned char *)scsi_cmd->ahs) + 2 + ahs_offset);
            if (ahs_offset + ahs_len  >= scsi_cmd->ahs_len)
            {
                exalog_error("iscsi: ahs incorect");
                cmd_put(cmd);
                return -1;
            }
            if (ahs_type == ISCSI_AHS_BIDI_READ_DATA_LENGTH)
                scsi_cmd->bidi_trans_len = get_bigendian32(
                    scsi_cmd->ahs + ahs_offset + 4);

            ahs_offset += 3 + ahs_len;
            if ((ahs_offset & 3) != 0)
                ahs_offset += 4 - (ahs_offset & 3);
        }
        /* second pass only extended  cdb (>16 bytes)*/
        ahs_offset = 0;
        while (ahs_offset < scsi_cmd->ahs_len - 2)
        {
            ahs_len = get_bigendian16(scsi_cmd->ahs + ahs_offset);
            ahs_type = *(((unsigned char *)scsi_cmd->ahs) + 2 + ahs_offset);
            if (ahs_offset + ahs_len  >= scsi_cmd->ahs_len)
            {
                exalog_error("iscsi: ahs incorrect");
                cmd_put(cmd);
                return -1;
            }
            if (ahs_type == ISCSI_AHS_EXTENDED_CDB)
            {
                memmove(scsi_cmd->ahs + 16,
                        scsi_cmd->ahs + ahs_offset + 4,
                        ahs_len - 1);
                memcpy(scsi_cmd->ahs, scsi_cmd->cdb, 16);
                break;
            }
            ahs_offset += 3 + ahs_len;
            if ((ahs_offset & 3) != 0)
                ahs_offset += 4 - (ahs_offset & 3);
        }
    }

    /*
     * Execute cdb.  device_command() will set scsi_cmd.input if there is input
     * data and set the length of the input to either scsi_cmd.trans_len or
     * scsi_cmd.bidi_trans_len, depending on whether scsi_cmd.output was set.
     *
     * When the device has finished executing the CDB (i.e., status has been
     * set), it will call the device_command_done() callback, instructing
     * this target code to send the response PDU.
     */

    scsi_cmd->fromdev = 0;
    cmd->callback = (int (*)(void*))device_command_done;
    cmd->callback_arg = cmd;

    /* clear out any immediate data */
    if ((!sess->sess_params.immediate_data) && scsi_cmd->length)
    {
        exalog_error("Cannot accept any Immediate data");
        cmd_put(cmd);
        return -1;
    }
    if (sess->sess_params.immediate_data && scsi_cmd->length)
    {
        if (sess->sess_params.max_data_seg_length)
        {
            /* FIXME: Why this unpacking operation is not done in iscsi_scsi_cmd_decap ? */
            uint32_t length = get_bigendian32(header + 4) & 0x00ffffff;

            if (length > sess->sess_params.max_data_seg_length)
            {
                exalog_error("Bad header length: %u > %u.", length,
                             sess->sess_params.max_data_seg_length);
                return -1;
            }
        }
        rc = iscsi_sock_msg(sess->sock, SOCKET_RECV, scsi_cmd->length, cmd->data);
        if (rc != scsi_cmd->length)
        {
            exalog_error("iscsi: connection broken");
            /* free up target command structure */
            cmd_put(cmd);
            return -1;
        }
        cmd->scsi_cmd.bytes_todev = rc;
    }

    if ((scsi_cmd->todev == 0) ||
        (scsi_cmd->trans_len - scsi_cmd->bytes_todev == 0))
    {
         /* if its a write, only start the command if all data is already present */
        nbd_list_post(&g_cmd_defered_send, cmd, -1);
    }
    else
    {
         /* ready to continue transfert */
        target_transfer_data(cmd);
    }

    return 0;
}


static int task_command_t(TARGET_SESSION_T *sess, unsigned char *header)
{
    ISCSI_TASK_CMD_T cmd;
    ISCSI_TASK_RSP_T rsp;
    unsigned char rsp_header[ISCSI_HEADER_LEN];

    /* Get & check args */

    if (iscsi_task_cmd_decap(header, &cmd) != 0)
    {
        exalog_error("iscsi_task_cmd_decap() failed");
        return -1;
    }


    memset(&rsp, 0, sizeof(ISCSI_TASK_RSP_T));
    rsp.response = ISCSI_TASK_RSP_FUNCTION_COMPLETE;

    switch (cmd.function)
    {
    case (ISCSI_TASK_CMD_ABORT_TASK):
        exalog_info("Initiator sent ABORT TASK lun %" PRIlun
		    " tag 0x%X ref tag 0x%X nexus %d",
		    cmd.lun, cmd.tag, cmd.ref_tag, sess->id);
	sess_cmd_display(sess);
	/* see rfc3720 10.6.1 ABORT TASK a) b) c) */

	if (!cmd_wait_tag(sess, cmd.ref_tag))
	{
            os_thread_mutex_lock(&sess->slock);
	    if (sess->ExpCmdSN >= cmd.RefCmdSN)
                rsp.response = ISCSI_TASK_RSP_NO_SUCH_TASK;
	    os_thread_mutex_unlock(&sess->slock);
	}
	exalog_debug("ABORT TASK success lun %" PRIlun " tag %d "
		     "ref tag %d nexus %d",
		     cmd.lun, cmd.tag, cmd.ref_tag, sess->id);
        break;

    case (ISCSI_TASK_CMD_ABORT_TASK_SET):
        exalog_info("Initiator sent ABORT TASK SET lun %" PRIlun " nexus %d",
		    cmd.lun, sess->id);
	rsp.response = ISCSI_TASK_RSP_NO_SUPPORT;
        break;

    case (ISCSI_TASK_CMD_CLEAR_ACA):
        exalog_info("Initiator sent CLEAR ACA lun %" PRIlun " nexus %d",
		    cmd.lun, sess->id);
        rsp.response = ISCSI_TASK_RSP_NO_SUPPORT;
        break;

    case (ISCSI_TASK_CMD_CLEAR_TASK_SET):
        exalog_info("Initiator sent CLEAR TASK SET lun %" PRIlun " nexus %d",
		    cmd.lun, sess->id);
        break;

    case (ISCSI_TASK_CMD_LOGICAL_UNIT_RESET):
        exalog_info("Initiator sent LOGICAL UNIT RESET lun %" PRIlun " nexus %d",
		    cmd.lun, sess->id);
	sess_cmd_display(sess);
        scsi_logical_unit_reset(cmd.lun);
	exalog_debug("LOGICAL UNIT RESET success lun %" PRIlun " nexus %d",
		     cmd.lun, sess->id);

        break;

    case (ISCSI_TASK_CMD_TARGET_WARM_RESET):
        exalog_info("Initiator sent TARGET WARM RESET nexus %d", sess->id);
        scsi_logical_unit_reset(RESET_ALL_LUNS);
	exalog_debug("TARGET WARM RESET success nexus %d", sess->id);
        break;

    case (ISCSI_TASK_CMD_TARGET_COLD_RESET):
        exalog_info("Initiator sent TARGET COLD RESET nexus %d", sess->id);
        scsi_logical_unit_reset(RESET_ALL_LUNS);
        exalog_debug("TARGET COLD RESET success nexus %d", sess->id);
	/* FIXME :we must probably also clear all persistent reservation */
        break;

    case (ISCSI_TASK_CMD_TARGET_REASSIGN):
        exalog_info("Initiator sent TARGET REASSIGN  lun %" PRIlun
		    " tag %d nexus %d",
		    cmd.lun, cmd.tag, sess->id);
	rsp.response = ISCSI_TASK_RSP_NO_SUPPORT;
        break;

    default:
        exalog_error("Initiator sent unknown task function %i (0x%X), SN=%d nexus %d\n",
		     cmd.function,
		     cmd.function,
		     cmd.CmdSN,
		     sess->id);
        rsp.response = ISCSI_TASK_RSP_NO_SUPPORT;
    }

    /* rfc3720 3.2.2.1.
     * Commands meant for immediate delivery are marked with an immediate
     *  delivery flag; they MUST also carry the current CmdSN.  CmdSN does
     * not advance after a command marked for immediate delivery is sent.
     * TODO : respect immediate for all command */
    if (cmd.immediate)
        session_cmd_sn(sess, cmd.CmdSN, cmd.ExpStatSN);
    else
        session_newcmd_sn(sess, cmd.CmdSN, cmd.ExpStatSN);

    rsp.tag = cmd.tag;


    os_thread_mutex_lock(&sess->send_lock);

    if (cmd.immediate)
	session_stat_sn(sess, &rsp.StatSN, &rsp.MaxCmdSN, &rsp.ExpCmdSN);
    else
	session_newstat_sn(sess, &rsp.StatSN, &rsp.MaxCmdSN, &rsp.ExpCmdSN);
    iscsi_task_rsp_encap(rsp_header, &rsp);
    session_sock_send_header_and_data(sess, rsp_header, NULL, 0);

    os_thread_mutex_unlock(&sess->send_lock);

    return 0;
}


static int async_event(int session_id, lun_t lun, void *sense_data, size_t len)
{
    ISCSI_ASYNCHRONOUS_MESSAGE rsp;
    unsigned char rsp_header[ISCSI_HEADER_LEN];
    TARGET_SESSION_T *session;

    session = nbd_get_elt_by_num(session_id, &g_session_q);
    if ((session) && (session->IsLoggedIn == 1))
    {
        unsigned char sense[len + 2];
        memset(&rsp, 0, sizeof(ISCSI_ASYNCHRONOUS_MESSAGE));
        rsp.lun = lun;
        rsp.AsyncEvent = 0; /* scsi event */
        rsp.AHSlength = 0;
        rsp.length = len + 2 /* sense data size */;

        os_thread_mutex_lock(&session->send_lock);

        session_newstat_sn(session, &rsp.StatSN, &rsp.MaxCmdSN, &rsp.ExpCmdSN);
        iscsi_async_event_encap(rsp_header, &rsp);
        set_bigendian16(len, sense);
	memcpy(sense + 2, sense_data, len);
        session_sock_send_header_and_data(session,
                                          rsp_header,
                                          sense,
                                          2 + len);

        os_thread_mutex_unlock(&session->send_lock);
    }
    return 0;
}


int target_reset_bus(void)
{
    ISCSI_ASYNCHRONOUS_MESSAGE rsp;
    long index;
    unsigned char rsp_header[ISCSI_HEADER_LEN];
    TARGET_SESSION_T *session;

    for (index = 0; index < CONFIG_TARGET_MAX_SESSIONS; index++)
    {
        session = nbd_get_elt_by_num(index, &g_session_q);
        if ((session) && (session->IsLoggedIn == 1))
        {
            memset(&rsp, 0, sizeof(ISCSI_ASYNCHRONOUS_MESSAGE));
            rsp.lun = 0xffffffff;
            rsp.Parameter3 = 2;
            rsp.AsyncEvent = 1; /* iscsi event */
            rsp.AHSlength = 0;
            rsp.length = 0;

            os_thread_mutex_lock(&session->send_lock);

            session_newstat_sn(session,
                               &rsp.StatSN,
                               &rsp.MaxCmdSN,
                               &rsp.ExpCmdSN);
            iscsi_async_event_encap(rsp_header, &rsp);
            session_sock_send_header_and_data(session, rsp_header, NULL, 0);

            os_thread_mutex_unlock(&session->send_lock);
        }
    }
    return 0;
}


static int nop_out_t(TARGET_SESSION_T *sess, unsigned char *header)
{
    ISCSI_NOP_OUT_T nop_out;
    char *ping_data = NULL;

    if (iscsi_nop_out_decap(header, &nop_out) != 0)
    {
        exalog_error("iscsi_nop_out_decap() failed");
        return -1;
    }

    if (nop_out.length)
    {
        ping_data = os_malloc(nop_out.length);
        iscsi_sock_msg(sess->sock, SOCKET_RECV, nop_out.length, ping_data);
    }

    if (nop_out.tag != 0xffffffff)
    {
        ISCSI_NOP_IN_T nop_in;
        unsigned char rsp_header[ISCSI_HEADER_LEN];
        session_newcmd_sn(sess, nop_out.CmdSN, nop_out.ExpStatSN);

        /* build response to initiator */
        memset(&nop_in, 0, sizeof(ISCSI_NOP_IN_T));
        nop_in.length = nop_out.length;
        nop_in.lun = nop_out.lun;
        nop_in.tag = nop_out.tag;
        nop_in.transfer_tag = 0xffffffff;

        os_thread_mutex_lock(&sess->send_lock);

        session_newstat_sn(sess,
                           &nop_in.StatSN,
                           &nop_in.MaxCmdSN,
                           &nop_in.ExpCmdSN);
        iscsi_nop_in_encap(rsp_header, &nop_in);
        session_sock_send_header_and_data(sess,
                                          rsp_header,
                                          ping_data,
                                          nop_in.length);

        os_thread_mutex_unlock(&sess->send_lock);
    }

    if (ping_data)
        os_free(ping_data);

    return 0;
}


/*
 * text_command_t
 */

static int text_command_t(TARGET_SESSION_T *sess, unsigned char *header)
{
    ISCSI_TEXT_CMD_T text_cmd;
    ISCSI_TEXT_RSP_T text_rsp;
    unsigned char rsp_header[ISCSI_HEADER_LEN];
    char *text_in = NULL;
    char *text_out = NULL;
    int len_in;
    int len_out = 0;

    /* Get text args */
    if (iscsi_text_cmd_decap(header, &text_cmd) != 0)
    {
        exalog_error("iscsi_text_cmd_decap() failed");
        return -1;
    }

    session_newcmd_sn(sess, text_cmd.CmdSN, text_cmd.ExpStatSN);

    if ((text_out = os_malloc(ISCSI_PARAM_MAX_TEXT_LEN)) == NULL)
    {
        exalog_error("os_malloc() failed");
        return -1;
    }
    /* Read text parameters */

    len_in = text_cmd.length;
    if (len_in)
    {
        iscsi_parameter_t *ptr;

        if ((text_in = os_malloc(len_in + 1)) == NULL)
        {
            exalog_error("os_malloc() failed");
            os_free(text_out);
            return -1;
        }

        if (iscsi_sock_msg(sess->sock, SOCKET_RECV, len_in, text_in) != len_in)
        {
            exalog_error("iscsi_sock_msg() failed");
            os_free(text_out);
            os_free(text_in);
            return -1;
        }
        text_in[len_in] = '\0';

        /* parse the incoming parameters */
        if (param_text_parse(sess->params, text_in, len_in, text_out,
                             &len_out, false) != 0)
        {
            os_free(text_in);
            os_free(text_out);
            return -1;
        }

        /* Handle exceptional cases not covered by parameters.c (e.g., SendTargets) */
        if ((ptr = param_list_elt(sess->params, "SendTargets")) == NULL)
        {
            exalog_error("iSCSI parameter 'SendTargets' not found");
            os_free(text_out);
            os_free(text_in);
            return -1;
        }
        /* FIXME I think this should be moved to the negociation module */
        if (ptr->is_rx_offer)
        {
            if (ptr->offer_rx && !strcmp(ptr->offer_rx, "All")
                && !param_list_value_is_equal(sess->params, "SessionType", "Discovery"))
            {
                if (param_text_add("SendTargets", "Reject",
                                   text_out, &len_out, 0) != 0)
                {
                    os_free(text_in);
                    os_free(text_out);
                    return -1;
                }
            }
            else
            {
                int i;

                if (param_text_add("TargetName", target_name(),
                                   text_out, &len_out, 0) != 0)
                {
                    os_free(text_in);
                    os_free(text_out);
                    return -1;
                }
                for (i = 0; i < num_cluster_listen_addresses; i++)
                {
                    if (param_text_add("TargetAddress",
                                      cluster_listen_addresses[i], text_out,
                                      &len_out, 0) != 0)
                    {
                        os_free(text_in);
                        os_free(text_out);
                        return -1;
                    }
                }
            }
            ptr->is_rx_offer = false;
        }

        /* FIXME WTF? We're parsing OUR OWN offer??? oO */
        /* Parse outgoing offer
         * It seems that this code only check the answer before sending it
         */
        if (len_out)
        {
            /* FIXME the variable 'text_out' id passed as argument 'text_in'
             * and the argument 'text_in' is actually NULL
             */
            if (param_text_parse(sess->params, text_out, len_out, NULL, NULL,
                                 true) != 0)
            {
                os_free(text_in);
                os_free(text_out);
                return -1;
            }
        }
    }

    if (sess->IsFullFeature)
        set_session_parameters(sess->params, &sess->sess_params);

    /* Send response */

    text_rsp.final = text_cmd.final;
    text_rsp.cont = 0;
    text_rsp.length = len_out;
    text_rsp.lun = text_cmd.lun;
    text_rsp.tag = text_cmd.tag;
    if (text_rsp.final)
        text_rsp.transfer_tag = 0xffffffff;
    else
        text_rsp.transfer_tag = 0x1234;

    os_thread_mutex_lock(&sess->send_lock);

    session_newstat_sn(sess,
                       &text_rsp.StatSN,
                       &text_rsp.MaxCmdSN,
                       &text_rsp.ExpCmdSN);
    iscsi_text_rsp_encap(rsp_header, &text_rsp);

    if (len_out == 0)
        session_sock_send_header_and_data(sess, rsp_header, NULL, 0);
    else
        session_sock_send_header_and_data(sess, rsp_header, text_out, len_out);

    os_thread_mutex_unlock(&sess->send_lock);

    os_free(text_in);
    os_free(text_out);
    return 0;
}


/*
 * login_command_t() handles login requests and replies.
 */
static int login_command_t(TARGET_SESSION_T *sess, unsigned char *header)
{
    ISCSI_LOGIN_CMD_T cmd;
    ISCSI_LOGIN_RSP_T rsp;
    unsigned char rsp_header[ISCSI_HEADER_LEN];
    char *text_in = NULL;
    char *text_out = NULL;
    int len_in = 0;
    int len_out = 0;
    int status = 0;
    char TargetName[256];
    char InitiatorName[256];

    /* Initialize response */
    memset(&rsp, 0, sizeof(ISCSI_LOGIN_RSP_T));
    rsp.status_class = ISCSI_LOGIN_STATUS_INITIATOR_ERROR;

    /* Get login args & check preconditions */
    if (iscsi_login_cmd_decap(header, &cmd) != 0)
    {
        exalog_error("iscsi_login_cmd_decap() failed");
        goto response;
    }

    if ((cmd.cont != 0) && (cmd.transit != 0))
    {
        exalog_error("Bad cmd.continue.  Expected 0.");
        goto response;
    }
    else if ((cmd.version_max < ISCSI_VERSION)
             || (cmd.version_min > ISCSI_VERSION))
    {
        exalog_error
	    (
		"Target iscsi version (%u) not supported by initiator [Max Ver (%u) and Min Ver (%u)]",
		ISCSI_VERSION,
		cmd.version_max,
		cmd.version_min);
        rsp.status_class = ISCSI_LOGIN_STATUS_INITIATOR_ERROR;
        rsp.status_detail = 0x05;       /* Version not supported */
        rsp.version_max = ISCSI_VERSION;
        rsp.version_active = ISCSI_VERSION;
        goto response;
    }
    else if (cmd.tsih != 0)
    {
        exalog_error("Bad cmd.tsih (%u). Expected 0.", cmd.tsih);
        goto response;
    }

    /* Parse text parameters and build response */
    if ((text_out = os_malloc(ISCSI_PARAM_MAX_TEXT_LEN)) == NULL)
    {
        exalog_error("os_malloc() failed");
        return -1;
    }
    if ((len_in = cmd.length))
    {
        if ((text_in = os_malloc(len_in + 1)) == NULL)
        {
            exalog_error("os_malloc() failed");
            os_free(text_out);
            return -1;
        }
        if (iscsi_sock_msg(sess->sock, SOCKET_RECV, len_in, text_in) != len_in)
        {
            exalog_error("Could not read on iSCSI socket %d", sess->sock);
            os_free(text_in);
            os_free(text_out);
            return -1;
        }
        text_in[len_in] = '\0';

        /* Parse incoming parameters (text_out will contain the response we need
         * to send back to the initiator */
        if ((status =
	     param_text_parse(sess->params, text_in, len_in, text_out,
			      &len_out, false)) != 0)
        {
            switch (status)
            {
            case ISCSI_PARAM_STATUS_FAILED:
                exalog_error("*** ISCSI_PARAM_STATUS_FAILED ***");
                rsp.status_detail = 0;
                break;

            default:
                /*
                 * Need to set the detail field (status_detail field)
                 */
                exalog_trace("status = %i", status);
                break;
            }
            goto response;
        }

        /* Declares the TPGT to the initiator.
         * This key must be sent in the first Login Response PDU of a Normal session.
         */
        if (!sess->LoginStarted)
        {
            if (param_text_add("TargetPortalGroupTag", TARGET_PORTAL_GROUP_TAG,
                               text_out, &len_out, 0) != 0)
            {
                os_free(text_in);
                os_free(text_out);
                return -1;
            }
        }

        /* Parse the outgoing offer */
        /* It seems that this code only check the answer before sending it */
        if (len_out)
        {
            /* FIXME the variable 'text_out' id passed as argument 'text_in'
             * and the argument 'text_in' is actually NULL
             */
            if (param_text_parse(sess->params, text_out, len_out, NULL, NULL,
                                 true) != 0)
            {
                os_free(text_in);
                os_free(text_out);
                return -1;
            }
        }
    }

    if (!sess->LoginStarted)
        sess->LoginStarted = 1;

    /* For now, we accept whatever the initiator's current and next states are, and we are always */
    /* ready to transition to that state. */

    rsp.csg = cmd.csg;
    rsp.nsg = cmd.nsg;
    rsp.transit = cmd.transit;

    if (cmd.transit && (cmd.nsg == ISCSI_LOGIN_STAGE_FULL_FEATURE))
    {
        /* Check post conditions */

        if (param_list_value_is_equal(sess->params, "InitiatorName", ""))
        {
            exalog_error("InitiatorName not specified");
            goto response;
        }
        if (param_list_value_is_equal(sess->params, "SessionType", "Normal"))
        {
            if (param_list_value_is_equal(sess->params, "TargetName", ""))
            {
                exalog_error("TargetName not specified");
                goto response;
            }
            else if (!param_list_value_is_equal(sess->params, "TargetName", target_name()))
            {
                exalog_error("Bad TargetName \"%s\" (expected \"%s\")",
                             param_list_get_value(sess->params, "TargetName"),
                             target_name());
                goto response;
            }
        }
        if (param_list_value_is_equal(sess->params, "SessionType", ""))
        {
            exalog_error("SessionType not specified");
            goto response;
        }

        sess->cid = cmd.cid;
        sess->isid = cmd.isid;
        sess->tsih = sess->id + 1;
        sess->IsFullFeature = 1;

        sess->IsLoggedIn = 1;
        if (param_list_value_is_equal(sess->params, "SessionType", "Discovery"))
            param_list_set_value(sess->params, "MaxConnections", "1");

        /* initialize the LUN access authorization corresponding to the
         * initiator
         */
        session_update_authorized_luns(sess);

        set_session_parameters(sess->params, &sess->sess_params);
    }

    /* No errors */

    rsp.status_class = rsp.status_detail = 0;
    rsp.length = len_out;

    /* Send login response */

response:
    /* FIXME session_cmd_sn call here is an obfuscated initialization of fields
     * ExpStatSN ExpCmdSN and MaxCmdSN. The RFC indicates that during login
     * phase ExpCmdSN MUST be initialized from CmdSN. Here it is done as a side
     * effect of the fucntion; it should be done explicitly here. */
    session_cmd_sn(sess, cmd.CmdSN, cmd.ExpStatSN);
    rsp.isid = cmd.isid;
    rsp.StatSN = cmd.ExpStatSN; /* debug */
    rsp.tag = cmd.tag;
    rsp.cont = cmd.cont;

    os_thread_mutex_lock(&sess->send_lock);

    session_newstat_sn(sess, &rsp.StatSN, &rsp.MaxCmdSN, &rsp.ExpCmdSN);
    if (!rsp.status_class)
    {
        if (rsp.transit && (rsp.nsg == ISCSI_LOGIN_STAGE_FULL_FEATURE))
        {
            rsp.version_max = ISCSI_VERSION;
            rsp.version_active = ISCSI_VERSION;
            rsp.tsih = sess->tsih;
        }
    }

    iscsi_login_rsp_encap(rsp_header, &rsp);

    session_sock_send_header_and_data(sess, rsp_header, text_out, rsp.length);

    os_thread_mutex_unlock(&sess->send_lock);

    if (rsp.status_class != 0)
    {
        exalog_error("bad status class 0x%x, expected 0x0", rsp.status_class);
        os_free(text_in);
        os_free(text_out);
        return -1;
    }

    if (cmd.transit && (cmd.nsg == ISCSI_LOGIN_STAGE_FULL_FEATURE))
    {
        /* just to keep the output tidy */
        /* FIXME os_strlcpy, FIXME 50 */

        strncpy(InitiatorName, param_list_get_value(sess->params, "InitiatorName"),
                50);
        InitiatorName[50] = '\0';
        strncpy(TargetName, target_name(), 50);
        TargetName[50] = '\0';

        exalog_trace("iscsi: LOGIN SUCCESSFUL\n * %25s:%50s *",
                     "InitiatorName", InitiatorName);
        if (strlen(param_list_get_value(sess->params, "InitiatorName")) > 50)
            exalog_trace("* %25s:%50s *", "InitiatorName (cont)",
                         param_list_get_value(sess->params, "InitiatorName") + 50);

        exalog_trace("* %25s:%50s *", "TargetName", TargetName);
        if (strlen(target_name()) > 50)
            exalog_trace("* %25s:%50s *", "TargetName (cont)",
                         param_list_get_value(sess->params, "TargetName") + 50);

        exalog_trace("* %25s:%50s *", "Type",
                     param_list_value_is_equal(sess->params, "SessionType",
                                               "Discovery") ? "Discovery" : "Normal");
        exalog_trace("* %25s:%50s *", "AuthMethod",
                     param_list_get_value(sess->params, "AuthMethod"));
        if (param_list_value_is_equal(sess->params, "AuthMethod", "CHAP"))
            exalog_trace("* %25s:%50s *", "InitiatorUser",
                         CONFIG_TARGET_USER_IN);

        exalog_trace("* %25s:%50llu *", "ISID", sess->isid);
        exalog_trace("* %25s:%50u *", "TSIH", sess->tsih);
        exalog_trace("* %25s:%50s *", "InitialR2T",
                     param_list_get_value(sess->params, "InitialR2T"));
        exalog_trace("* %25s:%50s *", "ImmediateData",
                     param_list_get_value(sess->params, "ImmediateData"));
        exalog_trace("* %25s:%50s *", "MaxRecvDataSegmentLength",
                     param_list_get_value(sess->params, "MaxRecvDataSegmentLength"));
        exalog_trace("* %25s:%50s *", "FirstBurstLength",
                     param_list_get_value(sess->params, "FirstBurstLength"));
        exalog_trace("* %25s:%50s *", "MaxBurstLength",
                     param_list_get_value(sess->params, "MaxBurstLength"));
    }

    os_free(text_in);
    os_free(text_out);

    return 0;
}


static int logout_command_t(TARGET_SESSION_T *sess, unsigned char *header)
{
    ISCSI_LOGOUT_CMD_T cmd;
    ISCSI_LOGOUT_RSP_T rsp;
    unsigned char rsp_header[ISCSI_HEADER_LEN];

    memset(&rsp, 0, sizeof(ISCSI_LOGOUT_RSP_T));
    if (iscsi_logout_cmd_decap(header, &cmd) != 0)
    {
        exalog_error("iscsi_logout_cmd_decap() failed");
        return -1;
    }
    session_newcmd_sn(sess, cmd.CmdSN, cmd.ExpStatSN);

    if ((cmd.reason == ISCSI_LOGOUT_CLOSE_RECOVERY)
        && param_list_value_is_equal(sess->params, "ErrorRecoveryLevel", "0"))
        rsp.response = ISCSI_LOGOUT_STATUS_NO_RECOVERY;

    rsp.tag = cmd.tag;

    os_thread_mutex_lock(&sess->send_lock);

    session_newstat_logout_sn(sess, &rsp.StatSN, &rsp.MaxCmdSN, &rsp.ExpCmdSN);
    iscsi_logout_rsp_encap(rsp_header, &rsp);

    session_sock_send_header_and_data(sess, rsp_header, NULL, 0);

    os_thread_mutex_unlock(&sess->send_lock);

    exalog_trace("sent logout response OK");
    exalog_trace("scsi: LOGOUT SUCCESSFUL ");
    exalog_trace("* %25s:%50s *", "Type",
                 param_list_value_is_equal(sess->params, "SessionType", "Discovery")
                 ? "Discovery" : "Normal");
    exalog_trace("* %25s:%50llu *", "ISID", sess->isid);
    exalog_trace("* %25s:%50u *", "TSIH", sess->tsih);

    sess->IsLoggedIn = 0;

    return 0;
}


static int verify_cmd_t(TARGET_SESSION_T *sess, unsigned char *header)
{
    int op = ISCSI_OPCODE(header);

    if ((!sess->LoginStarted) && (op != ISCSI_LOGIN_CMD))
    {
        /* Terminate the connection */
        exalog_error("session %i: iSCSI op 0x%x attempted before LOGIN PHASE",
                     sess->id, op);
        return -1;
    }

    if (!sess->IsFullFeature
        && ((op != ISCSI_LOGIN_CMD) && (op != ISCSI_LOGOUT_CMD)))
    {
        ISCSI_LOGIN_RSP_T rsp;
        unsigned char rsp_header[ISCSI_HEADER_LEN];

        exalog_error
	    (
		"session %i: iSCSI op 0x%x attempted before FULL FEATURE",
		sess->id,
		op);

        /* Create Login Reject response */
        memset(&rsp, 0, sizeof(ISCSI_LOGIN_RSP_T));

        os_thread_mutex_lock(&sess->send_lock);

        session_stat_sn(sess, NULL, NULL, NULL);
        rsp.status_class = ISCSI_LOGIN_STATUS_INITIATOR_ERROR;
        rsp.status_detail = 0x0b;
        rsp.version_max = ISCSI_VERSION;
        rsp.version_active = ISCSI_VERSION;
        iscsi_login_rsp_encap(rsp_header, &rsp);
        session_sock_send_header_and_data(sess, rsp_header, NULL, 0);

        os_thread_mutex_unlock(&sess->send_lock);

        return -1;
    }
    return 0;
}

static int send_r2t(TARGET_SESSION_T *sess, TARGET_CMD_T *cmd,
	            ISCSI_SCSI_CMD_T *args)
{
    ISCSI_R2T_T r2t;
    int desired_xfer_len;
    unsigned char header[ISCSI_HEADER_LEN];

    desired_xfer_len = args->trans_len - args->bytes_todev;

    if (desired_xfer_len > sess->sess_params.max_burst_length)
    {
        exalog_error("Bad xfer len: %u > %u.", desired_xfer_len,
                     sess->sess_params.max_burst_length);
        return -1;
    }

    memset(&r2t, 0, sizeof(ISCSI_R2T_T));

    r2t.tag          = args->tag;
    r2t.transfer_tag = 0x1234;

    os_thread_mutex_lock(&sess->send_lock);

    session_stat_sn(sess, &r2t.StatSN, &r2t.MaxCmdSN, &r2t.ExpCmdSN);

    r2t.length = desired_xfer_len;
    r2t.offset = args->bytes_todev;

    iscsi_r2t_encap(header, &r2t);

    session_sock_send_header_and_data(sess, header, NULL, 0);

    os_thread_mutex_unlock(&sess->send_lock);

    cmd->r2t_flag = 1;
    return 0;
}

static int iscsi_write_data(TARGET_SESSION_T *sess, unsigned char *header)
{
    ISCSI_WRITE_DATA_T data;
    TARGET_CMD_T *cmd;
    ISCSI_SCSI_CMD_T *args;
    int err = 0;

    err = iscsi_write_data_decap(header, &data);
    if (err != 0)
        return -1; /* FIXME shouldn't we return err ? */

    cmd = cmd_from_tag(sess, data.tag);
    if (cmd == NULL)
        return -1; /* FIXME shouldn't we return err ? */

    args = &(cmd->scsi_cmd);

    session_cmd_sn(sess, 0, data.ExpStatSN);

    /* check args */
    if (((sess->sess_params.max_data_seg_length) &&
         (data.length > sess->sess_params.max_data_seg_length)) ||
        ((args->bytes_todev + data.length) > args->trans_len) ||
        ((data.tag != args->tag) && (data.final)))
    {
        args->status = SCSI_STATUS_CHECK_CONDITION;
        return -1;
    }

    if (data.tag != args->tag && !data.final)
        reject_t(sess, header, ISCSI_REJECT_INVALID_PDU_FIELD);

    if (!(args->trans_len - (args->bytes_todev + data.length)))
    {
        if (!data.final)
        {
            exalog_trace("SORRY: bytes_todev = %u", args->bytes_todev);
            os_sleep(60); /* FIXME what is this supposed to do ?
                           * 60 is REALLY big... very stange... */
            exalog_error("Bad final bit");
            return -1;
        }
    }

    if ((cmd->data_len - (int) data.length - (int) data.offset < 0)
        ||  (iscsi_sock_msg(sess->sock, SOCKET_RECV, data.length,
                            (char *)cmd->data + data.offset)
	     != data.length))
    {
        cmd_put(cmd);
        return -1;
    }

    cmd->scsi_cmd.bytes_todev += data.length;
    if (!(cmd->scsi_cmd.trans_len - cmd->scsi_cmd.bytes_todev))
        nbd_list_post(&g_cmd_defered_send, cmd, -1);
    else if (!cmd->r2t_flag && (!sess->sess_params.initial_r2t &&
                               (sess->sess_params.first_burst_length
                                && args->bytes_todev >=
                                sess->sess_params.first_burst_length)))
    {
        /* If we're not in R2T mode and reach the first burst, then we still need
         * to send an R2T.  If we are in R2T mode, we would have already sent the
         * R2T for all the data in target_transfer_data */
	return send_r2t(sess, cmd, args);
    }

    return 0;
}


static int execute_t(TARGET_SESSION_T *sess, unsigned char *header)
{
    int op = ISCSI_OPCODE(header);
    int err = 0;

    if (verify_cmd_t(sess, header) != 0)
        return -1;

    switch (op)
    {
    case (ISCSI_TASK_CMD):
        err = task_command_t(sess, header);
        break;

    case (ISCSI_NOP_OUT):
        err = nop_out_t(sess, header);
        break;

    case (ISCSI_LOGIN_CMD):
        err = login_command_t(sess, header);
        break;

    case (ISCSI_TEXT_CMD):
        err = text_command_t(sess, header);
        break;

    case (ISCSI_LOGOUT_CMD):
        err = logout_command_t(sess, header);
        break;

    case (ISCSI_SCSI_CMD):
        err = scsi_command_t(sess, header);
        break;

    case (ISCSI_WRITE_DATA):
        err = iscsi_write_data(sess, header);
        break;

    default:
        exalog_error("Unknown Opcode 0x%x", ISCSI_OPCODE(header));
        reject_t(sess, header, ISCSI_REJECT_PROTOCOL_ERROR);
        break;
    }
    return err;
}

static void __init_target_session(TARGET_SESSION_T *s)
{
    iscsi_parameter_t **l = &s->params;
    char str_valid[ISCSI_PARAM_MAX_LEN];
    char str_dflt[ISCSI_PARAM_MAX_LEN];

    scsi_new_session(s->id);

    s->params = NULL;

    /*
     * ISCSI_PARAM_TYPE_LIST format:        <type> <key> <dflt> <valid list values>
     * ISCSI_PARAM_TYPE_BINARY format:      <type> <key> <dflt> <valid binary values>
     * ISCSI_PARAM_TYPE_NUMERICAL format:   <type> <key> <dflt> <max>
     * ISCSI_PARAM_TYPE_DECLARATIVE format: <type> <key> <dflt> ""
     */

    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_LIST,        "AuthMethod",           "None", "None"      ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_DECLARATIVE, "TargetPortalGroupTag", TARGET_PORTAL_GROUP_TAG, TARGET_PORTAL_GROUP_TAG) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_LIST,        "HeaderDigest",         "None", "None"      ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_LIST,        "DataDigest",           "None", "None"      ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL,   "MaxConnections",       "1",    "1"         ) == 0);
    /* FIXME I think SendTargets has *nothing* to do here. It seems it's
             here just as a lazy means to store information pertaining to the
             negociation protocol. */
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_DECLARATIVE, "SendTargets",          "",     ""          ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_DECLARATIVE, "TargetName",           "",     ""          ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_DECLARATIVE, "InitiatorName",        "",     ""          ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_DECLARATIVE, "TargetAlias",          "",     ""          ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_DECLARATIVE, "InitiatorAlias",       "",     ""          ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_DECLARATIVE, "TargetAddress",        "",     ""          ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_BINARY_OR,   "InitialR2T",           "Yes",  "Yes,No"    ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_BINARY_AND,  "OFMarker",             "No",   "Yes,No"    ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_BINARY_AND,  "IFMarker",             "No",   "Yes,No"    ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL_Z, "OFMarkInt",            "1",    "65536"     ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL_Z, "IFMarkInt",            "1",    "65536"     ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_BINARY_AND,  "ImmediateData",        "Yes",  "Yes,No"    ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL,   "DefaultTime2Wait",     "2",    "2"         ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL,   "DefaultTime2Retain",   "20",   "20"        ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL,   "MaxOutstandingR2T",    "1",    "1"         ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_BINARY_OR,   "DataPDUInOrder",       "Yes",  "Yes,No"    ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_BINARY_OR,   "DataSequenceInOrder",  "Yes",  "Yes,No"    ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL,   "ErrorRecoveryLevel",   "0",    "0"         ) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_DECLARATIVE, "SessionType", "Normal", "Normal,Discovery" ) == 0);

    /* Configurations of the iSCSI commands "buffer" sizes
     *
     * We use a simple configuration:
     *  - we force FirstBurstLength and MaxRecvDataSegmentLength
     *    with a default size (256KB)
     *  - we allow the target to negociate MaxBurstLength between
     *    this default size and the buffer size used by the target
     */
#define DATA_SEGMENT_LENGTH 262144

    EXA_ASSERT_VERBOSE(config_lun_buffer_size >= DATA_SEGMENT_LENGTH,
		       "Buffer size (%" PRIzu ") does not match "
		       "data segment length (%u)",
		       config_lun_buffer_size, DATA_SEGMENT_LENGTH);

    os_snprintf(str_dflt, sizeof(str_dflt), "%" PRIzu, DATA_SEGMENT_LENGTH);
    os_snprintf(str_valid, sizeof(str_valid), "%" PRIzu, config_lun_buffer_size);

    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL_Z,
                                            "MaxRecvDataSegmentLength", str_dflt, str_dflt) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL_Z,
                                            "FirstBurstLength", str_dflt, str_dflt) == 0);
    EXA_ASSERT(param_list_add(l, ISCSI_PARAM_TYPE_NUMERICAL_Z,
                                            "MaxBurstLength", str_dflt, str_valid) == 0);
}

/*
 * Currently one thread per session, used for both Rx and Tx.
 * XXX ^^^ From what I see, there is only one worker_proc_t thread... So?
 */
void worker_proc_t(void *arg)
{
    TARGET_SESSION_T *sess = (TARGET_SESSION_T *)arg;
    unsigned char header[ISCSI_HEADER_LEN];
    int logout = -3;
    int received = -4;

    exalog_as(EXAMSG_ISCSI_ID);

    __init_target_session(sess);
    session_init_sn(sess);

    /* Loop for commands or data */
    while (1)
    {
        received = iscsi_sock_msg(sess->sock, SOCKET_RECV, ISCSI_HEADER_LEN, header);
        if (received != ISCSI_HEADER_LEN)
            break;

        if ((execute_t(sess, header)) < 0)
            break;

        logout = (ISCSI_OPCODE(header) == ISCSI_LOGOUT_CMD);

        if (logout)
            break;
    }

    os_closesocket(sess->sock);

    while (1)
    {
        os_thread_mutex_lock((&sess->slock));
        if (sess->cmd_pending <= 0)
        {
            os_thread_mutex_unlock((&sess->slock));
            break;
        }
        os_thread_mutex_unlock((&sess->slock));
        exalog_debug("Session %d cmd pending %d", sess->id, sess->cmd_pending);
        /* FIXME wait for the end of all pending command for this session, yes we
           can use a waitqueue */
        os_sleep(2);
        sess_cmd_display(sess);
    }

    /* clean up any outstanding commands */
    param_list_free(sess->params);

    /* make session available */
    sess->IsLoggedIn = 0;

    /* signal to scsi command layer that this session is ended */
    scsi_del_session(sess->id);

    exalog_info("Connection closed: session %d", sess->id);

    nbd_list_post(&g_session_q.free, sess, -1);
}


int target_transfer_data(TARGET_CMD_T *cmd)
{
    TARGET_SESSION_T *sess = cmd->sess;
    ISCSI_SCSI_CMD_T *args = &(cmd->scsi_cmd);

    if (!args->todev || !(args->trans_len - args->bytes_todev))
        exalog_error("nothing to transfer for tag 0x%x", args->tag);

    /* if transfer is done, then signal the device */
    if (!(args->trans_len - args->bytes_todev))
    {
        if (!args->final)
        {
            exalog_error("Bad final bit in transfer");
            return -1;
        }
        nbd_list_post(&g_cmd_defered_send, cmd, -1);
        return 0;
    }

    /* Otherwise keep going. If we're running in R2T mode, or we've
     * reached the first burst of unsolicted data, then we must send
     * and R2T for the rest of the remaining data. */

    if (sess->sess_params.initial_r2t ||
        (sess->sess_params.first_burst_length
         && args->bytes_todev >= sess->sess_params.first_burst_length))
	return send_r2t(sess, cmd, args);

    return 0;
}

static int update_lun_authorizations(const export_t *export)
{
    unsigned int i;
    lun_t lun = export_iscsi_get_lun(export);

    for (i = 0; i < CONFIG_TARGET_MAX_SESSIONS; i++)
    {
        TARGET_SESSION_T *session = nbd_get_elt_by_num(i, &g_session_q);

        if (session == NULL || session->IsLoggedIn != 1)
            continue;

        if (export == NULL)
            session->authorized_luns[lun] = false;
        else
        {
            iqn_filter_policy_t policy;
            iqn_t initiator_iqn;

            iqn_from_str(&initiator_iqn, param_list_get_value(session->params, "InitiatorName"));
            policy = export_iscsi_get_policy_for_iqn(export, &initiator_iqn);

            session->authorized_luns[lun] = policy == IQN_FILTER_ACCEPT;
        }
    }

    return EXA_SUCCESS;
}

size_t target_get_buffer_size(void)
{
    return config_lun_buffer_size;
}

int target_get_nth_connected_iqn(lun_t lun, unsigned int n,
                                 iqn_t *iqn)
{
    unsigned int i, n_found;
    TARGET_SESSION_T *session;

    n_found = 0;
    for (i = 0; i < CONFIG_TARGET_MAX_SESSIONS; i++)
    {
        session = nbd_get_elt_by_num(i, &g_session_q);

        if (session == NULL || session->IsLoggedIn != 1
            || !session_lun_authorized(session, lun))
            continue;

        if (n_found == n)
        {
            iqn_from_str(iqn, param_list_get_value(session->params, "InitiatorName"));
            return EXA_SUCCESS;
        }

        n_found++;
    }

    return -ENOENT;
}
