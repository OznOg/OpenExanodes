/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXA_BD_USER_BD
#define EXA_BD_USER_BD

#ifdef __KERNEL__

#include "target/linux_bd_target/module/bd_list.h"

#include "target/linux_bd_target/include/bd_user.h"

#include "target/linux_bd_target/module/bd_user_kernel.h"

#include <linux/completion.h>

struct bd_session;

/** must equal to
 * min(include/linux/genhd.h sizeof((stuct genhd*)(NULL)->disk_name ),
 * KOBJ_NAME_LEN (20) and it's arbitrary value in 2.4 */
#define BD_DISK_NAME_SIZE       20
struct bd_minor
{
    struct bd_session    *bd_session;
    struct bd_list        bd_list;
    spinlock_t bd_lock;
    int minor;
    struct gendisk       *bd_gen_disk;
    volatile bool         dead;        /* will be read/write by Bd0 thread,
                                        * but asynchronously read by BdRequest
                                        * so it must be volatile !*/
    char need_sync;
    atomic_t use_count;
    struct bd_minor      *bd_next;

    unsigned char         current_run;               /* number of request of this minoir map in user mode */
};

/* kernel structure associated with each session, one session = 1 major number +
 * 1 vm + 2 semphore user/kernel + buffers */

struct bd_session
{
    struct bd_kernel_queue *bd_kernel_queue;    /*! Address in kernel space */
    struct bd_kernel_queue *bd_kernel_queue_user; /*! Address in user space */
    struct bd_user_queue   *bd_user_queue; /*! Address in kernel space */
    struct bd_user_queue   *bd_user_queue_user; /*! Address in user space */
    long bd_major;                      /*! Major number of the block device of this session */
    struct bd_root_list     bd_root; /*! Element used to store bd_requests */
    struct bd_minor        *bd_minor_last; /*! used to read all queue */
    struct bd_minor        *bd_minor; /*! Link structure of gendisk minor */
    struct bd_event        *bd_new_rq; /*! Event to wake up user if there are some  new Rq */
    struct bd_event        *bd_thread_event; /*! Event to wake up main kernel thread if there are new queue, new Rq done by user, if it's time to end,... */
    struct bd_session      *bd_next_session; /*! Link to next session */
    struct task_struct     *bd_task; /*! main task for  get_user_pages(current, mm, addr, 1, write, 0, &page, NULL); */
    struct page           **bd_unaligned_buf; /*! used to manage the buffer that do not fit in page */

    struct bd_request      *pending_req;
    struct bd_minor        *pending_minor;

    struct completion       bd_end_completion;

    atomic_t     total_use_count;

    int          bd_page_size;
    int          bd_buffer_size;
    int          bd_max_queue;

    int          bd_barrier_enable;

    struct bd_barrillet_queue bd_new_request;
    struct bd_barrillet_queue bd_ack_request;
};

#define bd_queue_num(Q) ((((unsigned long) Q) -\
                          ((unsigned long) Q->bd_session->bd_kernel_queue)) / \
                         sizeof(struct bd_kernel_queue))

void  bd_end_request(struct bd_kernel_queue *,  int err);
int   bd_prepare_request(struct bd_kernel_queue *);
int   bd_register_drv(struct bd_session *);
void  bd_unregister_drv(struct bd_session *);
void  bd_flush_q(struct bd_session *session);
int   bd_minor_add_new(struct bd_session *session, int minor,
                       unsigned long size_in512_bytes, bool readonly);
void bd_put_session(struct bd_session **);

long bd_post_new_rq(struct bd_session *session, struct bd_request *req);

struct bd_session *bd_launch_session(struct bd_init *init);

const char *bd_minor_name(const struct bd_minor *bd_minor);

void  bd_end_q(struct bd_minor *bd_minor, int err);
int   bd_minor_remove(struct bd_minor *bd_minor);
int   bd_minor_set_size(struct bd_minor *bd_minor,
                        unsigned long size_in512_bytes);

#endif

#endif
