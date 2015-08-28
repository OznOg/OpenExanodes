/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "target/iscsi/include/pr_lock_algo.h"

#include <string.h>
#include <stdio.h>

#include "common/include/exa_error.h"
#include "os/include/strlcpy.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_nodeset.h"
#include "common/include/threadonize.h"
#include "common/include/exa_nbd_list.h"
#include "os/include/os_thread.h"
#include "log/include/log.h"

static volatile bool pr_lock_run;

static struct nbd_root_list msg_root;
static struct nbd_list algopr_msg;
static struct nbd_list msg_from_future;

#define new_element() \
    nbd_list_remove(&algopr_msg.root->free, NULL, LISTWAIT)
#define release_element(elt) \
    nbd_list_post(&algopr_msg.root->free, (elt), -1)

typedef enum {
#define ALGOPR_MESSAGE_FIRST ALGOPR_MESSAGE_LOCKSERVER_OK
	ALGOPR_MESSAGE_LOCKSERVER_OK = 10,
	ALGOPR_MESSAGE_PR_CMD,
	ALGOPR_MESSAGE_PR_CMD_DONE,
	ALGOPR_MESSAGE_LOCK,
	ALGOPR_MESSAGE_LOCKED,
	ALGOPR_MESSAGE_UNLOCK,
	ALGOPR_MESSAGE_LOCK_UPDATE_OTHER_NODE
#define ALGOPR_MESSAGE_LAST ALGOPR_MESSAGE_LOCK_UPDATE_OTHER_NODE
} msg_type_t;
#define MESSAGE_TYPE_IS_VALID(t) \
    ((t) >= ALGOPR_MESSAGE_FIRST && (t) <= ALGOPR_MESSAGE_LAST)


#define ALGOPR_THREAD_STACK_SIZE 16384

/* FIXME The size of the algopr_msgnetwork_t structure must be the same under
 * windows and linux because of algopr_network.c implementation which need to
 * know the exact size of this structure.
 * Packing is made necessary for exanodes 5.0.X compatibility. Changing the
 * structure or the packing would make a rolling upgrade impossible. */
#ifdef WIN32
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct
{
    msg_type_t   type;
    bool         from_lockserver;
    exa_nodeid_t from_node_id;
    bool         to_lockserver;
    exa_nodeid_t to_node_id;
    uint16_t     emitter_sequency;
    uint16_t     to_membership_incarnation;
    uint16_t     from_membership_incarnation;
} __attribute__((packed)) algopr_msgnetwork_t;
#ifdef WIN32
#pragma pack(pop)
#endif

typedef struct
{
    /* FIXME Note the wonderful almost copypast with struc above... */
    msg_type_t   type;
    bool         from_lockserver;
    exa_nodeid_t from_node_id;
    bool         to_lockserver;
    exa_nodeid_t to_node_id;
    uint16_t     emitter_sequency;
    int          to_membership_incarnation;
    int          from_membership_incarnation;

    void *data;    /* only valid for ALGOPR_MESSAGE_PR_CMD */
    int data_size;
} algopr_message_t;

typedef struct
{
    enum
    {
	ALGOPR_NEW_MEMBERSHIP = 1,
	ALGOPR_SUSPEND,
	ALGOPR_RESUME,
	ALGOPR_LOCAL_TARGET_NEW_PR,
	ALGOPR_MESSAGE,
    } op;
#define ALGOPR_OPERATION_IS_VALID(op) \
    ((op) >= ALGOPR_NEW_MEMBERSHIP && (op) <= ALGOPR_MESSAGE)

    union
    {
        exa_nodeset_t membership;
        algopr_message_t message;
        struct {
            char ip[EXA_MAXSIZE_NICADDRESS + 1];
            exa_nodeid_t nid;
        } addr;
	void *new_pr_data;
    } u;
} algopr_op_t;

typedef struct
{
    exa_nodeid_t  this_node_id;
    long          membership_incarnation[EXA_MAX_NODES_NUMBER];
    exa_nodeset_t membership;
    exa_nodeid_t  PR_lockserver;
    bool          suspended;
} algopr_shared_t;

typedef struct
{
    exa_nodeset_t     PR_read_membership;
    exa_nodeset_t     PR_read_membership_ok;
    int               PR_read_incarnation_at_beginning;
    int               PR_pending;
    void             *PR_pending_private;
    enum {
	ALGOPR_LOCKCLIENT_STATE_PASSIVE,
	ALGOPR_LOCKCLIENT_STATE_WAIT_PROCESS_REMOTE
    } state;
    struct nbd_list msg_new_pr;
} algopr_lockclient_t;

typedef struct
{
    enum {
	ALGOPR_LOCKSERVER_STATE_STANDBY,
	ALGOPR_LOCKSERVER_STATE_WAIT_ALL_OK,
	ALGOPR_LOCKSERVER_STATE_READY_UNLOCKED,
	ALGOPR_LOCKSERVER_STATE_READY_LOCKED
    } state;
    exa_nodeset_t membership;
    exa_nodeid_t node_with_lock; /* for debug only */
    struct nbd_list msg_lock;
    int msg_lock_nb;
    int best_incarnation_node;
    int best_incarnation_incarnation;
} algopr_lockserver_t;

static struct algopr_thread_args
{
    os_thread_t     tid;
    exa_nodeid_t  nodeid;
} algopr_thread_args;

/* FIXME Is this function really useful ? */
static exa_nodeid_t algopr_get_lockserver_from_membership(const exa_nodeset_t *a)
{
    return exa_nodeset_first(a);
}

/* called by target */
void algopr_pr_new_pr(void *private_data)
{
    algopr_op_t *msg = new_element();
    EXA_ASSERT(msg != NULL);

    msg->op = ALGOPR_LOCAL_TARGET_NEW_PR;
    msg->u.new_pr_data = private_data;
    nbd_list_post(&algopr_msg, msg, -1);
}


void algopr_new_membership(const exa_nodeset_t *membership)
{
    algopr_op_t *msg = new_element();
    EXA_ASSERT(msg != NULL);

    msg->op = ALGOPR_NEW_MEMBERSHIP;
    msg->u.membership = *membership;
    nbd_list_post(&algopr_msg, msg, -1);
}


/* called by algopr_network.c */
void algopr_new_msg(const unsigned char *data, int size, unsigned char *buffer, int bsize)
{
    algopr_op_t *msg = new_element();
    const algopr_msgnetwork_t *msg_data = (algopr_msgnetwork_t *) data;
    /* FIXME remove this copy. this is useless and made necessary by
     * uneeded bigbuffer complexity */

    EXA_ASSERT(msg != NULL);

    msg->op                         = ALGOPR_MESSAGE;

    msg->u.message.type             = msg_data->type;
    msg->u.message.to_lockserver    = msg_data->to_lockserver;
    msg->u.message.from_lockserver  = msg_data->from_lockserver;
    msg->u.message.from_node_id     = msg_data->from_node_id;
    msg->u.message.to_node_id       = msg_data->to_node_id;
    msg->u.message.emitter_sequency = msg_data->emitter_sequency;
    msg->u.message.to_membership_incarnation =
        msg_data->to_membership_incarnation;
    msg->u.message.from_membership_incarnation =
        msg_data->from_membership_incarnation;

    EXA_ASSERT(EXA_NODEID_VALID(msg->u.message.from_node_id));
    EXA_ASSERT(EXA_NODEID_VALID(msg->u.message.to_node_id));

    msg->u.message.data      = buffer;
    msg->u.message.data_size = bsize;

    nbd_list_post(&algopr_msg, msg, -1);
}

static void algopr_update_incarnation_from_membership(
                              long membership_incarnation[EXA_MAX_NODES_NUMBER],
                              const exa_nodeset_t *membership)
{
    exa_nodeid_t node_id;

    for (node_id = 0; node_id < EXA_MAX_NODES_NUMBER; node_id++)
    {
        if (exa_nodeset_contains(membership, node_id))
            membership_incarnation[node_id]++;
        else /* incarnation is reset for nodes down FIXME is that correct ? */
            membership_incarnation[node_id] = 0;
    }
}


/* send functions */
static void algopr_send_back(const algopr_shared_t *shared,
                             const algopr_message_t *msg,
                             msg_type_t msg_type)
{
    algopr_msgnetwork_t msg_temp;

    EXA_ASSERT(MESSAGE_TYPE_IS_VALID(msg_type));

    msg_temp.from_lockserver = msg->to_lockserver;
    msg_temp.to_lockserver   = msg->from_lockserver;
    msg_temp.to_node_id      = msg->from_node_id;
    msg_temp.from_node_id    = msg->to_node_id;
    msg_temp.type            = msg_type;

    msg_temp.to_membership_incarnation =
        shared->membership_incarnation[msg_temp.to_node_id];
    msg_temp.from_membership_incarnation =
        shared->membership_incarnation[shared->this_node_id];
    msg_temp.emitter_sequency = msg->emitter_sequency;

    algopr_send_data(msg_temp.to_node_id, &msg_temp,
                     sizeof(algopr_msgnetwork_t), NULL, 0);
}


/* FIXME where does this 64 comes from ? and why 64 ? */
static uint16_t emitter_sequency = 64;

static void algopr_send_msg(const algopr_shared_t *shared, bool from_lockserver,
                            exa_nodeid_t node_id, bool to_lockserver,
                            msg_type_t msg_type, void *data, int data_size)
{
    algopr_msgnetwork_t msg;

    EXA_ASSERT(MESSAGE_TYPE_IS_VALID(msg_type));

    msg.type             = msg_type;
    msg.emitter_sequency = emitter_sequency;
    msg.from_lockserver  = from_lockserver;
    msg.from_node_id     = shared->this_node_id;
    msg.to_lockserver    = to_lockserver;
    msg.to_node_id       = node_id;

    msg.to_membership_incarnation =
        shared->membership_incarnation[msg.to_node_id];
    msg.from_membership_incarnation =
        shared->membership_incarnation[shared->this_node_id];

    algopr_send_data(node_id, &msg, sizeof(algopr_msgnetwork_t), data, data_size);
}


static void algopr_bcast_msg(const algopr_shared_t *shared,
                             const exa_nodeset_t *membership,
                             msg_type_t msg_type, void *data, int data_size)
{
    exa_nodeid_t node_id;

    exa_nodeset_foreach(membership, node_id)
	algopr_send_msg(shared, false, node_id, false, msg_type,
		        data, data_size);
}

/**
 * Remove all messages coming from nodes that are not anymore part of the
 * membership.
 * This function is certainly buggy because of races on new nodes that may
 * arrive...
 * this is part of workaround for bug #4501 feel free to find a better fix
 *
 * @param[in] new   new membership
 */
static void algopr_update_messages(const exa_nodeset_t *new)
{
    algopr_op_t *msg;

    while ((msg = nbd_list_remove(&algopr_msg, NULL, LISTNOWAIT)) != NULL)
    {
        /* Messages are put in msg_from_future because they cannot be put
         * back in algopr_msg as there is no iterator on nbd_lists....
         * Anyway, all messages (future and current) are being put back
         * in algopr_msg here after */
        nbd_list_post(&msg_from_future, msg, -1);
    }

    /* Put all 'future' messages in current message list. If there are from
     * another future (current + n), then they will be postponed again in main
     * loop */
    while ((msg = nbd_list_remove(&msg_from_future, NULL, LISTNOWAIT)) != NULL)
    {
        /* remove messages comming from a dead node from the list. */
        if (msg->op == ALGOPR_MESSAGE
            && !exa_nodeset_contains(new, msg->u.message.from_node_id))
            release_element(msg);
        else
            nbd_list_post(&algopr_msg, msg, -1);
    }
}

/* algorithm membership incarnation */
static void algopr_shared_update(algopr_shared_t *shared,
	                         const exa_nodeset_t *membership)
{
    EXA_ASSERT(!exa_nodeset_is_empty(membership));

    /* Update membership */
    exa_nodeset_copy(&shared->membership, membership);

    /* choose the new incarnation */
    algopr_update_incarnation_from_membership(shared->
	    membership_incarnation, &shared->membership);

    /* Pick a new node for being the lockserver */
    shared->PR_lockserver = algopr_get_lockserver_from_membership(membership);
}

static void send_to_lockserver(const algopr_shared_t *shared, msg_type_t type)
{
    algopr_send_msg(shared, false, shared->PR_lockserver, true, type, NULL, 0);
}

/**
 * Initialise data used to wait that all nodes have finished processing
 * data given by scsi_pr_write_metadata()
 * In this first phase operation will be done locally
 */
static void algopr_lockclient_process_local(algopr_lockclient_t *lockclient,
                             const algopr_shared_t *shared,
                             bool update_other_node)
{
    void *buffer;
    int size;
    bool this_node_is_alone;
    bool nothing_to_do = true;
    lockclient->PR_read_membership = shared->membership;
    exa_nodeset_del(&lockclient->PR_read_membership, shared->this_node_id);
    this_node_is_alone = exa_nodeset_is_empty(&lockclient->PR_read_membership);

    do {
        if (!update_other_node && lockclient->PR_pending > 0)
        {
            algopr_op_t *msg_temp;
            msg_temp = nbd_list_remove(&lockclient->msg_new_pr, NULL, LISTNOWAIT);

            EXA_ASSERT(msg_temp != NULL);
            EXA_ASSERT(msg_temp->op == ALGOPR_LOCAL_TARGET_NEW_PR);

            lockclient->PR_pending_private = msg_temp->u.new_pr_data;
            nbd_list_post(&lockclient->msg_new_pr.root->free, msg_temp, -1);
            lockclient->PR_pending--;
        }
        else
            lockclient->PR_pending_private = ALGOPR_PRIVATE_DATA;

        size = scsi_pr_write_metadata(lockclient->PR_pending_private, &buffer);

	if (this_node_is_alone && size >= 0)
	    scsi_pr_finished(lockclient->PR_pending_private);

	if (!this_node_is_alone && size >= 0)
        {
	     nothing_to_do = false;
	     break;
	}

	EXA_ASSERT(!update_other_node || size > 0);

	if (update_other_node)
	    update_other_node = false;
    } while (lockclient->PR_pending > 0);

    if (nothing_to_do)
    {
        send_to_lockserver(shared, ALGOPR_MESSAGE_UNLOCK);
        return;
    }

    lockclient->PR_read_incarnation_at_beginning =
        shared->membership_incarnation[shared->this_node_id];
    exa_nodeset_reset(&lockclient->PR_read_membership_ok);

    EXA_ASSERT(size >= 0);

    algopr_bcast_msg(shared, &lockclient->PR_read_membership,
                     ALGOPR_MESSAGE_PR_CMD, buffer, size);
    exa_nodeset_add(&lockclient->PR_read_membership, shared->this_node_id);
    exa_nodeset_add(&lockclient->PR_read_membership_ok, shared->this_node_id);
    lockclient->state = ALGOPR_LOCKCLIENT_STATE_WAIT_PROCESS_REMOTE;
}


/**
 * If an prersistent reservation is in progress, check if this is now finished
 * If the current persistent reservation is finished or if another began, check
 * if we can begin a new one
 * @param pr
 * @param shared
 */
static void algopr_lockclient_process_check_finished(algopr_lockclient_t *lockclient,
	                                             const algopr_shared_t *shared)
{
    EXA_ASSERT(lockclient->state == ALGOPR_LOCKCLIENT_STATE_WAIT_PROCESS_REMOTE);

    /* a persisten reservation is in progress */
    exa_nodeset_intersect(&lockclient->PR_read_membership, &shared->membership);
    exa_nodeset_intersect(&lockclient->PR_read_membership_ok, &shared->membership);

    if (!exa_nodeset_equals(&lockclient->PR_read_membership,
		            &lockclient->PR_read_membership_ok))
        return;

    emitter_sequency++;

    /* all node have received data and processed it */
    scsi_pr_finished(lockclient->PR_pending_private);

    if (shared->membership_incarnation[shared->this_node_id] ==
        lockclient->PR_read_incarnation_at_beginning)
	send_to_lockserver(shared, ALGOPR_MESSAGE_UNLOCK);
    else
	send_to_lockserver(shared, ALGOPR_MESSAGE_LOCKSERVER_OK);

    lockclient->state = ALGOPR_LOCKCLIENT_STATE_PASSIVE;

    /* this persistent reservation is done, but perhaps another one was send
     * by local target */
    if (lockclient->PR_pending > 0)
	send_to_lockserver(shared, ALGOPR_MESSAGE_LOCK);
}

void algopr_lockclient(algopr_lockclient_t *lockclient,
	               const algopr_shared_t *shared,
		       algopr_op_t *msg)
{
    algopr_message_t *message = &msg->u.message;

    EXA_ASSERT(msg->op == ALGOPR_MESSAGE);
    EXA_ASSERT(MESSAGE_TYPE_IS_VALID(message->type));

    switch (lockclient->state)
    {
    case ALGOPR_LOCKCLIENT_STATE_PASSIVE:
	switch(message->type)
	{
	    case ALGOPR_MESSAGE_PR_CMD:
		EXA_ASSERT(message->data_size > 0);
		scsi_pr_read_metadata(msg, message->data, message->data_size);
		algopr_send_back(shared, message, ALGOPR_MESSAGE_PR_CMD_DONE);
		algpr_recv_msg_free(message->data);
		break;

	    case ALGOPR_MESSAGE_LOCKED:
		algopr_lockclient_process_local(lockclient, shared, false);
		break;

	    case ALGOPR_MESSAGE_LOCK_UPDATE_OTHER_NODE:
		algopr_lockclient_process_local(lockclient, shared, true);
		break;

	    case ALGOPR_MESSAGE_LOCKSERVER_OK:
	    case ALGOPR_MESSAGE_PR_CMD_DONE:
	    case ALGOPR_MESSAGE_LOCK:
	    case ALGOPR_MESSAGE_UNLOCK:
		EXA_ASSERT(0);
		break;
	}
	break;

    case ALGOPR_LOCKCLIENT_STATE_WAIT_PROCESS_REMOTE:
	switch(message->type)
	{
	    case ALGOPR_MESSAGE_PR_CMD_DONE:
		EXA_ASSERT(message->emitter_sequency == emitter_sequency);
		exa_nodeset_add(&lockclient->PR_read_membership_ok,
                                message->from_node_id);
		algopr_lockclient_process_check_finished(lockclient, shared);
		break;

	    case ALGOPR_MESSAGE_LOCKSERVER_OK:
	    case ALGOPR_MESSAGE_PR_CMD:
	    case ALGOPR_MESSAGE_LOCK:
	    case ALGOPR_MESSAGE_LOCKED:
	    case ALGOPR_MESSAGE_UNLOCK:
	    case ALGOPR_MESSAGE_LOCK_UPDATE_OTHER_NODE:
		EXA_ASSERT(0);
		break;
	}
	break;
    }
}


static void algopr_lockserver_flush_msg_lock(algopr_lockserver_t *lockserver)
{
    algopr_op_t *msg;

    while (lockserver->msg_lock_nb > 0)
    {
        msg = nbd_list_remove(&lockserver->msg_lock, NULL, LISTWAIT);
        EXA_ASSERT(msg != NULL);

        nbd_list_post(&lockserver->msg_lock.root->free, msg, -1);
        lockserver->msg_lock_nb--;
    }
}


static void algopr_lockserver_add_msg_lock(algopr_lockserver_t *lockserver,
                                           algopr_message_t *msg)
{
    nbd_list_post(&lockserver->msg_lock, msg, -1);
    lockserver->msg_lock_nb++;
}


static algopr_op_t *algopr_lockserver_get_msg_lock(algopr_lockserver_t *lockserver)
{
    algopr_op_t *msg;

    if (lockserver->msg_lock_nb == 0)
        return NULL;

    msg = nbd_list_remove(&lockserver->msg_lock, NULL, LISTWAIT);
    EXA_ASSERT(msg != NULL);

    lockserver->msg_lock_nb--;

    return msg;
}

/* FIXME add const on msg */
static void state_wait_all_ok_handle_msg(algopr_lockserver_t *lockserver,
                                         const algopr_shared_t *shared,
                                         algopr_message_t *msg)
{
    EXA_ASSERT(MESSAGE_TYPE_IS_VALID(msg->type));
    switch (msg->type)
    {
	case ALGOPR_MESSAGE_LOCK:
	    algopr_lockserver_add_msg_lock(lockserver, msg);
	    break;

	case ALGOPR_MESSAGE_LOCKSERVER_OK:
	    if (msg->from_membership_incarnation >
		    lockserver->best_incarnation_incarnation)
	    {
		lockserver->best_incarnation_node = msg->from_node_id;
		lockserver->best_incarnation_incarnation =
		    msg->from_membership_incarnation;
	    }

            EXA_ASSERT_VERBOSE(!exa_nodeset_contains(&lockserver->membership,
                                                     msg->from_node_id),
                               "Node %d already acknoledged membership "
                               EXA_NODESET_FMT, msg->from_node_id,
                               EXA_NODESET_VAL(&shared->membership));
	    exa_nodeset_add(&lockserver->membership, msg->from_node_id);

	    if (!exa_nodeset_equals(&shared->membership,
		                    &lockserver->membership))
            {
		release_element(msg);
		break;
            }

	    lockserver->node_with_lock = lockserver->best_incarnation_node;

	    /* if we find no no nodes with incarnation > -1, it's a  bug */
	    EXA_ASSERT(lockserver->best_incarnation_node != EXA_NODEID_NONE);

	    algopr_send_msg(shared, true, lockserver->best_incarnation_node,
		    false, ALGOPR_MESSAGE_LOCK_UPDATE_OTHER_NODE, NULL, 0);

	    lockserver->state = ALGOPR_LOCKSERVER_STATE_READY_LOCKED;

	    release_element(msg);
	    break;

	case ALGOPR_MESSAGE_PR_CMD:
	case ALGOPR_MESSAGE_PR_CMD_DONE:
	case ALGOPR_MESSAGE_LOCKED:
	case ALGOPR_MESSAGE_UNLOCK:
	case ALGOPR_MESSAGE_LOCK_UPDATE_OTHER_NODE:
	    EXA_ASSERT_VERBOSE(0, "message type %d is not supported in state %d",
		               msg->type, lockserver->state);
	    break;
    }
}

/* FIXME add const on msg (at least...) */
static void state_ready_locked_handle_msg(algopr_lockserver_t *lockserver,
                                          const algopr_shared_t *shared,
                                          algopr_message_t *msg)
{
    EXA_ASSERT(MESSAGE_TYPE_IS_VALID(msg->type));
    switch (msg->type)
    {
	case ALGOPR_MESSAGE_LOCK:
	    algopr_lockserver_add_msg_lock(lockserver, msg);
	    break;

	case ALGOPR_MESSAGE_UNLOCK:
	    {
		algopr_op_t *msg_temp;

                /* Check lock is released by the owner */
                EXA_ASSERT(lockserver->node_with_lock == msg->from_node_id);

                /* If there are waiting lock demands, handle the next one*/
		msg_temp = algopr_lockserver_get_msg_lock(lockserver);
		if (msg_temp != NULL)
		{
		    lockserver->node_with_lock = msg_temp->u.message.from_node_id;
		    algopr_send_back(shared, &msg_temp->u.message, ALGOPR_MESSAGE_LOCKED);
		    lockserver->state = ALGOPR_LOCKSERVER_STATE_READY_LOCKED;
		    release_element(msg_temp);
		}
                else /* no lock request was pending, turn into unlocked state */
                    lockserver->state = ALGOPR_LOCKSERVER_STATE_READY_UNLOCKED;

		release_element(msg);
	    }
	    break;

	case ALGOPR_MESSAGE_LOCKSERVER_OK:
	case ALGOPR_MESSAGE_PR_CMD:
	case ALGOPR_MESSAGE_PR_CMD_DONE:
	case ALGOPR_MESSAGE_LOCKED:
	case ALGOPR_MESSAGE_LOCK_UPDATE_OTHER_NODE:
	    EXA_ASSERT_VERBOSE(0, "message type %d is not supported in state %d",
		               msg->type, lockserver->state);
	    break;
    }
}

/* FIXME add const on msg (at least...) */
static void state_ready_unlocked_handle_msg(algopr_lockserver_t *lockserver,
                                            const algopr_shared_t *shared,
                                            algopr_message_t *msg)
{
    EXA_ASSERT(MESSAGE_TYPE_IS_VALID(msg->type));

    switch (msg->type)
    {
	case ALGOPR_MESSAGE_LOCK:
	    lockserver->node_with_lock = msg->from_node_id;
	    algopr_send_back(shared, msg, ALGOPR_MESSAGE_LOCKED);
	    lockserver->state = ALGOPR_LOCKSERVER_STATE_READY_LOCKED;

	    release_element(msg);
	    break;

	case ALGOPR_MESSAGE_LOCKSERVER_OK:
	case ALGOPR_MESSAGE_PR_CMD:
	case ALGOPR_MESSAGE_PR_CMD_DONE:
	case ALGOPR_MESSAGE_LOCKED:
	case ALGOPR_MESSAGE_UNLOCK:
	case ALGOPR_MESSAGE_LOCK_UPDATE_OTHER_NODE:
	    EXA_ASSERT_VERBOSE(0, "message type %d is not supported in state %d",
		               msg->type, lockserver->state);
	    break;
    }
}

static void algopr_lockserver(algopr_lockserver_t *lockserver,
                              const algopr_shared_t *shared,
                              algopr_op_t *msg)
{
    EXA_ASSERT(msg);

    EXA_ASSERT(msg->op == ALGOPR_MESSAGE);

    switch (lockserver->state)
    {
	case ALGOPR_LOCKSERVER_STATE_STANDBY:
	    EXA_ASSERT(0);
	    break;

	case ALGOPR_LOCKSERVER_STATE_WAIT_ALL_OK:
	    state_wait_all_ok_handle_msg(lockserver, shared, &msg->u.message);
	    break;

	case ALGOPR_LOCKSERVER_STATE_READY_UNLOCKED:
	    state_ready_unlocked_handle_msg(lockserver, shared, &msg->u.message);
	    break;

	case ALGOPR_LOCKSERVER_STATE_READY_LOCKED:
	    state_ready_locked_handle_msg(lockserver, shared, &msg->u.message);
	    break;
    }
}

static void handle_new_membership(algopr_lockserver_t *lockserver,
        algopr_lockclient_t *lockclient,
        algopr_shared_t *shared, const exa_nodeset_t *membership)
{
    /* Server side stuff */
    /* FIXME all theses operation can be done in any state ? */

    algopr_shared_update(shared, membership);

    lockserver->best_incarnation_node = EXA_NODEID_NONE;
    lockserver->best_incarnation_incarnation = -1;

    algopr_lockserver_flush_msg_lock(lockserver);

    exa_nodeset_reset(&lockserver->membership);

    if (shared->PR_lockserver != shared->this_node_id)
	lockserver->state = ALGOPR_LOCKSERVER_STATE_STANDBY;
    else
	lockserver->state = ALGOPR_LOCKSERVER_STATE_WAIT_ALL_OK;

    /* Client side stuff */
    switch (lockclient->state)
    {
	case ALGOPR_LOCKCLIENT_STATE_PASSIVE:
	    /* FIXME no check of if the local node is actaullay inside mship */
	    send_to_lockserver(shared, ALGOPR_MESSAGE_LOCKSERVER_OK);
	    if (lockclient->PR_pending > 0)
		send_to_lockserver(shared, ALGOPR_MESSAGE_LOCK);

	    break;

	case ALGOPR_LOCKCLIENT_STATE_WAIT_PROCESS_REMOTE:
	    algopr_lockclient_process_check_finished(lockclient, shared);
	    break;
    }
}

static void handle_local_target_new_pr(algopr_lockclient_t *lockclient,
				       const algopr_shared_t *shared,
				       algopr_op_t *msg)
{
    /* FIXME a really better way would be to use type checking of the function
     * params in place of asserts, but here we need not to the the msg... */
    EXA_ASSERT(ALGOPR_OPERATION_IS_VALID(msg->op));
    EXA_ASSERT(msg->op == ALGOPR_LOCAL_TARGET_NEW_PR);

    /* There is nothing to do on server side when receiving sush a message,
     * so handle it only on client side. */
    switch (lockclient->state)
    {
	case ALGOPR_LOCKCLIENT_STATE_PASSIVE:
	    nbd_list_post(&lockclient->msg_new_pr, msg, -1);
	    lockclient->PR_pending++;
	    if (lockclient->PR_pending == 1)    /* no LOCK in progress */
		send_to_lockserver(shared, ALGOPR_MESSAGE_LOCK);
	    break;

	case ALGOPR_LOCKCLIENT_STATE_WAIT_PROCESS_REMOTE:
            nbd_list_post(&lockclient->msg_new_pr, msg, -1);
            lockclient->PR_pending++;
            break;
    }
}

void algopr_suspend(void)
{
    algopr_op_t *msg = new_element();
    EXA_ASSERT(msg != NULL);

    algopr_network_suspend();

    msg->op = ALGOPR_SUSPEND;
    nbd_list_post(&algopr_msg, msg, -1);
}

void algopr_resume(void)
{
    algopr_op_t *msg = new_element();
    EXA_ASSERT(msg != NULL);

    algopr_network_resume();

    msg->op = ALGOPR_RESUME;
    nbd_list_post(&algopr_msg, msg, -1);
}

static void handle_message(algopr_lockserver_t *lockserver,
                           algopr_lockclient_t *lockclient,
                           algopr_shared_t *shared,
                           algopr_op_t *msg)
{
    switch (msg->op)
    {
        case ALGOPR_SUSPEND:
            shared->suspended = true;
            break;

        case ALGOPR_RESUME:
            EXA_ASSERT_VERBOSE(false, "Resume not supported when not suspended");
            break;

        case ALGOPR_NEW_MEMBERSHIP:
            EXA_ASSERT_VERBOSE(false, "Membership not supported when not suspended");
            break;

        case ALGOPR_LOCAL_TARGET_NEW_PR:
            handle_local_target_new_pr(lockclient, shared, msg);
            break;

        case ALGOPR_MESSAGE:
            EXA_ASSERT(msg->u.message.to_node_id == shared->this_node_id);
            if (msg->u.message.to_membership_incarnation <
                    shared->membership_incarnation[msg->u.message.from_node_id]
                    && (msg->u.message.from_lockserver
                        || msg->u.message.to_lockserver))
            {
                /* message from the past to an old lockserver or from an old
                 * lockserver */
                release_element(msg);
                break;
            }

            if (msg->u.message.to_membership_incarnation >
                    shared->membership_incarnation[msg->u.message.from_node_id])
            {
                /* message from the future, replay this message later when
                 * future become present
                 * FIXME Why do we only postpone messages of type
                 * ALGOPR_MESSAGE ? why not doing the same for
                 * ALGOPR_LOCAL_TARGET_NEW_PR ? */
                nbd_list_post(&msg_from_future, msg, -1);
                break;
            }

            if (msg->u.message.to_lockserver)
                algopr_lockserver(lockserver, shared, msg);
            else
            {
                algopr_lockclient(lockclient, shared, msg);
                release_element(msg);
            }
            break;
    }
}

static void handle_message_when_suspended(algopr_lockserver_t *lockserver,
                                          algopr_lockclient_t *lockclient,
                                          algopr_shared_t *shared,
                                          algopr_op_t *msg)
{
    switch (msg->op)
    {
        case ALGOPR_NEW_MEMBERSHIP:
            handle_new_membership(lockserver, lockclient, shared,
                                  &msg->u.membership);
            release_element(msg);
            break;

        case ALGOPR_SUSPEND:
            break;

        case ALGOPR_RESUME:
            shared->suspended = false;

            /* New membership was received, update messages list in order
             * to get rid of spurious messages from dead nodes. */
            algopr_update_messages(&shared->membership);

            break;

        case ALGOPR_LOCAL_TARGET_NEW_PR:
        case ALGOPR_MESSAGE:
            /* Postpone this request later, until resume.*/
            nbd_list_post(&msg_from_future, msg, -1);
            break;
    }
}

static void algopr_thread(void *args)
{
    struct algopr_thread_args *thread_args = (struct algopr_thread_args *) args;
    algopr_shared_t shared;
    algopr_lockclient_t lockclient;
    algopr_lockserver_t lockserver;

    exalog_as(EXAMSG_ISCSI_ID);

    /* FIXME memset is NOT an initialization of a structure... */
    memset(&shared, 0, sizeof(algopr_shared_t));
    memset(&lockclient, 0, sizeof(algopr_lockclient_t));
    memset(&lockserver, 0, sizeof(algopr_lockserver_t));

    nbd_init_list(&msg_root, &lockserver.msg_lock);
    nbd_init_list(&msg_root, &lockclient.msg_new_pr);
    nbd_init_list(&msg_root, &msg_from_future);

    shared.this_node_id = thread_args->nodeid;

    /* Init supsended and wait for recovery informations (like mship) */
    shared.suspended = true;

    while (pr_lock_run)
    {
	struct nbd_list *lists_in[1];
	algopr_op_t *msg = NULL;
	bool lists_out = false;

	lists_in[0] = &algopr_msg;
	if (nbd_list_select(lists_in, &lists_out, 1, 1000))
        {
            /* FIXME: using a waiting 'nbd_list_remove' after select is a
             * nonsense */
	    msg = nbd_list_remove(&algopr_msg, NULL, LISTWAIT);
            EXA_ASSERT(msg != NULL);
        }


	/* timeout is reached */
	if (msg == NULL)
	    continue;

	EXA_ASSERT(ALGOPR_OPERATION_IS_VALID(msg->op));

        if (shared.suspended)
            handle_message_when_suspended(&lockserver, &lockclient, &shared, msg);
        else
            handle_message(&lockserver, &lockclient, &shared, msg);
    }
}


/* FIXME well 5000, why not 3 ? */
#define MAX_MESSAGE 5000

int algopr_init(exa_nodeid_t nodeid, int max_buffer_size)
{
    int ret;

    nbd_init_root(MAX_MESSAGE, sizeof(algopr_op_t), &msg_root);
    nbd_init_list(&msg_root, &algopr_msg);

    algopr_thread_args.nodeid = nodeid;
    pr_lock_run = true;

    if (!exathread_create_named(&algopr_thread_args.tid, ALGOPR_THREAD_STACK_SIZE,
                                algopr_thread, (void *) &algopr_thread_args,
                                "algopr_thread"))
	return -EXA_ERR_DEFAULT;

    ret = algopr_init_plugin(nodeid, max_buffer_size + sizeof(algopr_msgnetwork_t));
    if (ret != EXA_SUCCESS)
    {
        pr_lock_run = false;
        os_thread_join(algopr_thread_args.tid);
    }

    return ret;
}


void algopr_cleanup(void)
{
    pr_lock_run = false;
    os_thread_join(algopr_thread_args.tid);
    algopr_close_plugin();
}
