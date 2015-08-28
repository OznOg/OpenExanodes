/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef BD_USER_KERNEL_H
#define BD_USER_KERNEL_H

#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif

/* BdEvent machanism us use to have processing consumption
 * and less deadlock, no more semaphore out of bound, the main idea is :
 * The event "receiver" test and process all possibely cause of event at each event,
 * so it is not needed to add multiple if N event is pendeing ONLY ONE MORE event need
 * to be received by the caller.
 */
#define BD_EVENT_ACK_NEW        1       /* event to signal a new ack by user or a new queue from kernel*/
#define BD_EVENT_KILL           2       /* event to signal that the session will be killed */
#define BD_EVENT_SUSPEND        4       /* some minor is now suspended, do the right thing */
#define BD_EVENT_RESUME         8       /* suspend -> up */
#define BD_EVENT_NEW            16      /* New node */
#define BD_EVENT_DEL            32      /* destroy a node */
#define BD_EVENT_DOWN           64      /* suspend / active -> down */
#define BD_EVENT_UP             128     /* suspend -> up */
#define BD_EVENT_TIMEOUT        512     /* Timeout */
#define BD_EVENT_SETSIZE        1024    /* resizing a minor */
#define BD_EVENT_IS_INUSE       2048    /* test if a minor is in use (opened) */

/* Structure to manage event */
struct bd_event_msg
{
    unsigned long        bd_type;                    /**< type of evenement */
    long long            bd_minor;                   /**< targeted minor */
    bool                 bd_minor_readonly;          /**< used for minor creation in readonly mode */
    uint64_t             bd_minor_size_in512_bytes;  /**< BdMinorSize : used to create minor */
    int                  bd_result;
    struct completion    bd_event_completion;        /**< completion to wait for the end of a message */
    struct bd_event_msg *next;
};

struct bd_event
{
    spinlock_t bd_event_sl;                  /**< Used to protect access to BdEventAnother */
    struct semaphore       bd_event_sem;     /**< Used to up/down a semaphore if necessary */
    int                    bd_event_another; /**< Used to say if a new event is pending */
    int                    bd_event_waiting; /**< Used to say if we waiting on the semaphore */
    unsigned long          bd_type;          /**< type of waiting Event */
    volatile unsigned long bd_event_number;  /**< Used for NewEventWaiting */
    struct bd_event_msg   *bd_old_msg;       /**< last msg processed */
    struct bd_event_msg   *bd_msg;
};

void bd_new_event_msg_wait_processed(struct bd_event *bd_event,
                                     struct bd_event_msg *msg);

void bd_new_event(struct bd_event *bd_event, unsigned long bd_type);
int  bd_wait_event(struct bd_event *bd_event, unsigned long *bd_type,
                   struct bd_event_msg  **bd_event_msg);


#endif
