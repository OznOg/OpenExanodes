/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <string.h>
#include "common/include/exa_error.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_math.h"
#include "common/include/exa_nodeset.h"
#include "common/include/threadonize.h"
#include "config/exa_version.h"
#include "target/iscsi/include/target_perf.h"
#include "target/iscsi/include/scsi.h"
#include "target/iscsi/include/iscsi.h"
#include "target/iscsi/include/endianness.h"
#include "target/iscsi/include/device.h"
#include "target/iscsi/include/target.h"
#include "target/include/target_adapter.h"
#include "target/iscsi/include/pr_lock_algo.h"
#include "target/iscsi/include/scsi_persistent_reservations.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"
#include "os/include/os_thread.h"
#include "os/include/os_inttypes.h"
#include "log/include/log.h"
#include "lum/export/include/executive_export.h"
#include "lum/export/include/export.h"
#include "blockdevice/include/blockdevice.h"

#define MAX_LUN_NAME 40
#define MAX_LUN_SERIAL 40
#define CONFIG_DISK_BLOCK_LEN_DFLT          512
#define MAX_RESPONSE_CDB 4096
#define PACK_BUFFER_TEMP 64

#define QUEUE_DEPTH 20

/*
 * Globals
 */
typedef enum
{
    SCSI_PACK_NEW_SESSION = 1,
    SCSI_PACK_DEL_SESSION = 2,
    SCSI_PACK_CDB = 3,
    SCSI_PACK_LOGICAL_UNIT_RESET = 4
} scsi_pack_type_t;

#define SCSI_PACK_TYPE_IS_VALID(spt) \
    ((spt) >= SCSI_PACK_NEW_SESSION && (spt) <= SCSI_PACK_LOGICAL_UNIT_RESET)

struct lun_data_struct
{
    char serial[MAX_LUN_SERIAL];
    lum_export_t *export;
    long long size_in_sector;
    int command_in_progress;
    int nb_waiter;
    int nb_reset_waiter;
    os_sem_t waiter;
    os_sem_t reset_waiter;
    os_thread_mutex_t lock;
    struct target_cmd_t *cmd_next;
    struct target_cmd_t *cmd_prev;
};

typedef struct
{
    unsigned char temp[PACK_BUFFER_TEMP];
    int           size;
} sam_command_t;   /* sam mean scsi architectural model */

static exa_nodeid_t this_node_id = EXA_NODEID_NONE;

struct lun_data_struct lun_data[MAX_LUNS];
static struct nbd_list g_cmd_queue;     /* worker are shared between luns */
static struct
{
    pr_context_t *pr_context;
    void *buffer;
    struct
    {
        os_sem_t new;
        os_sem_t progress;
        sam_command_t command;
    } sam;  /* use to manage sam3 comand like logical unit reset, new session */
    os_thread_mutex_t lock; /* XXX This seems to be pr_context in fact... */
} reservations;

/* Currently, only one transport above ISCSI is supported */
static const scsi_transport_t *transport = NULL;

int scsi_register_transport(const scsi_transport_t *t)
{
    EXA_ASSERT(transport == NULL);

    if (t == NULL)
        return -EINVAL;

    if (t->update_lun_access_authorizations == NULL
        || t->lun_access_authorized == NULL
        || t->async_event == NULL)
        return -EINVAL;

    transport = t;

    return EXA_SUCCESS;
}

void scsi_unregister_transport(const scsi_transport_t *t)
{
    EXA_ASSERT(transport == NULL || t == transport);
    transport = NULL;
}

/** Ugly infringing layer access to non-SCSI-specific structure */
const export_t *scsi_get_export(lun_t lun)
{
    EXA_ASSERT(LUN_IS_VALID(lun));
    if (lun_data[lun].export == NULL)
        return NULL;
    else
        return lum_export_get_desc(lun_data[lun].export);
}

/**
 * Get the serial of a lun.
 *
 * @param[in]  lun     LUN to get the serial of
 * @param[out] serial  Resulting serial
 *
 * @return EXA_SUCCES if successful, -EINVAL otherwise
 */
static int get_lun_serial(lun_t lun, char *serial)
{
    EXA_ASSERT(serial != NULL);

    if (!LUN_IS_VALID(lun))
        return -EINVAL;

    os_thread_mutex_lock(&lun_data[lun].lock);
    os_strlcpy(serial, lun_data[lun].serial, MAX_LUN_SERIAL);
    os_thread_mutex_unlock(&lun_data[lun].lock);

    return EXA_SUCCESS;
}

/**
 * Get the size of a LUN in sectors
 *
 * XXX Ideally, the prototype should be similar to that of get_lun_serial().
 *
 * @param[in]  lun      LUN to get info on
 * @param[out] sectors  Size in sectors (512 bytes)
 *
 * @return size if successful, 0 otherwise
 */
static uint64_t get_lun_sectors(lun_t lun)
{
    uint64_t sectors;

    if (!LUN_IS_VALID(lun))
        return 0;

    os_thread_mutex_lock(&lun_data[lun].lock);
    sectors = lun_data[lun].size_in_sector;
    os_thread_mutex_unlock(&lun_data[lun].lock);

    return sectors;
}

/**
 * Add a cmd to the cmd link list of the lun
 * caller must hold lun_data[lun].lock
 * @param lun
 * @param cmd cmd to add to lun link list
 */
static void scsi_lun_add_cmd(lun_t lun, struct target_cmd_t *cmd)
{
    cmd->lun_cmd_prev = NULL;
    cmd->lun_cmd_next = lun_data[lun].cmd_next;
    if (lun_data[lun].cmd_next == NULL)
        lun_data[lun].cmd_prev = cmd;
    else
        cmd->lun_cmd_next->lun_cmd_prev = cmd;
    lun_data[lun].cmd_next = cmd;
}

/**
 * Remove a cmd from cmd link list of the lun.
 * Caller must hold lun_data[lun].lock.
 * @param lun
 * @param cmd cmd to remove from link list of the lun.
 */

static void scsi_lun_del_cmd(lun_t lun, struct target_cmd_t *cmd)
{
    if (cmd->lun_cmd_prev == NULL)
    {
        lun_data[lun].cmd_next = cmd->lun_cmd_next;
    }
    else
    {
        cmd->lun_cmd_prev->lun_cmd_next = cmd->lun_cmd_next;
    }
    if (cmd->lun_cmd_next == NULL)
    {
	lun_data[lun].cmd_prev = cmd->lun_cmd_prev;
    }
    else
    {
        cmd->lun_cmd_next->lun_cmd_prev = cmd->lun_cmd_prev;
    }
}


static void scsi_lun_begin_command(lun_t lun, struct target_cmd_t *cmd)
{
    bool need_wait;

    if (lun >= MAX_LUNS)
        return;

    do
    {
        need_wait = false;

        os_thread_mutex_lock(&lun_data[lun].lock);

        if (lun_data[lun].nb_reset_waiter == 0)
        {
            lun_data[lun].command_in_progress++;
	    scsi_lun_add_cmd(lun, cmd);
	    cmd->status = COMMAND_NOT_STARTED;
        }
        else
        {
            need_wait = true;
            lun_data[lun].nb_waiter++;
        }

        os_thread_mutex_unlock(&lun_data[lun].lock);

        if (need_wait)
            os_sem_wait(&lun_data[lun].waiter);
    } while (need_wait);
}


static void scsi_lun_end_command(lun_t lun, struct target_cmd_t *cmd)
{
    if (lun >= MAX_LUNS)
        return;

    os_thread_mutex_lock(&lun_data[lun].lock);

    /* COMMAND_ABORT is already removed from command_in_progress */
    if (cmd->status != COMMAND_ABORT)
        lun_data[lun].command_in_progress--;
    EXA_ASSERT(lun_data[lun].command_in_progress >= 0);

    scsi_lun_del_cmd(lun, cmd);

    if (lun_data[lun].command_in_progress == 0)
    {
        while (lun_data[lun].nb_reset_waiter > 0)
        {
            os_sem_post(&lun_data[lun].reset_waiter);
            lun_data[lun].nb_reset_waiter--;
        }

        while (lun_data[lun].nb_waiter > 0)
        {
            os_sem_post(&lun_data[lun].waiter);
            lun_data[lun].nb_waiter--;
        }
    }

    os_thread_mutex_unlock(&lun_data[lun].lock);
}


static void lun_data_cleanup(void)
{
    lun_t lun;

    algopr_cleanup();

    for (lun = 0; lun < MAX_LUNS; lun++)
    {
        os_thread_mutex_destroy(&lun_data[lun].lock);
        memset(lun_data[lun].serial, 0, MAX_LUN_SERIAL);
        os_sem_destroy(&lun_data[lun].waiter);
        os_sem_destroy(&lun_data[lun].reset_waiter);
        lun_data[lun].export = NULL;
    }

    pr_context_free(reservations.pr_context);
    os_free(reservations.buffer);
    reservations.buffer = NULL;
    reservations.pr_context = NULL;
    os_sem_destroy(&reservations.sam.new);
    os_sem_destroy(&reservations.sam.progress);
    os_thread_mutex_destroy(&reservations.lock);
}


static void target_to_initiator_message(lun_t lun, int sense, int asc_ascq);


/* wait for all command  are done and freeze new command */
static void scsi_lun_local_logical_unit_reset(lun_t lun)
{
    bool need_wait;
    struct target_cmd_t *cmd;

    if (lun >= MAX_LUNS)
        return;

    os_thread_mutex_lock(&lun_data[lun].lock);

    /* ABORT all abortable commands i.e. RESERVE/RELEASE/PERSISTENT RESERVE
     * since all other commands are immediatly executed and READ/WRITE/SYNCHRONIZE CACHE
     * is in vrt and cannot be canceled */
    cmd = lun_data[lun].cmd_next;
    while (cmd != NULL)
    {
	if (cmd->status != COMMAND_ABORT) /* command already aborted */
            switch (cmd->scsi_cmd.cdb[0])
            {
                case PERSISTENT_RESERVE_OUT:
                case RESERVE_6:
                case RELEASE_6:
                    /* Since reserve/release/persistent reserve
                       out/scsi_lun_local_logical_unit_reset will be done in
                       the same context (the algopr thread), no additionnal
                       locking is needed */
                    exalog_info("SCSI: ABORT command tag %x type %x",
                                cmd->scsi_cmd.tag, cmd->scsi_cmd.cdb[0]);
                    lun_data[lun].command_in_progress--;
                    EXA_ASSERT(lun_data[lun].command_in_progress >= 0);
                    /* we no more consider this command in progress */
                    cmd->status = COMMAND_ABORT;
                    break;
		default:
                    break;
	    }
        cmd = cmd->lun_cmd_next;
    }

    need_wait = (lun_data[lun].command_in_progress != 0);
    if (need_wait)
	lun_data[lun].nb_reset_waiter++;

    os_thread_mutex_unlock(&lun_data[lun].lock);

    if (need_wait)
        os_sem_wait(&lun_data[lun].reset_waiter);

    /* reset spc2 reserve/release if they are any reserve/release on it */
    os_thread_mutex_lock(&reservations.lock);

    pr_reset_lun_reservation(reservations.pr_context, lun);

    os_thread_mutex_unlock(&reservations.lock);

    /*
     * FIXME: UNIT ATTENTION BUS DEVICE RESET FUNCTION OCCURED must
     * be sent to all nexus except the nexus that emitted the reset command
     * (see sam4r11 5.6.2)
     */
    target_to_initiator_message(lun,
                              SCSI_SENSE_UNIT_ATTENTION,
			      SCSI_SENSE_ASC_BUS_DEVICE_RESET_FUNCTION_OCCURED);
}


/* scsi_pack_cdb() /scsi_unpack_cdb() pack/unpack a session id, lun and cdb in a
 * buffer, in future they can also compress scsi command data block since they
 * know some field is irrelevent or must be set to 0
 */
static int scsi_pack(unsigned char *buffer,
                     lun_t lun,
                     int session_id,
                     unsigned char *cdb,
                     scsi_pack_type_t scsi_type,
                     int size,
                     int max_size)
{
    EXA_ASSERT_VERBOSE(SCSI_PACK_TYPE_IS_VALID(scsi_type),
                       "invalid scsi pack type: %d", scsi_type);

    if (size + 5 > max_size)
    {
        return 0;
    }
    set_bigendian16(lun, buffer);
    set_bigendian16(session_id, buffer + 2);
    buffer[4] = (unsigned char) scsi_type;
    memcpy(buffer + 5, cdb,  size);
    return size + 5;
}


static int scsi_unpack(unsigned char *buffer,
                       lun_t *lun,
                       int *session_id,
                       unsigned char **cdb,
                       scsi_pack_type_t *scsi_type,
                       int packed_size)
{
    *lun = get_bigendian16(buffer);
    *session_id = get_bigendian16(buffer + 2);
    *scsi_type = (scsi_pack_type_t) buffer[4];
    EXA_ASSERT_VERBOSE(SCSI_PACK_TYPE_IS_VALID(*scsi_type),
                       "invalid scsi pack type: %d", *scsi_type);
    *cdb = buffer + 5;

    return packed_size - 5;
}


/* give a global session_id (like a clustered session_id) from local session_id
 *
 * FIXME: There can be colisions on the global session ID due to
 * CONFIG_TARGET_MAX_SESSIONS modulo.
 */
static int scsi_global_session_id(int local_session_id)
{
    return local_session_id % CONFIG_TARGET_MAX_SESSIONS
        + CONFIG_TARGET_MAX_SESSIONS * this_node_id;
}


static void scsi_info_from_global_session_id(int global_session_id,
                                             exa_nodeid_t *node_id,
                                             int *local_session_id)
{
    EXA_ASSERT(node_id);
    EXA_ASSERT(local_session_id);

    *node_id = (exa_nodeid_t)(global_session_id / CONFIG_TARGET_MAX_SESSIONS);
    EXA_ASSERT(EXA_NODEID_VALID(*node_id));

    *local_session_id = global_session_id % CONFIG_TARGET_MAX_SESSIONS;
}


static void scsi_process_packed_data(unsigned char *packed_data,
                                     int packed_size,
                                     scsi_command_status_t *scsi_status)
{
    lun_t lun;
    int session_id;
    unsigned char *scsi_cdb;
    scsi_pack_type_t scsi_pack_type;

    scsi_unpack(packed_data, &lun, &session_id, &scsi_cdb,
                &scsi_pack_type, packed_size);

    EXA_ASSERT_VERBOSE(SCSI_PACK_TYPE_IS_VALID(scsi_pack_type),
                       "invalid scsi pack type: %d", scsi_pack_type);

    switch (scsi_pack_type)
    {
    case SCSI_PACK_CDB:
        os_thread_mutex_lock(&reservations.lock);
        pr_reserve_out(reservations.pr_context, lun, scsi_cdb,
                       session_id, scsi_status);
        EXA_ASSERT(scsi_status->status == SCSI_STATUS_GOOD);
        os_thread_mutex_unlock(&reservations.lock);
        break;

    case SCSI_PACK_NEW_SESSION:
        os_thread_mutex_lock(&reservations.lock);
        pr_add_session(reservations.pr_context, session_id);
        os_thread_mutex_unlock(&reservations.lock);
        break;

    case SCSI_PACK_DEL_SESSION:
        os_thread_mutex_lock(&reservations.lock);
        pr_del_session(reservations.pr_context, session_id);
        os_thread_mutex_unlock(&reservations.lock);
        break;

    case SCSI_PACK_LOGICAL_UNIT_RESET:
        if (lun == RESET_ALL_LUNS)
        {
            for (lun = 0; lun < MAX_LUNS; lun++)
                scsi_lun_local_logical_unit_reset(lun);
        }
        else
            scsi_lun_local_logical_unit_reset(lun);
        SCSI_STATUS_OK(scsi_status, 0);
        break;
    }
}


/* provided by target to pr_lock_algo
 * if a cdb is provided, we apply the cdb on our persistent reserve data instead of reread from the medium */
void scsi_pr_read_metadata(void *ref_msg, void *cdb, int cdb_size)
{
    if (cdb_size < pr_context_packed_size(reservations.pr_context))
    {
        scsi_command_status_t scsi_status; /* the status will be ignored, because the initiator of the request
                                         * already process them successfully */
        scsi_process_packed_data(cdb, cdb_size, &scsi_status);
        return;
    }

    /* we received a message with no cdb, so our persistent reserve data have been remote written to buffer */
    os_thread_mutex_lock(&reservations.lock);

    EXA_ASSERT(pr_context_unpack(reservations.pr_context, cdb, cdb_size) == 0);

    os_thread_mutex_unlock(&reservations.lock);
}


/*
 * A persistent reserve initiated by this target is finished, we will reply
 * to the command in another thread.
 * Acts as a callback (called by the PR algorithm).
 */
void scsi_pr_finished(void *private_data)
{
    TARGET_CMD_T *cmd = (TARGET_CMD_T *) private_data;

    if (private_data == &reservations.sam.command)
    {
        os_sem_post(&reservations.sam.progress);
        return;
    }
    if (private_data == ALGOPR_PRIVATE_DATA)
    {
        return; /* PR not initiated by this local target */
    }
    nbd_list_post(&g_cmd_queue, cmd, -1);
}


static int add_sense(int sense_key,
                     int asc_ascq,
                     unsigned char *sense,
                     int sense_size);

int scsi_pr_write_metadata(void *private_data, void **buffer)
{
    TARGET_CMD_T *cmd = (TARGET_CMD_T *) private_data;
    int data_size = pr_context_packed_size(reservations.pr_context);
    int global_session_id;
    scsi_command_status_t status;
    /* we try to give the command data block to the other target nodes, to
       allow us to not read new pr data from medium */
    static unsigned char temp[PACK_BUFFER_TEMP];
    int size;

    *buffer = NULL;
    if (private_data == ALGOPR_PRIVATE_DATA)
    {
        /* this pr was not initiated by local target and we have valid metadata */
        pr_context_pack(reservations.pr_context, reservations.buffer, data_size);
        *buffer = reservations.buffer;
        return data_size;
    }
    if (private_data == &reservations.sam.command)
    { /* this command is a sam command */
        *buffer = reservations.sam.command.temp;
        scsi_process_packed_data(reservations.sam.command.temp,
                                 reservations.sam.command.size,
                                 &status);
        return reservations.sam.command.size;
    }
    /* this command is a persistent reserve */
    if (cmd->status == COMMAND_ABORT)
    {
	/* this command was aborted by a logical_unit_reset() */

        /* since TAS in control mode is set to 0 (spc3r23 7.4.6)
	 * we must not send the error to initiator,
	 * so we send the error to scsi_transport to allow it
	 * to free associated ressource, but it must not
	 * send back the error to initiator */
        SCSI_STATUS_ERROR(&status,
			SCSI_STATUS_TASK_ABORTED,
			0,
                        0);
        cmd->scsi_cmd.status = status.status;
        cmd->scsi_cmd.length = status.length_out;
        cmd->scsi_cmd.fromdev = 1;
        cmd->data_len = status.length_out;
	/* cmd->status is already set */
	exalog_info("SCSI TAG %x cdb <%x> ABORTED", cmd->scsi_cmd.tag, cmd->scsi_cmd.cdb[0]);
        nbd_list_post(&g_cmd_queue, cmd, -1);
        return -1;
    }

    status.done = 0;
    global_session_id = scsi_global_session_id(sess_get_id(cmd->sess));
    os_thread_mutex_lock(&reservations.lock);
    pr_reserve_out(reservations.pr_context, cmd->scsi_cmd.lun, cmd->data,
                   global_session_id, &status);
    os_thread_mutex_unlock(&reservations.lock);
    if (status.status != SCSI_STATUS_GOOD)
    {
        status.length_out = add_sense(status.sense,
                                      status.asc_ascq,
                                      cmd->data,
                                      MAX_RESPONSE_CDB);
        cmd->scsi_cmd.status = status.status;
        cmd->scsi_cmd.length =    status.length_out;
        cmd->scsi_cmd.fromdev = 1;
        cmd->data_len = status.length_out;
        cmd->status  = COMMAND_PERSISTENT_RESERVE_FAILED;
        nbd_list_post(&g_cmd_queue, cmd, -1);
        return -1;
    }

    size = scsi_pack(temp,
                     cmd->scsi_cmd.lun,
                     global_session_id,
		     cmd->data,
                     SCSI_PACK_CDB,
                     SCSI_PERSISTENT_RESERVE_OUT_CDB_SIZE,
                     PACK_BUFFER_TEMP);
    if (size > 0)
    {
        *buffer = temp;
	/* Since reserve/release/persistent reserve out/scsi_lun_local_logical_unit_reset
	   will be done in the same context (the algopr thread), no
           additional locking is needed to change cmd->status */
	cmd->status  = COMMAND_PERSISTENT_RESERVE_SUCCESS;
    }
    return size;
}


void condition_async_new(int session_id,
                         lun_t lun,
                         int status,
                         int sense,
                         int asc_ascq);

/* FIXME Split this function in two or rename it: there is *no* reason to
         pass a node id to a function called 'lun_data_init' */
static int lun_data_init(void)
{
    lun_t lun;
    int ret;

    for (lun = 0; lun < MAX_LUNS; lun++)
    {
        os_thread_mutex_init(&lun_data[lun].lock);
        memset(lun_data[lun].serial, 0, MAX_LUN_SERIAL);
        lun_data[lun].export = NULL;
        lun_data[lun].size_in_sector = 0;
        lun_data[lun].command_in_progress = 0;
        lun_data[lun].nb_waiter = 0;
        lun_data[lun].nb_reset_waiter = 0;
        os_sem_init(&lun_data[lun].waiter, 0);
        os_sem_init(&lun_data[lun].reset_waiter, 0);
	lun_data[lun].cmd_next = NULL;
	lun_data[lun].cmd_prev = NULL;
    }

    reservations.pr_context = pr_context_alloc(condition_async_new);
    reservations.buffer = os_malloc(pr_context_packed_size(reservations.pr_context));
    os_sem_init(&reservations.sam.new, 1);
    os_sem_init(&reservations.sam.progress, 0);
    os_thread_mutex_init(&reservations.lock);

    /* FIXME: We should retrieve the Node ID by requesting a more
     * appropriate component than the VRT.
     */
    ret = algopr_init(this_node_id, pr_context_packed_size(reservations.pr_context));
    if (ret != EXA_SUCCESS)
	goto error;

    return EXA_SUCCESS;

error:
    lun_data_cleanup();
    return ret;
}


static int add_sense(int sense_key,
                     int asc_ascq,
                     unsigned char *sense,
                     int sense_size)
{
    if (sense_size < 18)
        return 0;

    memset(sense, 0, 18);
    sense[0] = SCSI_SENSE_DATA_VALID |
               SCSI_SENSE_DATA_CURENT_ERROR_FIXED_SIZE_DESCRIPTOR /* no defered error */;
    sense[2] =  sense_key;
    sense[3] = 0; /*additional sense code and sense additionnal code qualifier not suported */
    sense[7] = 8;
    set_bigendian16(asc_ascq, sense + 12);

    return 18;
}


/**
 * send a new asynchronous message with sense data to initiator connected to
 * session_id, if no command was done since the last send of this condition, we
 * don't sent another message
 */
void condition_async_new(int global_session_id,
                         lun_t lun,
                         int status, /* FIXME: remove unused argument */
                         int sense,
                         int asc_ascq)
{
    unsigned char data[18];
    exa_nodeid_t node_id;
    int local_session_id;

    scsi_info_from_global_session_id(global_session_id,
                                     &node_id,
                                     &local_session_id);

    /* if the targeted session is not on this node exit */
    if (node_id != this_node_id)
        return;

    add_sense(sense, asc_ascq, data, sizeof(data));
    transport->async_event(local_session_id, lun, data, sizeof(data));
}


/**
 * send asynchronous message to all initiator connected localy
 */
static void target_to_initiator_message(lun_t lun, int sense, int asc_ascq)
{
    int local_session_id;
    unsigned char data[18];

    for (local_session_id = 0;
         local_session_id < CONFIG_TARGET_MAX_SESSIONS;
         local_session_id++)
    {
        add_sense(sense, asc_ascq, data, sizeof(data));
        transport->async_event(local_session_id, lun, data, sizeof(data));
    }
}


/**
 * @brief Add the export into the internal structures
 *
 * @param[in] lun       The export LUN - duplicate information
 * @param[in] export    The export structure
 * @param[in] size      Size of the exported volume (in bytes)
 * @param[in] readonly  Is the export readonly - unused information
 *
 * Use only in a properly locked section with 'lun_data[lun].lock'
 */
static void lun_data_add_export(lun_t lun, lum_export_t *export,
                                uint64_t size, bool readonly)
{
    char *serial_str;

    EXA_ASSERT(LUN_IS_VALID(lun));
    EXA_ASSERT(lun_data[lun].export == NULL);

    /* FIXME : TODO : use readonly for read/write and mode sense */

    /* Initialize the data associated with the new lun */
    lun_data[lun].export = export;

    /* For now, lun serial is export uuid. */
    memset(lun_data[lun].serial, 0, MAX_LUN_SERIAL);
    serial_str = uuid2str(lum_export_get_uuid(export), lun_data[lun].serial);
    EXA_ASSERT(serial_str != NULL);

    lun_data[lun].size_in_sector = size;
}

/**
 * @brief Remove the export from the internal structures
 *
 * @param[in] lun   Logical unit number
 *
 * Use only in a properly locked section with 'lun_data[lun].lock'
 */
static void lun_data_remove_export(lun_t lun)
{
    EXA_ASSERT(LUN_IS_VALID(lun));

    lun_data[lun].serial[0]      = '\0';
    lun_data[lun].size_in_sector = 0;
    lun_data[lun].export         = NULL;
}

/**
 * @brief Is the export in-use
 *
 * @param[in] lun   Logical unit number
 *
 * Use only in a properly locked section with 'lun_data[lun].lock'
 */
static bool lun_data_is_inuse(lun_t lun)
{
    EXA_ASSERT(LUN_IS_VALID(lun));

    return lun_data[lun].cmd_next != NULL;
}

static int adapter_signal_new_export(lum_export_t *lum_export, uint64_t size)
{
    bool readonly;
    lun_t lun;
    export_t *export;

    EXA_ASSERT(lum_export != NULL);

    export = lum_export_get_desc(lum_export);

    lun = export_iscsi_get_lun(export);

    EXA_ASSERT(LUN_IS_VALID(lun));

    readonly = export_is_readonly(export);

    os_thread_mutex_lock(&lun_data[lun].lock);

    /* This function seems to take each field of export separatly, this is
     * probably just a sign that is should take the export entierly... */
    lun_data_add_export(lun, lum_export, size, readonly);

    os_thread_mutex_unlock(&lun_data[lun].lock);

    transport->update_lun_access_authorizations(export);

    /* signal the new lun to the initiators */
    target_to_initiator_message(0, SCSI_SENSE_UNIT_ATTENTION,
                                SCSI_SENSE_ASC_REPORTED_LUNS_DATA_HAS_CHANGED);
    target_to_initiator_message(lun, SCSI_SENSE_UNIT_ATTENTION,
                                SCSI_SENSE_ASC_INQUIRY_DATA_HAS_CHANGED);

    return EXA_SUCCESS;
}

static int adapter_signal_remove_export(const lum_export_t *lum_export)
{
    export_t *export;
    lun_t lun;

    EXA_ASSERT(lum_export != NULL);

    export = lum_export_get_desc(lum_export);

    lun = export_iscsi_get_lun(export);

    EXA_ASSERT(LUN_IS_VALID(lun));

    os_thread_mutex_lock(&lun_data[lun].lock);

    if (lun_data[lun].export == NULL)
    {
        exalog_error("Failed to remove export LUN %" PRIlun ": "
                     "not initialized in the SCSI component", lun);
        os_thread_mutex_unlock(&lun_data[lun].lock);
        return -ENOENT;
    }

    EXA_ASSERT_VERBOSE(lun_data[lun].export == lum_export,
                       "Export doesn't match previously registered one at LUN %"PRIlun,
                       lun);

    if (lun_data_is_inuse(lun))
    {
        os_thread_mutex_unlock(&lun_data[lun].lock);
        return -VRT_ERR_VOLUME_IS_IN_USE;
    }

    lun_data_remove_export(lun);

    os_thread_mutex_unlock(&lun_data[lun].lock);

    target_to_initiator_message(0, SCSI_SENSE_UNIT_ATTENTION,
                                SCSI_SENSE_ASC_REPORTED_LUNS_DATA_HAS_CHANGED);

    return 0;
}


static int adapter_signal_export_update_iqn_filters(const lum_export_t *lum_export)
{
    lun_t lun;
    const export_t *export = lum_export_get_desc(lum_export);

    EXA_ASSERT(export != NULL);

    lun = export_iscsi_get_lun(export);

    transport->update_lun_access_authorizations(export);

    /* signal the new lun to the initiators */
    target_to_initiator_message(0, SCSI_SENSE_UNIT_ATTENTION,
                                SCSI_SENSE_ASC_REPORTED_LUNS_DATA_HAS_CHANGED);
    target_to_initiator_message(lun, SCSI_SENSE_UNIT_ATTENTION,
                                SCSI_SENSE_ASC_INQUIRY_DATA_HAS_CHANGED);

    return EXA_SUCCESS;
}


static void adapter_export_set_size(lum_export_t *export, uint64_t size_in_sector)
{
    int lun;
    struct lun_data_struct *ld = NULL;
    bool need_attention = false;

    EXA_ASSERT(export != NULL);
    EXA_ASSERT(size_in_sector != 0);

    /* Find the export in lun_data array and keep the lock on it when found */
    for (lun = 0; lun < MAX_LUNS; lun++)
    {
        ld = &lun_data[lun];

        os_thread_mutex_lock(&ld->lock);

        if (ld->export == export)
            break;

        os_thread_mutex_unlock(&ld->lock);
    }

    /* Assume that the export MUST exist */
    EXA_ASSERT(lun < MAX_LUNS);

    if (size_in_sector != ld->size_in_sector)
	need_attention = true;

    ld->size_in_sector = size_in_sector;

    os_thread_mutex_unlock(&ld->lock);

    if (need_attention)
        target_to_initiator_message(lun, SCSI_SENSE_UNIT_ATTENTION,
                                    SCSI_SENSE_ASC_CAPACITY_DATA_HAS_CHANGED);
}

static lum_export_inuse_t adapter_export_get_inuse(const lum_export_t *lum_export)
{
    export_t *export;
    lum_export_inuse_t inuse;
    lun_t lun;

    export = lum_export_get_desc(lum_export);

    lun = export_iscsi_get_lun(export);

    EXA_ASSERT(LUN_IS_VALID(lun));

    os_thread_mutex_lock(&lun_data[lun].lock);

    if (lun_data[lun].export != lum_export)
    {
        exalog_error("Cannot get 'in-use' information for export LUN %" PRIlun ": "
                     "not initialized in the SCSI component", lun);
        os_thread_mutex_unlock(&lun_data[lun].lock);
        return EXPORT_UNKNOWN_IN_USE;
    }

    if (lun_data_is_inuse(lun))
        inuse = EXPORT_IN_USE;
    else
        inuse = EXPORT_NOT_IN_USE;

    os_thread_mutex_unlock(&lun_data[lun].lock);

    return inuse;
}

static int adapter_static_init(exa_nodeid_t node_id)
{
    this_node_id = node_id;
    return lun_data_init();
}

static int adapter_static_cleanup(void)
{
    lun_data_cleanup();

    return EXA_SUCCESS;
}

static bool target_init_done = false;

static int adapter_init(const lum_init_params_t *params)
{
    int err;
    err = target_init(params->iscsi_queue_depth, params->buffer_size,
                       &params->target_iqn, params->target_listen_address);
    target_init_done = err == 0;

    return err;
}

static int adapter_cleanup(void)
{
    if (!target_init_done)
        return 0;

    target_init_done = false;

    return target_shutdown();
}

/*
 * Prototypes
 */

static void lun_worker_proc(void *arg);
static void prepare_disk_read(TARGET_CMD_T *cmd,
                              lun_t lun,
                              unsigned long long lba,
                              uint32_t len,
                              scsi_command_status_t *scsi_status);
static void prepare_disk_write(TARGET_CMD_T *cmd,
                               lun_t lun,
                               unsigned long long lba,
                               uint32_t len,
			       bool fua,
                               scsi_command_status_t *scsi_status);

/*
 * Target's interface.  The target system calls all commands named "device_"
 */

#define BUS_INACTIVITY_LIMIT 20 /* after 20s of nothing, initiator can think there are a problem and issue a lun reset */
static void do_mode_sense(lun_t lun,
                          unsigned char *cdb,
                          unsigned char *data,
                          scsi_command_status_t *scsi_status)
{
    struct {
        uint8_t op_code      :8;
        uint8_t reserved     :3;
        bool dbd             :1;
        bool llbaa           :1; /* only valid for sense10 */
        uint8_t lun          :3; /* only valid for sense6 */
        uint8_t page_code    :6;
        uint8_t page_ctl     :2;
        uint8_t subpage_code :8;
        union {
            struct {
                uint8_t alloc_len :8;
                uint8_t control   :8;
            } _6;
            struct {
                uint8_t reserved[3];
                uint8_t alloc_len_msb :8;
                uint8_t alloc_len_lsb :8;
                uint8_t control       :8;
            } _10;
        };
    } *mode_sense = (void *)cdb;
    uint16_t alloc_len;
    int len = 4;
    int err = -1;
    int block_descriptor_length = 0;

    size_t target_buffer_size = target_get_buffer_size();

    switch (mode_sense->op_code)
    {
    case MODE_SENSE_6:
        alloc_len = mode_sense->_6.alloc_len;
        data[1] = 0;   /* medium type must be 0 sb3r07 6.3.1 */
        data[2] = SCSI_SENSE_DOPFUA;   /* device specific parametter
                                        * sbc3r07 6.3.1
                                        * SCSI_SENSE_DOPFUA | SCSI_SENSE_WP */
        /* TODO : set SCSI_SENSE_WP if needed */
        len = 4;
        break;

    case MODE_SENSE_10:
        alloc_len = (mode_sense->_10.alloc_len_msb << 8)
                     + mode_sense->_10.alloc_len_lsb;
        data[2] = 0;   /* medium type must be 0 sb3r07 6.3.1 */
        data[3] = SCSI_SENSE_DOPFUA;   /* device specific parametter
                                        * sbc3r07 6.3.1
                                        * SCSI_SENSE_DOPFUA | SCSI_SENSE_WP */
        /* TODO : set SCSI_SENSE_WP if needed */
        len = 8;
        break;

    default:
        EXA_ASSERT(0);
        break;
    }

    if (!mode_sense->dbd)
    {
        block_descriptor_length = 8;
        set_bigendian32(get_lun_sectors(lun), data + len);
        set_bigendian32(512, data + len + 4);
        len += 8;
    }

    if (mode_sense->page_code & MODE_PAGE_DISCONNECT_RECONNECT)
    {
        data[len + 0] = MODE_PAGE_DISCONNECT_RECONNECT;
        data[len + 1] = 0xe;
        data[len + 2] = 0x80;
        data[len + 3] = 0x80;
        set_bigendian16(BUS_INACTIVITY_LIMIT, data + len + 4);
        set_bigendian16(0, data + len + 6);   /* no disconnect time limit */
        set_bigendian16(0, data + len + 8);   /* no connect time limit */
	/* maximum burst size, 512 is in SCSI and have no relation with CONFIG_DISK_BLOCK_LEN_DFLT*/
        set_bigendian16(target_buffer_size / 512, data + len + 10);
        data[len + 12] = 0;
	data[len + 13] = 0; /* unused reserved field */
	/* first burst size, 512 is in SCSI and have no relation with CONFIG_DISK_BLOCK_LEN_DFLT*/
        set_bigendian16(target_buffer_size / 512, data + len + 14);
        len +=  data[len + 1] + 2;
        err = 0;
    }

    if (mode_sense->page_code & MODE_PAGE_CACHING_MODE_PAGE)
    {
        data[len + 0] = MODE_PAGE_CACHING_MODE_PAGE;
        data[len + 1] = 0x12;
        data[len + 2] = MODE_SENSE_CACHING_MODE_DISC |
                        MODE_SENSE_CACHING_MODE_WCE;
        /* IC = 0 ABPF = 0 CAP = 0 DISC = 1 SIZE = 0 WCE = 1 MF = 0 RCD = 0 */
        data[len + 3] = 0;
        set_bigendian16(0xffff, data + len + 4);  /* disable pre-fetch transfer length */
        set_bigendian16(0,      data + len + 6);  /* minimum pre-fetch */
        set_bigendian16(0xffff, data + len + 8);  /* maximum pre-fetch */
        set_bigendian16(0xffff, data + len + 10); /* maximum pre-fetch ceiling*/
        data[len + 12] = MODE_SENSE_CACHING_MODE_LBCSS;
        data[len + 13] = 255;   /* number of cache segment */
        set_bigendian16(0xffff, data + len + 14);  /*cache size */
        data[len + 16] = 0; /* Unused */
        data[len + 17] = 0; /* Unused */
        data[len + 18] = 0; /* Unused */
        data[len + 19] = 0; /* Unused */
        len += data[len + 1] + 2;
        err = 0;
    }

    if (mode_sense->page_code & MDOE_PAGE_CONTROL_MODE_PAGE)
    {
        data[len] = MDOE_PAGE_CONTROL_MODE_PAGE;
        data[len + 1] = 0xa;
        data[len + 2] =
		/* we doesn(t have a clustered task set */
		MODE_SENSE_CONTROL_TST_LOGICAL_UNIT_MAINTAINS_SEPARATE_TASK_SETS_FOR_EACH_I_T_NEXUS |
		MODE_SENSE_GLTSD;   /* TST = 0 TMF = 0 D_SENSE = 0 GLTSD = 1 RLEC = 0*/
        data[len + 3] =
            MODE_SENSE_QUEUE_ALGORITHM_MODIFIER_UNRESTRICTED_REORDERING_ALLOWED;
        data[len + 4] = 0;   /* vs = 0 rac = 0 uaintclk_ctrl = 0 wsp = 0*/
        data[len + 5] = 0;   /* ato = 0 tas = 0 autoload mode = 0 */
	data[len + 6] = 0; /* Obsolete field */
	data[len + 7] = 0; /* Obsolete field */
        set_bigendian16(BUS_INACTIVITY_LIMIT, data + len + 8);  /* busy timeout period */
        set_bigendian16(0, data + len + 10);   /* extented self-test completion */
        len += data[len + 1] + 2;
        err = 0;
    }

    /* disable this page, it was to badly implemented
     * if (mode_sense->page_code & MODE_PAGE_PROTOCOL_SPECIFIC_PORT_MODE_PAGE)
     * { //iscsi data
     *   memset (data + len , 0,  0x18);
     *   data[len] = MODE_PAGE_PROTOCOL_SPECIFIC_PORT_MODE_PAGE;
     *   data[len + 1] =
     *   data[len + 2] = INQUIRY_PROTOCOL_ISCSI;
     *   data[len + 4 + 1] = 0x16;
     *   set_bigendian16(CONFIG_DISK_BLOCK_LEN_DFLT,data + 4 +12+len);
     *   data[len+4 + 20] = 0x80;
     *   len += 0x16 + 2 ;
     *   err = 0;
     * }*/

    if (mode_sense->page_code & MODE_PAGE_INFORMATIONNAL_EXCEPTIONS_CONTROL)
    {
        data[len + 0] = MODE_PAGE_INFORMATIONNAL_EXCEPTIONS_CONTROL;
        data[len + 1] = 0x0a;
        data[len + 2] = 0;
        data[len + 3] = MODE_SENSE_1Ch_MRIE_GENERATE_UNIT_ATTENTION; /* MRIE */
        data[len + 4] = (MODE_SENSE_1Ch_INERVAL_TIMER >> 24) & 0xff;
        data[len + 5] = (MODE_SENSE_1Ch_INERVAL_TIMER >> 16) & 0xff;
        data[len + 6] = (MODE_SENSE_1Ch_INERVAL_TIMER >> 8) & 0xff;
        data[len + 7] = (MODE_SENSE_1Ch_INERVAL_TIMER) & 0xff;
        data[len + 8] = (MODE_SENSE_1Ch_MAX_REPORT_COUNT >> 24) & 0xff;
        data[len + 9] = (MODE_SENSE_1Ch_MAX_REPORT_COUNT >> 16) & 0xff;
        data[len + 10] = (MODE_SENSE_1Ch_MAX_REPORT_COUNT >> 8) & 0xff;
        data[len + 11] = (MODE_SENSE_1Ch_MAX_REPORT_COUNT) & 0xff;
        len += data[len + 1] + 2;
        err = 0;
    }

    switch (mode_sense->op_code)
    {
    case MODE_SENSE_6:
        data[0] = len - 1;
        data[3] = block_descriptor_length;
        break;

    case MODE_SENSE_10:
        set_bigendian16(len - 2, data);
        set_bigendian16(block_descriptor_length, data + 6);
        data[4] = 0;      /* longlba = 0 => divide by 8 */
        break;

    default:
        EXA_ASSERT(0);
        break;
    }

    if (err == -1)
    {
        SCSI_STATUS_ERROR(scsi_status,
                          SCSI_STATUS_CHECK_CONDITION,
                          SCSI_SENSE_ILLEGAL_REQUEST,
                          SCSI_SENSE_ASC_INVALID_FIELD_IN_CDB);
    }
    else
    {
        SCSI_STATUS_OK(scsi_status, MIN(alloc_len, len));
    }
}

/**
 * @brief Gives the list of LUNs provided by the target
 *
 * @param[in]  session      session we want to build the list for
 * @param[in]  cdb          obscure piece of header
 * @param[out] data         buffer containing the list of LUNs
 * @param[out] scsi_status  status to return
 *
 */
static void scsi_report_luns(TARGET_SESSION_T *session,
                             unsigned char *cdb,
                             unsigned char *data,
                             scsi_command_status_t *scsi_status)
{
    lun_t lun;
    int len = get_bigendian32(cdb + 6);
    unsigned char *current_data;

    if (len < 16)
    /* spc4r23 6.21 repoorts luns note 30 */
    {
        SCSI_STATUS_ERROR(scsi_status,
                          SCSI_STATUS_CHECK_CONDITION,
                          SCSI_SENSE_ILLEGAL_REQUEST,
                          SCSI_SENSE_ASC_INVALID_FIELD_IN_CDB);
        return;
    }

    /* Rq: SCSI convention and 'de facto' protocole
     * - each LUN in the list are coded on 8 bytes
     *   ... but only the first and second are realy usefull (cf. 'lun_set_bigendian')
     * - the list size (in bytes) is coded on the first 4 bytes
     * - bytes 4-7 are "Reserved" for ... something we apparently dont't care
     */
    current_data = data + 8;

    if (cdb[2] == 0x1)
    {
        /* Gives the list of all LUNs tagged WELL_KNOWN_LU
         *
         * FIXME : not compliant with the spc 3r32-2-1 (§6.21)
         * the code bellow always return LUN 0 while it should
         * return something like :
         *
         * for (lun = 0; lun < 2;  lun++)
         *  {
         *      if (get_lun_sectors(lun) == 0
         *          || !session_lun_authorized(session, lun))
         *      {
         *          lun_set_bigendian(lun, current_data);
         *          current_data += 8;
         *      }
         *  }
         *
         */
        lun_set_bigendian(0, current_data); current_data += 8;
    }
    else
    {
        /* Gives the list of all the LUNs the session is authorized to access */

        /* LUN 0 and 1 are always accessible */
        lun_set_bigendian(0, current_data); current_data += 8;
        lun_set_bigendian(1, current_data); current_data += 8;

        for (lun = 2; lun < MAX_LUNS;  lun++)
        {
            if (get_lun_sectors(lun) > 0
                && transport->lun_access_authorized(session, lun))
            {
                lun_set_bigendian(lun, current_data);
                current_data += 8;
            }
        }
    }

    set_bigendian32(current_data - (data + 8), data); /* size of the list (list head - list tail) */
    set_bigendian32(0, data + 4);                     /* wipe the "Reserved" field */

    SCSI_STATUS_OK(scsi_status, MIN(current_data - data, len));
}


static unsigned char * scsi_inquiry_add_id(unsigned char * data,
		int protocol_id,
		int piv,
		int association,
		int identificator_type,
		int code_set,
		int size)
{
    data[data[3] + 4] = (protocol_id << 4) | code_set;
    data[data[3] + 5] = (piv << 7) | (association << 4) | identificator_type;
    data[data[3] + 6] = 0;      /*reserved */
    data[data[3] + 7] = size;
    data[3] += (4 + size);
    return data + data[3] + 4 - size;
}

/*
 * sp3r23 7.6.3.3 Vendor specific identifier format
 * If the identifier type is 0h (i.e., vendor specific), no assignment authority was used and there is no guarantee that
 * the identifier is globally unique (i.e., the identifier is vendor specific).
 */
static void scsi_inquiry_add_vendor_specific(unsigned char * data,
                char * vendor_specific_id)
{
    int n = strlen(vendor_specific_id);
    unsigned char * id = scsi_inquiry_add_id(data, INQUIRY_PROTOCOL_FC_FCP2, 1,
                    INQUIRY_ASSOCIATION_LUN, INQUIRY_TYPE_VENDOR_SPECIFIC,
                    INQUIRY_CODE_SET_ASCII, n);
    strncpy((char *)id, vendor_specific_id, n); /* no need of final zero */
}

/*
 * spc3r23 7.6.3.6 NAA Identifier globally unique
 */
static void scsi_inquiry_add_naa(unsigned char * data,
                uint64_t ieee_company_id,
                uint64_t vendor_id)
{
    unsigned char * id = scsi_inquiry_add_id(data, INQUIRY_PROTOCOL_FC_FCP2, 1,
		    INQUIRY_ASSOCIATION_LUN, INQUIRY_TYPE_NAA,
		    INQUIRY_CODE_SET_BINARY, INQUIRY_NAA_IEEE_EXTENDED_SIZE);
    id[0] = (INQUIRY_NAA_IEEE_EXTENDED << 4) + ((vendor_id >> 32) & 0x0f);
    id[1] = (vendor_id >> 24) & 0xff;
    id[2] = (ieee_company_id >> 16) & 0xff;
    id[3] = (ieee_company_id >> 8) & 0xff;
    id[4] = ieee_company_id & 0xff;
    id[5] = (vendor_id >> 16) & 0xff;
    id[6] = (vendor_id >> 8) & 0xff;
    id[7] = vendor_id  & 0xff;
}

/*
 * sp3r23 7.6.3.4 T10 Identifier globally unique
 */
static void scsi_inquiry_add_t10(unsigned char * data,
                char * company_id,
                char * vendor_id)
{
    int n = strlen(vendor_id);
    unsigned char * id = scsi_inquiry_add_id(data, INQUIRY_PROTOCOL_FC_FCP2, 1,
		    INQUIRY_ASSOCIATION_LUN, INQUIRY_TYPE_T10_VENDOR_ID,
		    INQUIRY_CODE_SET_ASCII, n + 8);
    memset(id, 0, 8);
    strncpy((char *)id, company_id, 8);
    strcpy((char *)id + 8, vendor_id);
}


/**
 * @brief Get information about the device presented by a given LUN
 *
 * @param[in]  session      session we want to build the list for
 * @param[in]  lun          the LUN corresponding to the request
 * @param[in]  cbd          obscure request header
 * @param[out] data         the buffer to fill with the answer
 * @param[out] scsi_status  the status to return
 */
static void scsi_inquiry(TARGET_SESSION_T *session,
                         lun_t lun,
                         unsigned char *cdb,
                         unsigned char *data,
                         scsi_command_status_t *scsi_status)
{
    struct {
        uint8_t op_code       :8;
        bool evpd             :1;
        uint8_t reserved1     :4;
        uint8_t lun           :3;
        uint8_t page_code     :8;
        uint8_t alloc_len_msb :8;
        uint8_t alloc_len_lsb :8;
        uint8_t control       :8;
    } *inquiry_cmd = (void *)cdb;
    struct {
        uint8_t periph_dev_type       :5;
        uint8_t periph_qualifier      :3;
        uint8_t device_type_modifier  :7;
        bool    removable             :1;
        uint8_t ansi_approved_version :3;
        uint8_t ecma_version          :3;
        uint8_t iso_version           :2;
        uint8_t response_data_format  :4;
        uint8_t reserved1             :2;
        bool    trmIOP                :1;
        bool    aerc                  :1;
        uint8_t additional_length     :8;
        uint8_t reserved2[2];
        bool    sftre                 :1;
        bool    cmdque                :1;
        bool    reserved              :1;
        bool    linked                :1;
        bool    sync                  :1;
        bool    wbus16                :1;
        bool    wbus32                :1;
        bool    reladr                :1;
        uint8_t vendor_id[8];
        uint8_t product_id[16];
        uint8_t product_rev_level[4];
        uint8_t vendor_specific[20];
        uint8_t reserved3[40];
        uint8_t vendor_specific_param :8;
    } *inquiry_data = (void *)data;
    char serial[MAX_LUN_SERIAL];
    uint16_t alloc_len = (inquiry_cmd->alloc_len_msb << 8)
                         + inquiry_cmd->alloc_len_lsb;
    int len = 0;
    uint64_t nb_sectors;
    exa_uuid_t uuid;
    uint64_t chksum;

    size_t target_buffer_size = target_get_buffer_size();

    memset(inquiry_data, 0, alloc_len);         /* Clear allocated buffer */

    if (alloc_len > MAX_RESPONSE_CDB)
        alloc_len = MAX_RESPONSE_CDB;

    /* get the lun information to forge the answer */
    get_lun_serial(lun, serial);
    nb_sectors = get_lun_sectors(lun);

    /* set the peripheral information
     *
     * Rq: formerly the initializations QUALIFIER_CONNECTED and DEVICE_TYPE_SBC
     * were done implicitely with memset (as they are both equal to 0).
     */
    if (lun > MAX_LUNS)
    {
        inquiry_data->periph_qualifier = INQUIRY_PERIPHERAL_QUALIFIER_NOT_CAPABLE;
        inquiry_data->periph_dev_type = INQUIRY_PERIPHERAL_DEVICE_TYPE_UNKNOWN;

        /* Rq: 36 is the size of the information not vendor specific */
	SCSI_STATUS_OK(scsi_status,  MIN(alloc_len, 36));
	return;
    }

    if (nb_sectors > 0 && transport->lun_access_authorized(session, lun))
    {
        inquiry_data->periph_qualifier = INQUIRY_PERIPHERAL_QUALIFIER_CONNECTED;
        inquiry_data->periph_dev_type = INQUIRY_PERIPHERAL_DEVICE_TYPE_SBC;
    }
    else if (lun == 0 || lun == 1)
    {
        inquiry_data->periph_qualifier = INQUIRY_PERIPHERAL_QUALIFIER_CONNECTED;
        inquiry_data->periph_dev_type = INQUIRY_PERIPHERAL_DEVICE_TYPE_WELL_KNOWN_LU;
    }
    else
    {
        inquiry_data->periph_qualifier = INQUIRY_PERIPHERAL_QUALIFIER_CAPABLE;
        inquiry_data->periph_dev_type = INQUIRY_PERIPHERAL_DEVICE_TYPE_UNKNOWN;
    }

    if (!inquiry_cmd->evpd)
    {
        if (inquiry_cmd->page_code != 0 || alloc_len < 16)
        {
            SCSI_STATUS_ERROR(scsi_status,
                              SCSI_STATUS_CHECK_CONDITION,
                              SCSI_SENSE_ILLEGAL_REQUEST,
                              SCSI_SENSE_ASC_INVALID_FIELD_IN_CDB);
            return;
        }

        /* If the EVPD parameter bit is zero and the Page Code parameter byte
         * is zero then the target will return the standard inquiry data */
        inquiry_data->removable = false;
        inquiry_data->device_type_modifier = 0;
        inquiry_data->ansi_approved_version = INQUIRY_VERSION_SPC3;
        inquiry_data->response_data_format = INQUIRY_RESPONSE_DATA_SCSI3;
        inquiry_data->aerc = true; /* only for vmware, because AERC is obsolete in spc4 */
        inquiry_data->reserved2[1] = INQUIRY_MULTIP; /* FIXME norm does not seem clear with this*/
        inquiry_data->cmdque = true; /* Tagged Command Queueing */

        /* FIXME using memcpy here may expose internal data to outside as
         * SCSI_VENDOR_ID may be actually smaller than sizeof(inquiry_data->vendor_id) */
        memcpy(inquiry_data->vendor_id, SCSI_VENDOR_ID, sizeof(inquiry_data->vendor_id));
        memcpy(inquiry_data->product_id, SCSI_PRODUCT_ID, sizeof(inquiry_data->product_id));
        memcpy(inquiry_data->product_rev_level, EXA_VERSION, sizeof(inquiry_data->product_rev_level));
        /* vendor specific is not filled */

	/* additionnal field */
        /* FIXME I cannot find documentation about this fields...
         * complete list is given in http://www.t10.org/lists/stds.htm
         * (good luck) */
        set_bigendian16(INQUIRY_VERSION_DESCRIPTOR_ISCSI_NO_VERSION_CLAIMED,
                        data + 58);
	set_bigendian16(INQUIRY_VERSION_DESCRIPTOR_SPC3_T10_1416_D_R23,
			data + 60);
	set_bigendian16(INQUIRY_VERSION_DESCRIPTOR_SPC3_ANSI_INCITS_408_2005,
			data + 62);
	len = 64;
	if (nb_sectors > 0)
	{
            set_bigendian16(INQUIRY_VERSION_DESCRIPTOR_SBC_T10_0999_D_08b,
                        data + len);
	    len += 2;
	}
	data[4] = len - 5;  /* Additional length */

        SCSI_STATUS_OK(scsi_status,  MIN(alloc_len, len));
    }
    else
    {
        /* vital product data (VPD) are requested */
        if (nb_sectors == 0)
        {
            /* no device in this lun, so no evpd page supported ! spc3r23 6.4.4 */
            SCSI_STATUS_ERROR(scsi_status,
                              SCSI_STATUS_CHECK_CONDITION,
                              SCSI_SENSE_ILLEGAL_REQUEST,
                              SCSI_SENSE_ASC_INVALID_FIELD_IN_CDB);
            return;
        }

        data[1] = 0x0;
        switch (inquiry_cmd->page_code)
        {                           /* cmd->data[0-1] is the same as for normal inquiry */
        case INQUIRY_PAGE_SUPORTED_VPD_PAGE:
            data[1] = INQUIRY_PAGE_SUPORTED_VPD_PAGE;
            data[2] = 0x00;
            data[3] = 7 - 3;        /* (last == 7 ) -3 */
            data[4] = INQUIRY_PAGE_SUPORTED_VPD_PAGE;       /* this page is suported ! */
            data[5] = INQUIRY_PAGE_BLOCKS_LIMIT;
            data[6] = INQUIRY_PAGE_DEVICE_IDENTIFICATION;
            data[7] = INQUIRY_PAGE_UNIT_SERIAL_NUMBER;
            SCSI_STATUS_OK(scsi_status,  MIN(4 + data[3], alloc_len));
            break;

        case INQUIRY_PAGE_UNIT_SERIAL_NUMBER:
            data[1] = INQUIRY_PAGE_UNIT_SERIAL_NUMBER;
            data[2] = 0x00;
            data[3] = strlen(serial);
            strcpy((char *)(data + 4), serial);
            SCSI_STATUS_OK(scsi_status,  MIN(4 + data[3], alloc_len));
            break;

        case INQUIRY_PAGE_BLOCKS_LIMIT:
            data[1] = INQUIRY_PAGE_BLOCKS_LIMIT;
            data[3] = 0xc;
            set_bigendian16(4096 / CONFIG_DISK_BLOCK_LEN_DFLT, data + 6);                /* Optimum transfer alignement */
            set_bigendian32(target_buffer_size / CONFIG_DISK_BLOCK_LEN_DFLT, data + 8);  /* Maximum transfer length */
            set_bigendian32(target_buffer_size / CONFIG_DISK_BLOCK_LEN_DFLT, data + 12); /*Optimum transfer length */
            SCSI_STATUS_OK(scsi_status,  MIN(4 + data[3], alloc_len));
            break;

        case INQUIRY_PAGE_DEVICE_IDENTIFICATION:
            data[1] = INQUIRY_PAGE_DEVICE_IDENTIFICATION;
            data[2] = 0;
            data[3] = 0;
            scsi_inquiry_add_vendor_specific(data, serial); /* it is not unique
                                                             but spc3r23 7.6.3.3 say "there is no
                                                             guarantee that this identifier
                                                             is globally unique" */

            if (uuid_scan (serial , &uuid) == EXA_SUCCESS)
            {
                /* naa request a 36 bits uuid wich is concat wit 12 bits company OUI (naa is requested by Windows) */
                scsi_inquiry_add_naa(data, SEANODES_IEEE_COMPANY_ID_OUI,  (uint64_t)uuid.id[0]
                               + (((uint64_t)uuid.id[1] & 0xf) << 32ULL));
            }
            else
            {
                chksum = 0;
                switch (strlen(serial)>4?5:strlen(serial))
                {
                case 4:
                    chksum += (uint64_t)serial[3] << 32ULL;
                case 3:
                    chksum += (uint64_t)serial[2] << 24ULL;
                case 2:
                    chksum += (uint64_t)serial[1] << 16ULL;
                case 1:
                    chksum += (uint64_t)serial[0] << 16ULL;
                default: /* no chksum */
                    break;
                }
                scsi_inquiry_add_naa(data, SEANODES_IEEE_COMPANY_ID_OUI,  chksum);
            }

            /* t10 must be unique (requested by VMWare ESX 3.5) SEANODES was
             * already registered at T10 see
             * http://www.t10.org/lists/vid-alph.htm#VID_S
             */
            scsi_inquiry_add_t10(data, "Seanodes", serial);

            SCSI_STATUS_OK(scsi_status, MIN(data[3] + 4, alloc_len));
            break;

        default:
            SCSI_STATUS_ERROR(scsi_status,
                              SCSI_STATUS_CHECK_CONDITION,
                              SCSI_SENSE_ILLEGAL_REQUEST,
                              SCSI_SENSE_ASC_INVALID_FIELD_IN_CDB);
        }
    }
}


int device_init(struct nbd_root_list *cmd_root)
{
    os_thread_t thread;

    ISCSI_TARGET_PERF_INIT();

    /* shared lun command queue */
    if (nbd_init_list(cmd_root, &g_cmd_queue) < 0)
        return -1;

    if (!os_thread_create(&thread, 0, lun_worker_proc, NULL))
        return -1;

    return 0;
}


int device_cleanup(struct nbd_root_list *cmd_root)
{
    ISCSI_TARGET_PERF_CLEANUP();
    nbd_close_root(cmd_root);
    return 0;
}

/**
 * @brief BIO termination callback
 *
 * The function interpretes the BIO error code. It puts the corresponding
 * command into the NBD list 'g_cmd_queue' so it can be treated by the
 * "replying" thread 'lun_worker_proc'.
 *
 * FIXME this function probably makes part of the LUM executive...  (I am not
 * sure that scsi commands nee to know about BIOs)
 */
static void disk_end_io(int err, void *data)
{
    TARGET_CMD_T *cmd = (TARGET_CMD_T *) data;

    switch(cmd->status)
    {
    case COMMAND_READ_NEED_READ:
        cmd->status = (err == 0) ? COMMAND_READ_SUCCESS : COMMAND_READ_FAILED;
	exalog_trace("disk_end_io tag %x %s", cmd->scsi_cmd.tag,
		     (err == 0) ? "COMMAND_READ_SUCCESS" : "COMMAND_READ_FAILED");
	break;

    case COMMAND_WRITE_NEED_WRITE:
        cmd->status = (err == 0) ? COMMAND_WRITE_SUCCESS : COMMAND_WRITE_FAILED;
	exalog_trace("disk_end_io tag %x %s", cmd->scsi_cmd.tag,
		     (err == 0) ? "COMMAND_READ_SUCCESS" : "COMMAND_WRITE_FAILED");
	break;

    default:
	EXA_ASSERT_VERBOSE(0, "End IO tag %x: Unexpected command status %d\n",
			   cmd->scsi_cmd.tag, cmd->status);
    }

    nbd_list_post(&g_cmd_queue, cmd, -1);
}


/* called by target when a new session from an initiator was created */
void scsi_new_session(int session_id)
{
    scsi_command_status_t scsi_status; /* for this command we ignore the status */

    os_sem_wait(&reservations.sam.new);

    reservations.sam.command.size = scsi_pack(reservations.sam.command.temp,
                                              MAX_LUNS + 1,
                                              scsi_global_session_id(session_id),
                                              NULL,
                                              SCSI_PACK_NEW_SESSION,
                                              0,
                                              PACK_BUFFER_TEMP);
    scsi_process_packed_data(reservations.sam.command.temp,
                             reservations.sam.command.size,
                             &scsi_status);
    algopr_pr_new_pr(&reservations.sam.command);

    os_sem_wait(&reservations.sam.progress);

    /* now the command is processed */
    os_sem_post(&reservations.sam.new);
}


/* called by target when a new session from an initiator was deleted */
void scsi_del_session(int session_id)
{
    scsi_command_status_t scsi_status; /* for this command we ignore the status */

    os_sem_wait(&reservations.sam.new);
    reservations.sam.command.size = scsi_pack(reservations.sam.command.temp,
                                              MAX_LUNS + 1,
                                              scsi_global_session_id(session_id),
                                              NULL,
                                              SCSI_PACK_DEL_SESSION,
                                              0,
                                              PACK_BUFFER_TEMP);
    scsi_process_packed_data(reservations.sam.command.temp,
                             reservations.sam.command.size,
                             &scsi_status);
    algopr_pr_new_pr(&reservations.sam.command);

    os_sem_wait(&reservations.sam.progress);

    /* now the command is processed */
    os_sem_post(&reservations.sam.new);
}


/* called by target when a new session from an initiator was deleted */
void scsi_logical_unit_reset(lun_t lun)
{
    scsi_command_status_t scsi_status; /* for this command we ignore the status */

    if (lun >= MAX_LUNS && lun != RESET_ALL_LUNS)
    {
        exalog_error("SCSI trying to reset an invalid lun %u", (unsigned) lun);
        return;
    }
    os_sem_wait(&reservations.sam.new);

    reservations.sam.command.size = scsi_pack(reservations.sam.command.temp,
                                              lun,
                                              -1,
                                              NULL,
                                              SCSI_PACK_LOGICAL_UNIT_RESET,
                                              0,
                                              PACK_BUFFER_TEMP);
    scsi_process_packed_data(reservations.sam.command.temp,
                             reservations.sam.command.size,
                             &scsi_status);
    algopr_pr_new_pr(&reservations.sam.command);

    os_sem_wait(&reservations.sam.progress);

    /* now the command is processed */
    os_sem_post(&reservations.sam.new);
}


void scsi_command_submit(TARGET_CMD_T *cmd)
{
    ISCSI_SCSI_CMD_T *args = &(cmd->scsi_cmd);
    unsigned char *cdb = args->cdb;
    lun_t lun = args->lun;
    scsi_command_status_t scsi_status;
    unsigned long long lba;
    uint32_t len;
    bool fua;

    scsi_lun_begin_command(lun, cmd);
    scsi_status.done = 0;

    /* special command */
    if (cdb[0] == INQUIRY)
    {
        /* INQUIRY is always allowed */
        scsi_inquiry(cmd->sess, lun, cdb, cmd->data, &scsi_status);
    }
    else if (cdb[0] == REPORT_LUNS && lun == 0)
    {
        /* FIXME code and comment are wrong, according to spec, when the iscsi
         * command does not require a lun, the lun param should be ignored:
           "Some opcodes operate on a specific Logical Unit. The Logical Unit
           Number (LUN) field identifies which Logical Unit. If the opcode does
           not relate to a Logical Unit, this field is either ignored or may be
           used in an opcode specific way."
           */
        /* REPORT_LUNS can only be done for lun 0*/
        scsi_report_luns(cmd->sess, cdb, cmd->data, &scsi_status);
    }
    /* XXX get_lun_sectors() is used as a validity check here; should add
       and use lun_is_defined() instead. */
    else if (get_lun_sectors(lun) == 0
             || !transport->lun_access_authorized(cmd->sess, lun))
    {
        /* the other commands must be done on valid luns */
        SCSI_STATUS_ERROR(&scsi_status, SCSI_STATUS_CHECK_CONDITION,
                          SCSI_SENSE_ILLEGAL_REQUEST,
                          SCSI_SENSE_ASC_LOGICAL_UNIT_NOT_SUPPORTED);
    }

    if (scsi_status.done == 0)
    {
        os_thread_mutex_lock(&reservations.lock);

        if (!pr_check_rights(reservations.pr_context, lun, cdb,
                             scsi_global_session_id(sess_get_id(cmd->sess))))
           SCSI_STATUS_ERROR(&scsi_status, SCSI_STATUS_RESERVATION_CONFLICT,
                             0, 0);

        os_thread_mutex_unlock(&reservations.lock);
    }

    if (scsi_status.done == 0)
    {
        switch (cdb[0])
        {
        case VERIFY_6:
        case VERIFY_10:
        case VERIFY_16: /* TODO: do the right thing for verfy :
                   * flush the target address to medium
                   * and verify there are no error */
        case TEST_UNIT_READY:
            SCSI_STATUS_OK(&scsi_status, 0);
            break;

        /* SCSIComplianceTest request that.
         *  Just log but process as an unhandled request */
        case REQUEST_SENSE:
        case MODE_SELECT_6:
        case MODE_SELECT_10:
        case RESERVE_10:
        case RELEASE_10:
            SCSI_STATUS_ERROR(&scsi_status,
                              SCSI_STATUS_CHECK_CONDITION,
                              SCSI_SENSE_ILLEGAL_REQUEST,
                              SCSI_SENSE_ASC_INVALID_COMMAND_OPERATION_CODE);
            break;

        case PERSISTENT_RESERVE_OUT:
            /* for persistent reserve out, the additionnal parametters are send
             * like normal data and not in the cdb
             *
             * FIXME: Looks like it seriously infringes the layers isolation as
             *        'cmd' is the iSCSI structure. Another hint is that
             *        SCSI_CDB_MAX_FIXED_LENGTH is not used elsewhere.
             *        The remark also applies to RELEASE_6.
             */
            memmove((char *) cmd->data + SCSI_CDB_MAX_FIXED_LENGTH, cmd->data,
                    cmd->scsi_cmd.length);
            memcpy(cmd->data, cmd->scsi_cmd.cdb, SCSI_CDB_MAX_FIXED_LENGTH);
            /* reserve/release don't need the memcpy, but persistent reserve
             * out need it */
	    algopr_pr_new_pr(cmd);
	    break;

        case RESERVE_6:
        case RELEASE_6:
            memcpy(cmd->data, cmd->scsi_cmd.cdb, SCSI_CDB_MAX_FIXED_LENGTH);
            /* reserve/release don't need the memcpy, but persistent reserve
             * out need it */
            algopr_pr_new_pr(cmd);
            break;

        case PERSISTENT_RESERVE_IN:
            os_thread_mutex_lock(&reservations.lock);
            pr_reserve_in(reservations.pr_context, lun, cdb, cmd->data,
		/* FIXME next computation is really scarry: cannot figure out
		 * the purpose of such a complicated thing */
                sess_get_id(cmd->sess) + CONFIG_TARGET_MAX_SESSIONS * this_node_id,
                &scsi_status);
            os_thread_mutex_unlock(&reservations.lock);
            break;

        case MODE_SENSE_6:
        case MODE_SENSE_10:
            do_mode_sense(lun, cdb, cmd->data, &scsi_status);
            break;

        case  SYNCHRONIZE_CACHE_10:
        case  SYNCHRONIZE_CACHE_16:
            /* XXX the behaviour here does not follow the required one:
             * "If the arguments to 'lba' and 'count' are both zero then all
             * blocks in the cache are synchronized. If 'lba' is greater than
             * zero while 'count' is zero then blocks in the cache whose
             * address is from and including the 'lba' argument to the highest
             * lba on the device are synchronized. If both 'lba' and 'count'
             * are non zero then blocks in the cache whose addresses lie in
             * the range lba_argument to lba_argument+count_argument-1
             * inclusive are synchronized with the medium."
             * In our case, we do a FUA at lba 0 of size 0 and rely and the
             * fact that the VRT implements FUA with a barrier which
             * synchronize the whole disk. */
            prepare_disk_write(cmd, lun, 0, 0, true, &scsi_status);
            break;

        case READ_CAPACITY:
        {
            uint64_t lun_size = 0;
            lun_size = get_lun_sectors(lun);
            if (lun_size > 0xFFFFFFFF)
                set_bigendian32(0xFFFFFFFF, (unsigned char *)cmd->data + 0);
            else
                set_bigendian32(lun_size - 1, (unsigned char *)cmd->data + 0);
            set_bigendian32(512,  (unsigned char *)cmd->data + 4);
            SCSI_STATUS_OK(&scsi_status, 8);
        }
        break;

        case WRITE_6:
            lba = get_bigendian32(cdb) & 0x001fffff;
            len = ((cdb[4] == 0) ? 256 : cdb[4]);
            fua = false;
            prepare_disk_write(cmd, lun, lba, len, fua, &scsi_status);
            break;

        case READ_6:
            lba = get_bigendian32(cdb) & 0x001fffff;
            len = cdb[4];
            prepare_disk_read(cmd, lun, lba, len, &scsi_status);
            break;

        case WRITE_10:
            lba = get_bigendian32(&cdb[2]);
            len = get_bigendian16(&cdb[7]);
            fua = ((cdb[1] & 0x8) != 0) ? true : false;
            prepare_disk_write(cmd, lun, lba, len, fua, &scsi_status);
            break;

        case READ_10:
            lba = get_bigendian32(&cdb[2]);
            len = get_bigendian16(&cdb[7]);
            prepare_disk_read(cmd, lun, lba, len, &scsi_status);
            break;

        case READ_12:
            lba = get_bigendian32(&cdb[2]);
            len = get_bigendian32(&cdb[6]);
            prepare_disk_read(cmd, lun, lba, len, &scsi_status);
            break;

        case WRITE_12:
            lba = get_bigendian32(&cdb[2]);
            len = get_bigendian16(&cdb[6]);
            fua = ((cdb[1] & 0x8) != 0) ? true : false;
            prepare_disk_write(cmd, lun, lba, len, fua, &scsi_status);
            break;

        case READ_16:
            lba = get_bigendian64(&cdb[2]);
            len = get_bigendian32(&cdb[10]);
            prepare_disk_read(cmd, lun, lba, len, &scsi_status);
            break;

        case WRITE_16:
            lba = get_bigendian64(&cdb[2]);
            len = get_bigendian32(&cdb[10]);
            fua = ((cdb[1] & 0x8) != 0) ? true : false;
            prepare_disk_write(cmd, lun, lba, len, fua, &scsi_status);
            break;

        case SERVICE_ACTION_IN_16:
        {
            switch (cdb[1] & 0x1F)
            {
            case READ_CAPACITY_16:
                memset(cmd->data, 0, args->length);
                set_bigendian64(get_lun_sectors(lun) - 1,
				(unsigned char *)cmd->data + 0);
                set_bigendian32(512, (unsigned char *)cmd->data + 8);
                SCSI_STATUS_OK(&scsi_status, MIN(32, get_bigendian32(cdb + 10)));
                break;

            default:
                exalog_error("UNKNOWN OPCODE_16 0x%x (lun %u)", cdb[1],
                             (unsigned) lun);
                SCSI_STATUS_ERROR(&scsi_status,
                                  SCSI_STATUS_CHECK_CONDITION,
                                  SCSI_SENSE_ILLEGAL_REQUEST,
                                  SCSI_SENSE_ASC_INVALID_COMMAND_OPERATION_CODE);
                break;
            }
        }
        break;

        default:
            SCSI_STATUS_ERROR(&scsi_status,
                              SCSI_STATUS_CHECK_CONDITION,
                              SCSI_SENSE_ILLEGAL_REQUEST,
                              SCSI_SENSE_ASC_INVALID_COMMAND_OPERATION_CODE);
            break;
        }
    }

    if (scsi_status.done == 1)
    {
        if (scsi_status.status == SCSI_STATUS_CHECK_CONDITION)
        {
            scsi_status.length_out = add_sense(scsi_status.sense,
                                               scsi_status.asc_ascq,
                                               cmd->data,
                                               MAX_RESPONSE_CDB);
        }
        if ((scsi_status.status != SCSI_STATUS_CHECK_CONDITION) &&
            (scsi_status.status != SCSI_STATUS_GOOD))
        {
            scsi_status.length_out = 0;
        }

        args->status = scsi_status.status;
        args->length =    scsi_status.length_out;

        args->fromdev = 1;
        cmd->data_len = args->length;
        scsi_lun_end_command(lun, cmd);
        cmd->callback(cmd->callback_arg);
    }
}


/**********************
* Internal functions *
**********************/

static void prepare_disk_write(TARGET_CMD_T *cmd,
                               lun_t lun,
                               unsigned long long lba,
                               uint32_t len,
                               bool fua,
                               scsi_command_status_t *scsi_status)
{
    uint64_t sector = BYTES_TO_SECTORS(lba * CONFIG_DISK_BLOCK_LEN_DFLT);

    if (lun_data[lun].size_in_sector < sector + len)
    {
        SCSI_STATUS_ERROR(scsi_status,
                          SCSI_STATUS_CHECK_CONDITION,
                          SCSI_SENSE_ILLEGAL_REQUEST,
                          SCSI_SENSE_ASC_LOGICAL_ADDRESS_OUT_OF_RANGE);
        return;
    }

    cmd->status = COMMAND_WRITE_NEED_WRITE;

    ISCSI_TARGET_PERF_MAKE_WRITE_REQUEST(
        cmd, len * CONFIG_DISK_BLOCK_LEN_DFLT / 1024.);

    /* FIXME fua is deactivated here which is buggy.
     * BTW, lum_export_submit_io implements synchronize cache which is NOT
     * equivalent. This fuzzy buggy behaviour deserve to be fixed. */
    fua = false;
    lum_export_submit_io(lun_data[lun].export,
                         BLOCKDEVICE_IO_WRITE, fua,
                         sector,
                         len * CONFIG_DISK_BLOCK_LEN_DFLT,
                         cmd->data,
                         cmd,
                         disk_end_io);
}


static void prepare_disk_read(TARGET_CMD_T *cmd,
                              lun_t lun,
                              unsigned long long lba,
                              uint32_t len,
                              scsi_command_status_t *scsi_status)
{
    uint64_t sector  = BYTES_TO_SECTORS(lba * CONFIG_DISK_BLOCK_LEN_DFLT);
    uint32_t num_bytes = len * CONFIG_DISK_BLOCK_LEN_DFLT;

    if (lun_data[lun].size_in_sector < sector + len)
    {
        SCSI_STATUS_ERROR(scsi_status,
                          SCSI_STATUS_CHECK_CONDITION,
                          SCSI_SENSE_ILLEGAL_REQUEST,
                          SCSI_SENSE_ASC_LOGICAL_ADDRESS_OUT_OF_RANGE);
        return;
    }

    cmd->status = COMMAND_READ_NEED_READ;

    ISCSI_TARGET_PERF_MAKE_READ_REQUEST(
        cmd, len * CONFIG_DISK_BLOCK_LEN_DFLT / 1024.);

    lum_export_submit_io(lun_data[lun].export,
                         BLOCKDEVICE_IO_READ, false,
                         sector,
                         num_bytes,
                         cmd->data,
                         cmd,
                         disk_end_io);
}


static void lun_worker_proc(void *arg)
{
    TARGET_CMD_T *cmd;

    exalog_as(EXAMSG_ISCSI_ID);

    /* the thread stops thanks to the nbdlist: when deleting a nbd list, all
     * waiters exits with NULL. FIXME maybe some kind of wait timeout would be
     * cleaner. */
    while ((cmd = nbd_list_remove(&g_cmd_queue, NULL, LISTWAIT)) != NULL)
    {
        cmd->scsi_cmd.fromdev = 1;

        switch (cmd->status)
        {
        case COMMAND_READ_FAILED:
            cmd->scsi_cmd.status = SCSI_STATUS_CHECK_CONDITION;
            cmd->scsi_cmd.length =  add_sense(SCSI_SENSE_MEDIUM_ERROR,
                                              SCSI_SENSE_ASC_UNRECOVERED_READ_ERROR,
                                              cmd->data,
                                              MAX_RESPONSE_CDB);
            cmd->data_len = cmd->scsi_cmd.length;
            break;

        case COMMAND_WRITE_FAILED:
            cmd->scsi_cmd.status = SCSI_STATUS_CHECK_CONDITION;
            cmd->scsi_cmd.length =  add_sense(
                SCSI_SENSE_MEDIUM_ERROR,
                SCSI_SENSE_ASC_WRITE_ERROR,
                cmd->data,
                MAX_RESPONSE_CDB);
            cmd->data_len = cmd->scsi_cmd.length;
            break;

        case COMMAND_READ_SUCCESS:
            ISCSI_TARGET_PERF_END_READ_REQUEST(cmd);
            cmd->scsi_cmd.status = SCSI_STATUS_GOOD;
            cmd->scsi_cmd.length = cmd->data_len;
            break;

        case COMMAND_WRITE_SUCCESS:
            ISCSI_TARGET_PERF_END_WRITE_REQUEST(cmd);
            cmd->scsi_cmd.status = SCSI_STATUS_GOOD;
            cmd->scsi_cmd.length = 0;
            break;

        case COMMAND_PERSISTENT_RESERVE_FAILED:
            /* nothing to do */
            break;

        case COMMAND_PERSISTENT_RESERVE_SUCCESS:
            /* nothing to do */
            cmd->scsi_cmd.status = SCSI_STATUS_GOOD;
            cmd->scsi_cmd.length = 0;
            break;

	case COMMAND_ABORT:
	    /* nothing to do */
            break;

        default:
	    exalog_error("Unexpected command status (0x%02x) lun %" PRIlun " tag %d ",
			 cmd->status,  cmd->scsi_cmd.lun,  cmd->scsi_cmd.tag);
           break;
        }

        scsi_lun_end_command(cmd->scsi_cmd.lun, cmd);
        cmd->callback(cmd->callback_arg);
    }
}

static int adapter_set_readahead(const lum_export_t *export, uint32_t readahead)
{
    /* This is not implemented in iscsi and maybe it is not even relevant. */
    exalog_error("Don't know how to set readahead on iSCSI adapter.");
    return -EPERM;
}

static void adapter_set_mship(const exa_nodeset_t *pr_mship)
{
    char pr_mship_str[EXA_MAX_NODES_NUMBER + 1];

    EXA_ASSERT(exa_nodeset_contains(pr_mship, this_node_id));

    exa_nodeset_to_bin(pr_mship, pr_mship_str);
    exalog_debug("setting mship to %s", pr_mship_str);

    algopr_update_client_connections(pr_mship);

    algopr_new_membership(pr_mship);
}

static void adapter_set_peers(const adapter_peers_t *peers)
{
    exalog_debug("setting peers");
    algopr_set_clients(peers->addresses);
}

static void adapter_set_addresses(int num_addrs, const in_addr_t addrs[])
{
    target_set_addresses(num_addrs, addrs);
}

static void adapter_suspend(void)
{
    algopr_suspend();
}

static void adapter_resume(void)
{
    algopr_resume();
}

static int adapter_start_target(void)
{
    return iscsi_start_target();
}

static int adapter_stop_target(void)
{
    return iscsi_stop_target();
}

static target_adapter_t target_adapter =
{
    .static_init                      = adapter_static_init,
    .static_cleanup                   = adapter_static_cleanup,
    .init                             = adapter_init,
    .cleanup                          = adapter_cleanup,
    .signal_new_export                = adapter_signal_new_export,
    .signal_remove_export             = adapter_signal_remove_export,
    .signal_export_update_iqn_filters = adapter_signal_export_update_iqn_filters,
    .export_set_size                  = adapter_export_set_size,
    .export_get_inuse                 = adapter_export_get_inuse,
    .set_readahead                    = adapter_set_readahead,
    .suspend                          = adapter_suspend,
    .resume                           = adapter_resume,
    .set_mship                        = adapter_set_mship,
    .set_peers                        = adapter_set_peers,
    .set_addresses                    = adapter_set_addresses,
    .start_target                     = adapter_start_target,
    .stop_target                      = adapter_stop_target
};

const target_adapter_t * get_iscsi_adapter(void)
{
    return &target_adapter;
}
