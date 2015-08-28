/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_constants.h"
#include "common/include/exa_math.h"
#include "os/include/os_network.h"
#include "os/include/os_mem.h"
#include "target/iscsi/include/scsi.h"
#include "target/iscsi/include/scsi_persistent_reservations.h"
#include "target/iscsi/include/target.h"
#include "target/iscsi/include/endianness.h"

/*
 * This is used to give an abstraction between target and persistent reservation
 * command processing, it also give an help to store and retreive data from a
 * persistent storage.
 */

/* access righ */
typedef enum
{
    PR_TYPE_NONE = 0x0 /* only for internal use, not a SCSI value */,
    PR_TYPE_WRITE_EXCLUSIVE = 0x1,
    PR_TYPE_EXCLUSIVE_ACCESS = 0x3,
    PR_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY = 0x5,
    PR_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY = 0x6,
    PR_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS = 0x7,
    PR_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS = 0x8
} pr_type_t;

/* service action */
#define PR_OUT_REGISTER 0
#define PR_OUT_RESERVE 1
#define PR_OUT_RELEASE 2
#define PR_OUT_CLEAR 3
#define PR_OUT_PREEMPT 4
#define PR_OUT_PREEMPT_AND_ABORT 5
#define PR_OUT_REGISTER_AND_IGNORE_EXISTING_KEY 6
#define PR_OUT_REGISTER_AND_MOVE 7

#define PR_IN_READ_KEYS 0
#define PR_IN_READ_RESERVATION 1
#define PR_IN_REPORT_CAPABILITIES 2
#define PR_IN_READ_FULL_STATUS 3

#define PR_IN_READ_REPORT_CAPABILITIES_WR_EX_RO 0x20       /* write exlusive registrants only */
#define PR_IN_READ_REPORT_CAPABILITIES_SIP_C 0x08

/* scope */
#define PR_LU_SCOPE 0

/* This is the number of session id we will use to keep Registration and
 * Reservation of Nexus Loss */
#define PR_NEXUS_LOSS_REGISTRATION_DATA 64

/* FIXME: This constant is not related to the actual max number of nodes of
 * Exanodes. Which means that this code cannot work with more than 32 nodes
 * ... at least when there are many connections by node.
 */
#define WRONG_MAX_NODES 32
#define MAX_GLOBAL_SESSION (WRONG_MAX_NODES * CONFIG_TARGET_MAX_SESSIONS)
/* add the session used to keep registration of lost nexus */
#define MAX_GLOBAL_SESSION_PLUS_EXTRA (MAX_GLOBAL_SESSION + PR_NEXUS_LOSS_REGISTRATION_DATA)

#define MAX_REGISTRATIONS 32
#define SESSION_ID_NONE (MAX_GLOBAL_SESSION_PLUS_EXTRA + 1)

#define SPC2_RESERVE_NONE -1
typedef uint64_t pr_key_t;

/* FIXME: the terms 'session' and 'nexus' are not used consistantly in the
 * code. I don't know what is the good usage.
 */

typedef struct {
    uint32_t   session_id;
    pr_key_t   key;
} pr_registration_t;


/** All the information corresponding to the persistent reserve of a given LUN
 *
 * FIXME: It would be interesting to add a lock to the structre as it can be
 * manipulated as a whole
 */
typedef struct {
    pr_type_t  reservation_type;
    uint32_t   pr_generation;
    uint32_t   spc2_reserve;
    uint32_t   holder_index;
    pr_registration_t registrations[MAX_REGISTRATIONS];
} pr_info_t;

/**
 * This structure contains all the stuff needed to manage the PR for all LUNs
 *
 * FIXME: I name it context because it looks like one IMHO
 */
struct pr_context
{
    pr_send_sense_data_fn_t    send_sense_data;
    /** Array of PR information (by LUN) */
    pr_info_t                  pr_info[MAX_LUNS];

    /* IMHO, the session_id_used boolean is only used to avoid
     * calling the callback when the session has been deleted.
     *
     * FIXME: we should replace this table by a bool 'active' in
     * pr_registration_t
     */
    bool                       session_id_used[MAX_GLOBAL_SESSION_PLUS_EXTRA];
};

static void callback_send_sense_data(const pr_context_t *context, int session_id,
                                     lun_t lun, int status,
                                     int sense, int asc_ascq)
{

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION_PLUS_EXTRA);

    if (session_id > MAX_GLOBAL_SESSION)
    {
        return; /* this session id is a lost nexus */
    }

    context->send_sense_data(session_id, lun, SCSI_STATUS_CHECK_CONDITION,
                             SCSI_SENSE_UNIT_ATTENTION,
                             SCSI_SENSE_ASC_RESERVATIONS_RELEASED);
}


/**
 * Register a session with given key to the PR information corresponding to a LUN
 * If the session was already registred, it only change the key.
 *
 * @param[in] pr_info     The PR information structure
 * @param[in] session_id  ID of the registred session
 * @param[in] key         The key to associate with the session
 *
 * @return a pointer to the registration of NULL if all the registration slots
 * were already used
 */
static pr_registration_t *pr_info_add_registration(pr_info_t *pr_info,
                                                   uint32_t session_id, pr_key_t key)
{
    unsigned int i;
    unsigned int first_free = MAX_REGISTRATIONS;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        uint32_t id = pr_info->registrations[i].session_id;

        if (id == session_id)
        {
            pr_info->registrations[i].key = key;
            return &pr_info->registrations[i];
        }
        else if (id == SESSION_ID_NONE && first_free == MAX_REGISTRATIONS)
            first_free = i;
    }

    if (first_free >= MAX_REGISTRATIONS)
        return NULL;

    pr_info->registrations[first_free].session_id = session_id;
    pr_info->registrations[first_free].key = key;

    return &pr_info->registrations[first_free];
}

/**
 * Move all the registered keys from a session to the other one
 *
 * @param[in] pr_info          The PR information structure
 * @param[in] to_session_id    ID of the registred session
 * @param[in] from_session_id  ID of the registred session
 */
static void pr_info_move_registration(pr_info_t *pr_info,
                                      uint32_t to_session_id,
                                      uint32_t from_session_id)
{
    unsigned int i;

    EXA_ASSERT(to_session_id >= MAX_GLOBAL_SESSION
               && to_session_id < MAX_GLOBAL_SESSION_PLUS_EXTRA
               && from_session_id < MAX_GLOBAL_SESSION);

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        if (pr_info->registrations[i].session_id == from_session_id)
        {
            pr_info->registrations[i].session_id = to_session_id;
            return;
        }
    }
}


/**
 * Remove the registration of a given session
 *
 * @param[in] pr_info     The PR information structure
 * @param[in] session_id  ID of the registred session
 */
static void pr_info_del_registration(pr_info_t *pr_info, uint32_t session_id)
{
    unsigned int i;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        if (pr_info->registrations[i].session_id == session_id)
        {
            pr_info->registrations[i].session_id = SESSION_ID_NONE;
            pr_info->registrations[i].key = 0;
            return;
        }
    }
}

/**
 * Clear all the registrations
 *
 * @param[in] pr_info     The PR information structure
 */
static void pr_info_clear_registrations(pr_info_t *pr_info)
{
    unsigned int i;

    pr_info->holder_index = MAX_REGISTRATIONS;

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        pr_info->registrations[i].session_id = SESSION_ID_NONE;
        pr_info->registrations[i].key = 0;
    }
}


/**
 * Is there at least one registration
 *
 * @param[in] pr_info    The PR information structure
 */
static bool pr_info_has_registrations(const pr_info_t *pr_info)
{
    unsigned int i;

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        if (pr_info->registrations[i].session_id != SESSION_ID_NONE)
            return true;
    }

    return false;
}


static bool pr_info_is_registered(const pr_info_t *pr_info,
                                  uint32_t session_id)
{
    unsigned int i;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION_PLUS_EXTRA);

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        /* FIXME: don't know if the second part of the test is necessary */
        if (pr_info->registrations[i].session_id == session_id
            && pr_info->registrations[i].key != 0)
            return true;
    }

    return false;
}

static pr_key_t pr_info_get_registration_key(const pr_info_t *pr_info,
                                             uint32_t session_id)
{
    unsigned int i;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION_PLUS_EXTRA);

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        if (pr_info->registrations[i].session_id == session_id)
            return pr_info->registrations[i].key;
    }

    return 0;
}

pr_key_t pr_info_get_holder_key(const pr_info_t *pr_info)
{
    EXA_ASSERT(pr_info->holder_index < MAX_REGISTRATIONS);

    return pr_info->registrations[pr_info->holder_index].key;
}

static pr_key_t pr_info_get_holder_id(const pr_info_t *pr_info)
{
    EXA_ASSERT(pr_info->holder_index < MAX_REGISTRATIONS);

    return pr_info->registrations[pr_info->holder_index].session_id;
}

static void pr_info_set_holder(pr_info_t *pr_info, uint32_t session_id)
{
    unsigned int i;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION_PLUS_EXTRA);

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        if (pr_info->registrations[i].session_id == session_id)
        {
            pr_info->holder_index = i;
            return;
        }
    }

    EXA_ASSERT(false);
}


static int check_registration(const pr_context_t *context, int session_id, lun_t lun,
                              pr_key_t reservation_key, int service_action)
{
    const pr_info_t *pr_info = &context->pr_info[lun];
    bool is_registered = pr_info_is_registered(pr_info, session_id);
    pr_key_t current_key = pr_info_get_registration_key(pr_info, session_id);

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    /* are we registered with reservation_key ?
     *
     * FIXME: "is_registered" test added for intel iSCSI test
     */
    if (service_action != PR_OUT_REGISTER_AND_IGNORE_EXISTING_KEY
        && is_registered && current_key != reservation_key)
    {
        exalog_warning("iSCSI PR conflict: "
                       "session %i registration check on LUN %" PRIlun " failed"
                       ", registration key mismatch"
                       " (received %" PRIu64 " / current %" PRIu64 ")",
                       session_id, lun, reservation_key,
                       current_key);
        return SCSI_STATUS_RESERVATION_CONFLICT;
    }

    if (service_action != PR_OUT_REGISTER_AND_IGNORE_EXISTING_KEY
        && service_action != PR_OUT_REGISTER
        && !is_registered)
    {
        exalog_warning("iSCSI PR conflict: "
                       "session %i registration check on LUN %" PRIlun " failed"
                       ", session not registered",
                       session_id, lun);
        return SCSI_STATUS_RESERVATION_CONFLICT;
    }

    return SCSI_STATUS_GOOD;
}


/**
 *  Is this session is a reservation holder  (spc3r23 5.6.9)  ?
 *  @return true if there are the session id is a reservation holder of the lun
 *          false otherwise
 */
static bool persistent_is_holder(const pr_context_t *context,
                                 int session_id, lun_t lun)
{
    const pr_info_t *pr_info = &context->pr_info[lun];

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    switch (pr_info->reservation_type)
    {
    case PR_TYPE_WRITE_EXCLUSIVE:
    case PR_TYPE_EXCLUSIVE_ACCESS:
    case PR_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY:
    case PR_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY:
        return pr_info_get_holder_id(pr_info) == session_id;

    case PR_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS:
    case PR_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS:
        return pr_info_is_registered(pr_info, session_id);

    default:
        return false;   /* TODO: must assert here because we must never add a reservation with a unknown */
    }
    return false;
}


/**
 * Get lun reservation holder key (spc3r23 5.6.9 paragraphe 2
 *
 *  @return 0 if there are no reservation on the lun or the reservation type is
 *  all registrants otherwise give the key of the lun reservation holder if
 *  there are any reservation on it
 */
static pr_key_t get_holder_key(const pr_context_t *context, lun_t lun)
{
    pr_key_t key = 0;
    const pr_info_t *pr_info = &context->pr_info[lun];

    switch (pr_info->reservation_type)
    {
    case PR_TYPE_NONE:
        key = 0;
        break;

    case PR_TYPE_WRITE_EXCLUSIVE:
    case PR_TYPE_EXCLUSIVE_ACCESS:
    case PR_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY:
    case PR_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY:
        key = pr_info_get_holder_key(pr_info);
        break;

    case PR_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS:
    case PR_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS:
        key = 0;
        break;
    }
    return key;
}


/**
 * is there any reservation on this lun ?
 *
 *  @return true if there are a reservation on the lun false otherwise
 */
static bool is_lun_reserved(const pr_context_t *context, lun_t lun)
{
    const pr_info_t *pr_info = &context->pr_info[lun];

    return pr_info->reservation_type != PR_TYPE_NONE;
}


bool pr_check_rights(const pr_context_t *context, lun_t lun,
                     const unsigned char *cdb, int session_id)
{
    bool write = false;
    const pr_info_t *pr_info = &context->pr_info[lun];

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    exalog_debug("iSCSI PR: session %i check reservation rights on "
                 "LUN %" PRIlun, session_id, lun);

    write = cdb[0] == WRITE_6 || cdb[0] == WRITE_10 || cdb[0] == WRITE_12
            || cdb[0] == WRITE_16;

    if  (pr_info->spc2_reserve != SPC2_RESERVE_NONE)
    {
        if (session_id != pr_info->spc2_reserve && cdb[0] != INQUIRY)
        {
            exalog_debug("iSCSI PR: session %i not allowed to do persistent "
                         "reservation on LUN %" PRIlun ", LUN reserved in "
                         "SPC-2 mode (by session %i)", session_id, lun,
                         pr_info->spc2_reserve);
            return false;
        }
        return true; /* we have spc2 reserve on this lun, so no persistent
                      * reservation will apply */
    }

    if (cdb[0] == PERSISTENT_RESERVE_OUT || cdb[0] == PERSISTENT_RESERVE_IN)
        return true; /* FIXME: TODO: add all always accessible command if no
                      * spc2 reserve */

    switch (pr_info->reservation_type)
    {
    case PR_TYPE_NONE: /* no pr key */
         return true;

    case PR_TYPE_WRITE_EXCLUSIVE:
        if (!write)
            return true;
        /* Careful fall through */
    case PR_TYPE_EXCLUSIVE_ACCESS:
        if (persistent_is_holder(context, session_id, lun))
            return true;

        exalog_debug("iSCSI PR: session %i cannot get exclusive access to "
                     "LUN %" PRIlun ", it is not the holder", session_id, lun);
        return false;

    case PR_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY:
        if (!write)
            return true;
        /* Careful fall through */
    case PR_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY:
        if (pr_info_is_registered(pr_info, session_id))
            return true;

        exalog_debug("iSCSI PR: session %i cannot get exclusive access to LUN"
                     " %" PRIlun ", it is not a registrant", session_id, lun);
        return false;

    case PR_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS:
        if (!write)
            return true;
        /* Careful fall through */
    case PR_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS:
        if (pr_info_is_registered(pr_info, session_id))
            return true;

        exalog_debug("iSCSI PR: session %i cannot get exclusive access to LUN"
                     " %" PRIlun ", it is not a registrant", session_id, lun);
        return false;

    }

    exalog_error("iSCSI PR: unexpected registration type %02x to LUN %" PRIlun
                 " (session %i)", pr_info->reservation_type, lun, session_id);
    return false;
}


static bool persistent_reserve_lun(pr_context_t *context,
                                   int session_id, lun_t lun,
                                   pr_key_t reservation_key,
                                   pr_type_t access_type)
{
    pr_info_t *pr_info = &context->pr_info[lun];

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    /* lun have already have a persistent reserve key and we are not a
     * reservation lun holder */
    if (is_lun_reserved(context, lun)
        && !persistent_is_holder(context, session_id, lun))
    {
        exalog_warning("iSCSI PR conflict: session %i cannot change LUN %"
                       PRIlun " reservation, already reserved by another "
                       "session", session_id, lun);
        return false;
    }

     /* lun have already have a persistent reserve key and we are a
      * reservation lun holder and we change access type */
    if (is_lun_reserved(context, lun)
        && persistent_is_holder(context, session_id, lun)
        && pr_info->reservation_type != access_type)
    {
        exalog_warning("iSCSI PR conflict: session %i cannot change LUN %"
                       PRIlun " reservation, reservation type mismatch ("
                       "received %02x / current %02x)", session_id, lun,
                       access_type, pr_info->reservation_type);
        return false;
    }

    pr_info->reservation_type = access_type;
    pr_info_set_holder(pr_info, session_id);

    exalog_debug("iSCSI PR: session %i got a reservation on LUN %" PRIlun,
                 session_id, lun);

    return true;
}


static void persistent_release_lun(pr_context_t *context,
                                   int session_id, lun_t lun,
                                   pr_key_t reservation_key,
                                   int reservation_type,
                                   scsi_command_status_t *scsi_status)
{
    int id;
    pr_info_t *pr_info = &context->pr_info[lun];

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    /* lun already have a persistent reserve key
     * and we are not a reservation lun holder
     */
    if (is_lun_reserved(context, lun)
        && !persistent_is_holder(context, session_id, lun))
    {
        exalog_warning("iSCSI PR conflict: "
                       "session %i cannot release LUN %" PRIlun " reservation"
                       ", it does not hold the reservation",
                       session_id, lun);
        SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_RESERVATION_CONFLICT, 0, 0);
        return;
    }

    /* lun already have a persistent reserve key
     * and we are a reservation lun holder
     */
    if (is_lun_reserved(context, lun)
        && persistent_is_holder(context, session_id, lun)
        && reservation_type != pr_info->reservation_type)
    {
        exalog_warning("iSCSI PR failed: "
                       "session %i cannot release LUN %" PRIlun " reservation"
                       ", reservation type mismatch (received %02x / current %02x)",
                        session_id, lun, reservation_type,
                       pr_info->reservation_type);
        SCSI_STATUS_ERROR(
            scsi_status,
            SCSI_STATUS_CHECK_CONDITION,
            SCSI_SENSE_ILLEGAL_REQUEST,
            SCSI_SENSE_ASC_INVALID_RELEASE_OF_PERSISTENT_RESERVATION);
        return;
    }

    /* now all registrant reservation holder will be unregistered */
    if (pr_info->reservation_type != PR_TYPE_WRITE_EXCLUSIVE
        && pr_info->reservation_type != PR_TYPE_EXCLUSIVE_ACCESS)
    {
        unsigned int i;

        for (i = 0; i < MAX_REGISTRATIONS; i++)
        {
            id = pr_info->registrations[i].session_id;

            /* FIXME: the session that triggers the release dont't get the
             * unregistration callback. I understand that as the session does
             * not need to to be signaled with an events it has triggered.
             */
            if (id == SESSION_ID_NONE || id == session_id)
                continue;

            callback_send_sense_data(context, id, lun, SCSI_STATUS_CHECK_CONDITION,
                                     SCSI_SENSE_UNIT_ATTENTION,
                                     SCSI_SENSE_ASC_RESERVATIONS_RELEASED);
        }
    }

    /* no more reservation */
    pr_info->reservation_type = PR_TYPE_NONE;
    pr_info->holder_index = MAX_REGISTRATIONS;

    exalog_debug("iSCSI PR: session %i released the reservation on LUN %" PRIlun,
                 session_id, lun);
    SCSI_STATUS_OK(scsi_status, 0);
}


static void persistent_preempt_lun(pr_context_t *context,
                                   int session_id, lun_t lun,
                                   pr_key_t reservation_key,
                                   pr_key_t service_action_key,
                                   pr_type_t access_type,
                                   scsi_command_status_t *scsi_status)
{
    pr_info_t *pr_info = &context->pr_info[lun];
    bool all_registrants = false;
    unsigned int i;
    int id;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    /* spc 3 figure 3 pg 80 5.6.10.4.1 */
    if (pr_info->reservation_type == PR_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS
        || pr_info->reservation_type == PR_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS)
    {
        all_registrants = true;
    }

    if (!all_registrants && service_action_key == 0)
    {
        exalog_warning("iSCSI PR failed: "
                       "session %i cannot preempt reservation on LUN %" PRIlun
                       ", operation not allowed",
                       session_id, lun);
        SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_CHECK_CONDITION,
                          SCSI_SENSE_ILLEGAL_REQUEST,
                          SCSI_SENSE_ASC_INVALID_FIELD_IN_PARAMETER_LIST);
        return;
    }

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        pr_key_t key = pr_info->registrations[i].key;
        id = pr_info->registrations[i].session_id;

        /* spc3r20 5.6.10.4.3  paragraphe 2 b) expept I_T nexus used for this command
         *
         * FIXME: I think that this code unregister all the sessions that are
         * not the preempting one and that don't have the right registration key.
         */
        if ((service_action_key == key || service_action_key == 0)
            && id != session_id && id != SESSION_ID_NONE)
        {
            pr_info->registrations[i].key = 0;
            pr_info->registrations[i].session_id = SESSION_ID_NONE;
            callback_send_sense_data(context, id, lun, SCSI_STATUS_CHECK_CONDITION,
                                     SCSI_SENSE_UNIT_ATTENTION, 0);
        }
    }

    exalog_debug("iSCSI PR: session %i preempt on LUN %" PRIlun,
                 session_id, lun);
    SCSI_STATUS_OK(scsi_status, 0);

    /* if there are no reservation on lun, preempt don't put another
     * reservation
     */
    if (!is_lun_reserved(context, lun))
        return;

    if ((all_registrants && service_action_key == 0)
        || (!all_registrants && service_action_key == get_holder_key(context, lun)))
    {
        pr_info_set_holder(pr_info, session_id);
        pr_info->reservation_type = access_type;
    }
}


static void persistent_clear_lun(pr_context_t *context,
                                 int session_id, lun_t lun,
                                 scsi_command_status_t *scsi_status)
{
    pr_info_t *pr_info = &context->pr_info[lun];
    unsigned int i;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    for (i = 0; i < MAX_REGISTRATIONS; i++)
    {
        uint32_t id = pr_info->registrations[i].session_id;

        pr_info->registrations[i].session_id = SESSION_ID_NONE;
        pr_info->registrations[i].key = 0;

        /* FIXME: why not in this case ??? maybe spc 3 3 pg 83 5.6.10.6 */
        if (id != session_id && id != SESSION_ID_NONE)
            callback_send_sense_data(context, id, lun,
                                     SCSI_STATUS_CHECK_CONDITION,
                                     SCSI_SENSE_UNIT_ATTENTION, 0);
    }

    pr_info->reservation_type = PR_TYPE_NONE;
    pr_info->holder_index = MAX_REGISTRATIONS;

    exalog_debug("iSCSI PR: session %i cleared LUN %" PRIlun " reservations",
                 session_id, lun);
    SCSI_STATUS_OK(scsi_status, 0);
}


static void persistent_register_lun(pr_context_t *context,
                                    int session_id, lun_t lun,
                                    pr_key_t service_action_key,
                                    scsi_command_status_t *scsi_status)
{
    int id;
    bool remove_registration = true;
    pr_info_t *pr_info = &context->pr_info[lun];

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    exalog_debug("iSCSI PR: session %i register to LUN %" PRIlun " with key %" PRIu64,
                 session_id, lun, service_action_key);
    SCSI_STATUS_OK(scsi_status, 0);

    if (service_action_key != 0)
    {
        pr_registration_t *registration = pr_info_add_registration(pr_info,
                                                                   session_id,
                                                                   service_action_key);
        /* FIXME: manage the error */
        EXA_ASSERT_VERBOSE(registration != NULL,
                           "No more free registration for LUN %" PRIlun
                           ", (session=%i key=%" PRIu64 ")",
                           lun, session_id, service_action_key);

        return;
    }

    /* spc3r23 5.6.10.3 removing lun reservation */

    if (is_lun_reserved(context, lun))
    {
        if (pr_info->reservation_type == PR_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS
            || pr_info->reservation_type == PR_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS)
        {
            unsigned int i;

            /* if we are not a holder, don't remove reservation */
            remove_registration = persistent_is_holder(context, session_id, lun);

            for (i = 0; i < MAX_REGISTRATIONS; i++)
            {
                id = pr_info->registrations[i].session_id;

                if (id != SESSION_ID_NONE && id != session_id && context->session_id_used[id])
                {
                     /* this registered nexus was not the last one */
                    remove_registration = false;
                }
            }
        }
        else
        {
            remove_registration = false;
            if (persistent_is_holder(context, session_id, lun))
                 remove_registration = true;

            if (pr_info->reservation_type == PR_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY
                || pr_info->reservation_type == PR_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY)
            {
                unsigned int i;
                for (i = 0; i < MAX_REGISTRATIONS; i++)
                {
                    id = pr_info->registrations[i].session_id;

                    if (id != SESSION_ID_NONE && context->session_id_used[id] && id != session_id)
                    {
                        callback_send_sense_data(context, id, lun, SCSI_STATUS_CHECK_CONDITION,
                                                 SCSI_SENSE_UNIT_ATTENTION,
                                                 SCSI_SENSE_ASC_RESERVATIONS_RELEASED);
                    }
                }
            }
        }

        if (remove_registration)
        {
            pr_info->reservation_type = PR_TYPE_NONE;
            pr_info_clear_registrations(pr_info);
        }
    }

    pr_info_del_registration(pr_info, session_id);
}


/* functions exported */
/* we can use spc2 reserve/release on this unit, if no one have a registration key
 * on this unit and there are no reservation key on the LUN
 * (spc2r20 5.5.1page 24  paragraphe 3).
 */
static bool can_use_spc2_reserve(const pr_context_t *context,
                                       lun_t lun, int session_id)
{
    const pr_info_t *pr_info = &context->pr_info[lun];

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    if (is_lun_reserved(context, lun) || pr_info_has_registrations(pr_info))
        return false;

    /* no registration and no reservation on this lun */
    return true;
}


static bool persistent_spc2_reserve_lun(pr_context_t *context,
                                        int session_id, lun_t lun,
                                        scsi_command_status_t *scsi_status)
{
    pr_info_t *pr_info = &context->pr_info[lun];

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    if (!can_use_spc2_reserve(context, lun, session_id))
    {
        exalog_warning("iSCSI PR conflict: session %i cannot get SPC-2 "
                       "reservation on LUN %" PRIlun ", SPC-2 reserve not "
                       "possible on this LUN", session_id, lun);
        return false;
    }

    if (pr_info->spc2_reserve != SPC2_RESERVE_NONE
        && pr_info->spc2_reserve != session_id)
    {
        exalog_warning("iSCSI PR conflict: session %i cannot get SPC-2 "
                       "reservation on LUN %" PRIlun ", LUN already reserved "
                       "by %i", session_id, lun, pr_info->spc2_reserve);
        return false;
    }

    pr_info->spc2_reserve = session_id;

    exalog_debug("iSCSI PR: session %i got a SPC-2 reservation on LUN %"
                 PRIlun, session_id, lun);

    return true;
}


static bool persistent_spc2_release_lun(pr_context_t *context, int session_id,
                                        lun_t lun, scsi_command_status_t *scsi_status)
{
    pr_info_t *pr_info = &context->pr_info[lun];

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    if (!can_use_spc2_reserve(context, lun, session_id))
    {
        exalog_warning("iSCSI PR conflict: session %i cannot release SPC-2 "
                       "reservation on LUN %" PRIlun ", SPC-2 reserve not "
                       "possible on this LUN", session_id, lun);
        return false;
    }

    if (pr_info->spc2_reserve != SPC2_RESERVE_NONE
        && pr_info->spc2_reserve != session_id)
    {
        exalog_warning("iSCSI PR conflict: session %i cannot release SPC-2 "
                       "reservation on LUN %" PRIlun ", LUN already reserved "
                       "by %i", session_id, lun, pr_info->spc2_reserve);
        return false;
    }

    pr_info->spc2_reserve = SPC2_RESERVE_NONE;

    exalog_debug("iSCSI PR: session %i released SPC-2 reservation on LUN %"
                 PRIlun, session_id, lun);
    return true;
}


void pr_reset_lun_reservation(pr_context_t *context, lun_t lun)
{
    pr_info_t *pr_info = &context->pr_info[lun];


    pr_info->spc2_reserve = SPC2_RESERVE_NONE;
}


/**
 * persistent reserve in can be done with local copy of persistent reserve data
 */
void pr_reserve_in(const pr_context_t *context, lun_t lun, const unsigned char *cdb,
                   unsigned char *data_out, int session_id,
                   scsi_command_status_t *scsi_status)
{
    const pr_info_t *pr_info = &context->pr_info[lun];
    int service_action = cdb[1] & 0x1f;
    int alloc_len = get_bigendian16(cdb + 7);
    int i;
    int add_len = 0;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    exalog_debug("iSCSI PR: session %i requests PR information on LUN %" PRIlun,
                 session_id, lun);

    /* session_id = session_id * MAX_ */
    switch (service_action)
    {
    case PR_IN_READ_KEYS:
        set_bigendian32(pr_info->pr_generation, data_out);
        add_len = 0;
        for (i = 0; i < MAX_REGISTRATIONS; i++)
        {
            uint32_t id = pr_info->registrations[i].session_id;
            pr_key_t key = pr_info->registrations[i].key;

            if (id != SESSION_ID_NONE && key != 0 && context->session_id_used[id])
            {
                set_bigendian64(key, data_out + add_len + 8);
                add_len = add_len + 8;
            }
        }
        set_bigendian32(add_len, data_out + 4);
        SCSI_STATUS_OK(scsi_status, MIN(add_len + 8, alloc_len));
        return;

    case PR_IN_READ_RESERVATION:
        set_bigendian32(pr_info->pr_generation, data_out);
        if (!is_lun_reserved(context, lun))
        {
            set_bigendian32(0, data_out + 4);
            SCSI_STATUS_OK(scsi_status, 8);
            return;
        }
        set_bigendian32(16, data_out + 4);
        set_bigendian64(get_holder_key(context, lun), data_out + 8);
        set_bigendian64(0, data_out + 16);
        data_out[21] = (PR_LU_SCOPE << 4) | pr_info->reservation_type;
        SCSI_STATUS_OK(scsi_status, MIN(24, alloc_len));
        return;

    case PR_IN_REPORT_CAPABILITIES:
        set_bigendian16(8, data_out);
        data_out[2] = PR_IN_READ_REPORT_CAPABILITIES_SIP_C;
        data_out[3] = 0;
        set_bigendian16(PR_IN_READ_REPORT_CAPABILITIES_WR_EX_RO,
                        data_out + 4); /* only mode needed by MS */
        set_bigendian16(0, data_out + 6);
        SCSI_STATUS_OK(scsi_status, 8);
        return;

    default:
        exalog_error("iSCSI PR failed: "
                     "unexpected operation (%i) from session %i on LUN %" PRIlun,
                     service_action, session_id, lun);
        SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_CHECK_CONDITION,
                          SCSI_SENSE_ILLEGAL_REQUEST, 0);
        return;
    }
}


void pr_reserve_out(pr_context_t *context, lun_t lun,
                    const unsigned char *cdb, int session_id,
                    scsi_command_status_t *scsi_status)
{
    pr_info_t *pr_info = &context->pr_info[lun];

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    if (cdb[0] == RESERVE_6)
    {
       if (persistent_spc2_reserve_lun(context, session_id, lun, scsi_status))
           SCSI_STATUS_OK(scsi_status, 0);
       else
           SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_RESERVATION_CONFLICT, 0, 0);
       return;
    }

    if (cdb[0] == RELEASE_6)
    {
        if (persistent_spc2_release_lun(context, session_id, lun, scsi_status))
            SCSI_STATUS_OK(scsi_status, 0);
        else
            SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_RESERVATION_CONFLICT, 0, 0);
        return;
    }

    if (cdb[0] == PERSISTENT_RESERVE_OUT)
    {
        /* int len = get_bigendian32(cdb + 5); // FIXME: TODO: must be checked for all service action */
        int pr_scope = cdb[2] >> 4;
        int pr_type = cdb[2] & 0xf;
        const unsigned char *param = cdb + SCSI_CDB_MAX_FIXED_LENGTH;

        pr_key_t reservation_key = get_bigendian64(param);
        pr_key_t service_action_key = get_bigendian64(param + 8);
        unsigned char flags = param[20];
        unsigned char spec_i_pt = (flags >> 3) & 1;
        /*    unsigned char all_tg_pt = (flags >>2) & 1 ; */
        int service_action = cdb[1] & 0x1f;

        /* spc3r23 table 116 "allowed scope" */
        if (pr_scope != PR_LU_SCOPE
            && service_action != PR_OUT_REGISTER
            && service_action != PR_OUT_REGISTER_AND_IGNORE_EXISTING_KEY
            && service_action != PR_OUT_CLEAR)
        {
            exalog_error("iSCSI PR failed: "
                         "reserve operation (%i) from session %i on LUN %" PRIlun
                         " is not allowed in this scope",
                         service_action, session_id, lun);
            SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_CHECK_CONDITION,
                              SCSI_SENSE_ILLEGAL_REQUEST,
                              SCSI_SENSE_ASC_INVALID_FIELD_IN_CDB);
            return;
        }

        if (check_registration(context, session_id, lun, reservation_key,
                               service_action) != SCSI_STATUS_GOOD)
        {
            SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_RESERVATION_CONFLICT, 0, 0);
            return;
        }

        switch (service_action)
        {
        case PR_OUT_REGISTER:
            if (spec_i_pt == 0)
            {
                persistent_register_lun(context, session_id,
                                        lun, service_action_key, scsi_status);
                break;
            }
            /* spec_i_pt == 1 ! */
            if (reservation_key != 0)   /* spec_i_pt == 1 and we are on a registered nexus */
            {
                exalog_warning("iSCSI PR conflict: "
                               "reserve operation (%i) from session %i on LUN %" PRIlun
                               " is not allowed, already registered nexus %" PRIu64,
                               service_action, session_id, lun, reservation_key);
                SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_CHECK_CONDITION,
                                  SCSI_SENSE_ILLEGAL_REQUEST,
                                  SCSI_SENSE_ASC_INVALID_FIELD_IN_CDB);
                break;
                ;
            }

            persistent_register_lun(context, session_id, lun,
                                    service_action_key, scsi_status);
            break;

        case PR_OUT_REGISTER_AND_IGNORE_EXISTING_KEY:
            if (service_action_key == 0
                && !pr_info_is_registered(pr_info, session_id))
            {
                /* FIXME: is this a "nothing to do" case ? */
                exalog_debug("iSCSI PR: register and ignore existing keys"
                             " from session %i on LUN %" PRIlun " (ntd)",
                             session_id, lun);

                SCSI_STATUS_OK(scsi_status, 0);
                break;
            }

            if (spec_i_pt == 1)
            {
                exalog_warning("iSCSI PR failed: "
                               "reserve operation (%i) from session %i on LUN %" PRIlun
                               " is not allowed",
                               service_action, session_id, lun);
                SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_CHECK_CONDITION,
                                  SCSI_SENSE_ILLEGAL_REQUEST,
                                  SCSI_SENSE_ASC_INVALID_FIELD_IN_CDB);
                break;
            }
            persistent_register_lun(context, session_id, lun,
                                    service_action_key, scsi_status);
            break;

        case PR_OUT_RESERVE:
            if (persistent_reserve_lun(context, session_id, lun,
                                       reservation_key, pr_type))
                SCSI_STATUS_OK(scsi_status, 0);
            else
                SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_RESERVATION_CONFLICT,
                                  0, 0);
            break;

        case PR_OUT_RELEASE:
            persistent_release_lun(context, session_id, lun,
                                   reservation_key, pr_type, scsi_status);
            break;

        case PR_OUT_CLEAR:
            persistent_clear_lun(context, session_id, lun, scsi_status);
            break;

        case PR_OUT_PREEMPT:
            persistent_preempt_lun(context, session_id, lun,
                                   reservation_key, service_action_key,
                                   pr_type, scsi_status);
            break;

        case PR_OUT_PREEMPT_AND_ABORT:
            persistent_preempt_lun(context, session_id, lun,
                                   reservation_key, service_action_key,
                                   pr_type, scsi_status);
            break;
            /* FIXME : we must add abort task set and it's not done spc3 - 5.6.10.5 */

        case PR_OUT_REGISTER_AND_MOVE:
            /* FIXME : not implemented ,not needed for windows */
            exalog_error("iSCSI PR failed: "
                         "reserve operation 'register_and_move' not supported"
                         " (from session %i on LUN %" PRIlun ")",
                         session_id, lun);
            SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_CHECK_CONDITION,
                              SCSI_SENSE_ILLEGAL_REQUEST, 0);
            break;

        default:
            exalog_error("iSCSI PR failed: "
                         "reserve operation (%i) not expected"
                         " (from session %i on LUN %" PRIlun ")",
                         service_action, session_id, lun);
            SCSI_STATUS_ERROR(scsi_status, SCSI_STATUS_CHECK_CONDITION,
                              SCSI_SENSE_ILLEGAL_REQUEST, 0);
            break;
        }

        /* PR generation sp3r23 6.11.2 */
        if (scsi_status->status == SCSI_STATUS_GOOD
            && service_action != PR_OUT_RESERVE
            && service_action != PR_OUT_RELEASE)
        {
            pr_info->pr_generation++;
        }
    }
}


pr_context_t *pr_context_alloc(pr_send_sense_data_fn_t send_sense_data)
{
    pr_context_t *context;
    lun_t lun;

    context = os_malloc(sizeof(pr_context_t));
    if (context == NULL)
        return NULL;

    memset(context, 0, sizeof(pr_context_t));

    context->send_sense_data = send_sense_data;

    for (lun = 0; lun < MAX_LUNS; lun++)
    {
        pr_info_t *pr_info = &context->pr_info[lun];

        pr_info->spc2_reserve = SPC2_RESERVE_NONE;
        pr_info->reservation_type = PR_TYPE_NONE;
        pr_info_clear_registrations(pr_info);
    }

    return context;
}


void __pr_context_free(pr_context_t *context)
{
    os_free(context);
}

/* data need for packing */
int pr_context_packed_size(const pr_context_t *context)
{
    return sizeof(context->pr_info) + sizeof(context->session_id_used);
}

/* packing in a buffer */
int pr_context_pack(const pr_context_t *context, void *packed_buffer, int size)
{
    EXA_ASSERT(size >= pr_context_packed_size(context));

    memcpy(packed_buffer, context->pr_info, sizeof(context->pr_info));
    memcpy((char *)packed_buffer + sizeof(context->pr_info), context->session_id_used,
           sizeof(context->session_id_used));

    return 0;
}


/* unpacking a buffer */
int pr_context_unpack(pr_context_t *context, const void *packed_buffer, int size)
{
    EXA_ASSERT(size >= pr_context_packed_size(context));

    memcpy(context->pr_info, packed_buffer, sizeof(context->pr_info));
    memcpy(context->session_id_used, (char *)packed_buffer + sizeof(context->pr_info),
           sizeof(context->session_id_used));

    return 0;
}


/* adding a new session from a target */
void pr_add_session(pr_context_t *context, int session_id)
{
    lun_t lun;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    for (lun = 0; lun < MAX_LUNS; lun++)
    {
        pr_info_del_registration(&context->pr_info[lun], session_id);
    }

    context->session_id_used[session_id] = true;
}


/**
 * Is there any registration (key !=0) to one of the lun ?
 * If not, there are no persistent reserve data to keep for this nodes
 *  @return true or false
 */
static bool session_has_reserve_data(const pr_context_t *context, int session_id)
{
    lun_t lun;
    bool pending_registration = false;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION_PLUS_EXTRA);

    if (!context->session_id_used[session_id])
        return false;

    for (lun = 0; lun < MAX_LUNS; lun++)
    {
        const pr_info_t *pr_info = &context->pr_info[lun];

        if (pr_info_is_registered(pr_info, session_id))
            pending_registration = false;
    }

    return pending_registration;
}


static void persistent_reserve_move_session(pr_context_t *context,
                                            int from_session_id,
                                            int to_session_id)
{
    lun_t lun;

    EXA_ASSERT(from_session_id < MAX_GLOBAL_SESSION);
    EXA_ASSERT(to_session_id < MAX_GLOBAL_SESSION_PLUS_EXTRA);

    context->session_id_used[to_session_id] =
        context->session_id_used[from_session_id];
    context->session_id_used[from_session_id] = false;

    for (lun = 0; lun < MAX_LUNS; lun++)
    {
        pr_info_t *pr_info = &context->pr_info[lun];

        pr_info_move_registration(pr_info, to_session_id, from_session_id);

        if (pr_info->spc2_reserve == from_session_id)
            pr_info->spc2_reserve = to_session_id;
    }
}


/* removing a session from a target */
void pr_del_session(pr_context_t *context, int session_id)
{
    int free_session_id;

    EXA_ASSERT(session_id < MAX_GLOBAL_SESSION);

    /* spc3r20 5.6.4.1 say we must keep registration/reservation of lost nexus
     * so we will keep this data if needed but with another internal session_id
     */

    /* first check if there are pending registration or reservation associated
     * with this session */
    if (!session_has_reserve_data(context, session_id))
    {
        context->session_id_used[session_id] = false;
        return;
    }

    /* find a free session id */
    for (free_session_id = MAX_GLOBAL_SESSION;
         free_session_id < MAX_GLOBAL_SESSION_PLUS_EXTRA;
         free_session_id++)
    {
        if (!session_has_reserve_data(context, free_session_id))
            break;
    }

    /* we must do the right job to avoid this problem, and not simply return */
    if (free_session_id >= MAX_GLOBAL_SESSION_PLUS_EXTRA)
        return;

    persistent_reserve_move_session(context, session_id, free_session_id);
}

