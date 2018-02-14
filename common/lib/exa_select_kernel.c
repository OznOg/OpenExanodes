/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <linux/mm.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <net/sock.h>
#include <linux/poll.h>

#include "common/lib/exa_common_kernel.h"
#include "common/lib/exa_select_kernel.h"
#include "common/lib/exa_socket_kernel.h"

#define SELECT_TIMEOUT (HZ/2)

#define	FD_NFDBITS	(sizeof (long) * 8)

#define	__FD_SET(__n, __p)	((__p)->fds_bits[(__n)/FD_NFDBITS] |= \
				    (1ul << ((__n) % FD_NFDBITS)))

#define	__FD_CLR(__n, __p)	((__p)->fds_bits[(__n)/FD_NFDBITS] &= \
				    ~(1ul << ((__n) % FD_NFDBITS)))

#define	__FD_ISSET(__n, __p)	(((__p)->fds_bits[(__n)/FD_NFDBITS] & \
				    (1ul << ((__n) % FD_NFDBITS))) != 0l)

#define	__FD_ZERO(__p)	memset((void *)(__p), 0, sizeof (*(__p)))

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,13,0)
typedef wait_queue_entry_t wait_queue_t;
#endif

struct exa_select_elt
{
    struct socket *socket;
    wait_queue_entry_t wait;
};

struct exa_select
{
    fd_set select;
    fd_set result;
    int operation; /* EXA_SELECT_IN or EXA_SELECT_OUT */
    struct exa_select_elt elt[__FD_SETSIZE];
};


/**
 * query if there are some data pending on the socket
 * @param sock socket
 * @return 0 no data pending
 *         1 one or more byte pending in the receive buffer of this socket
 */

static int sock_readable(struct socket *sock)
{
    int flag = (*sock->file->f_op->poll)(sock->file, NULL);

    /* A readable device has POLLRDNORM | POLLIN up (according to Linux
     * Device Drivers page 164) */
    if ((flag & (POLLRDNORM | POLLIN)) == (POLLRDNORM | POLLIN))
        return 1;

    /* A device with POLLERR (error condition) or POLLHUP (EOF) will be
     * readable without blocking. (LDD page 164)
     */
    if ((flag & (POLLHUP | POLLERR)) != 0)
        return 1;

    return 0;
}


static int sock_writable(struct socket *sock)
{
    int flag = (*sock->file->f_op->poll)(sock->file, NULL);

    /* If we got HUP, writing would block, we must NOT signal that writing
     * is possible. (LDD page 164) */
    if ((flag & POLLHUP) == POLLHUP)
    {
        /* REMOVE_ME */
        printk(KERN_INFO "Debug SIGBUS: sock got POLLHUP (flag %x)\n", flag);
        printk(KERN_INFO "Prior to this patch we'd have returned %d\n",
                (flag & (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR)));
        return 0;
    }
    /* A writable device has POLLWRNORM | POLLOUT. (LDD page 164) */
    if ((flag & (POLLWRNORM | POLLOUT)) == (POLLWRNORM | POLLOUT))
        return 1;

    /* A device with POLLERR (error condition) will be
     * writable without blocking. (LDD page 164)
     */
    if ((flag & POLLERR) == POLLERR)
        return 1;

    return 0;
}

static void set_callbacks(struct socket *socket, struct exa_select_elt *elt)
{
    struct sock * sk=socket->sk;

    init_waitqueue_entry(&elt->wait,current);
    add_wait_queue(sk_sleep(sk), &elt->wait);
}


static void restore_callbacks(struct socket *socket,struct exa_select_elt *elt)
{
    struct sock * sk=socket->sk;

    remove_wait_queue(sk_sleep(sk), &elt->wait);
}


/**
 * Like select, but only on socket and without any memory allocation, the
 * result is in result[]. Like select, an signal can break it.
 * If operation==EXA_SELECT_IN it we will wait for incoming data. Otherwise,
 * if operation==EXA_SELECT_OUT it we will wait for successful outcoming data.
 *
 * @param sel struct with elt select[] initialized with the correct socket
 *            file descriptor to watch
 *
 * @return -EFAULT : ended, but no data to read on socket
 *         -EINVAL : one or more file descriptor to watch is not a socket,
 *                   but result[] have been updated
 *         0       : at least one operation occur, result[] have been set to
 *                   the change file descriptor
 */

static int exa_select(struct exa_select *sel)
{
    int i;
    int one=0;
    int ret = -EFAULT;

    /* first phase register on each queue */
    for (i = 0; i < __FD_SETSIZE; i++)
    {
        __FD_CLR(i, &sel->result);
        if (__FD_ISSET(i, &sel->select))
        {
            sel->elt[i].socket = exa_getsock(i);
            if (sel->elt[i].socket == NULL)
            {
                ret  = -EINVAL;
                continue;
            }

            set_callbacks(sel->elt[i].socket, &sel->elt[i]);
            if (sel->operation == EXA_SELECT_IN)
            {
                if (sock_readable(sel->elt[i].socket) == 1)
                one = 1;
            }

            if (sel->operation == EXA_SELECT_OUT)
                if (sock_writable(sel->elt[i].socket) == 1)
                    one = 1;
        }
    }

    /* second phase : check if nothing arrived and wait if nothing arrived */
    if (one==0)
    {
        int timeout = SELECT_TIMEOUT ;

        set_current_state(TASK_INTERRUPTIBLE);
        for (i = 0; i < __FD_SETSIZE; i++)
        {
            if (__FD_ISSET(i, &sel->select) && (sel->elt[i].socket != NULL))
            {
                if (sel->operation == EXA_SELECT_IN)
                {
                    if (sock_readable(sel->elt[i].socket) == 1)
                    one = 1;
                }

                if (sel->operation == EXA_SELECT_OUT)
                {
                    if (sock_writable(sel->elt[i].socket) == 1)
                    one = 1;
                }
            }
        }

        if (one == 0) /* if some data already pending, we must not wait (or some race can occur)*/
            timeout = schedule_timeout(timeout);

        set_current_state(TASK_RUNNING);
    }

    /* third : find wich socket receive/sent something */
    for (i = __FD_SETSIZE - 1; i >= 0; i--)
    {
        if (__FD_ISSET(i, &sel->select))
        {
            if (sel->elt[i].socket == NULL)
                continue;
            if (sel->operation == EXA_SELECT_IN)
            {
                if (sock_readable(sel->elt[i].socket) == 1)
                __FD_SET(i, &sel->result);
            }

            if (sel->operation == EXA_SELECT_OUT)
            {
                if (sock_writable(sel->elt[i].socket) == 1)
                __FD_SET(i, &sel->result);
            }

            if ((__FD_ISSET(i, &sel->result)) && (ret == -EFAULT))
                ret = 0;

            restore_callbacks(sel->elt[i].socket, &sel->elt[i]);
            fput(sel->elt[i].socket->file);
            sel->elt[i].socket = NULL;
        }
    }

    /* XXX this is not an error, -EFAULT is used here as the timeout return
     * value....
     * FIXME use ETIME to have an explicit timeout. */
    if (ret == -EFAULT)
        __FD_ZERO(&sel->result);

    return ret;
}


int exa_select_alloc(struct file *filp)
{
    if (filp->private_data != NULL)
        return 0;

    filp->private_data = vmalloc(sizeof(struct exa_select));
    if (filp->private_data == NULL)
        return -ENOMEM;

    return 0;
}


void exa_select_free(struct file *filp)
{
    if (filp->private_data != NULL)
    {
        vfree(filp->private_data);
        filp->private_data = NULL;
    }
}


int exa_select_kernel(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct exa_select *sfs = (struct exa_select *)filp->private_data;
    int ret, ret2;

    if (sfs == NULL)
        return -ENOMEM;

    sfs->operation = cmd;
    ret = copy_from_user(&sfs->select, (fd_set *)arg, sizeof(fd_set));
    if (ret)
        return -EFAULT;

    ret = exa_select(sfs);

    ret2 = copy_to_user((fd_set *)arg, &sfs->result, sizeof(fd_set));
    if (ret2 != 0)
        printk("Error while copying to user space: %d", ret2);

    return ret != 0 ? ret : ret2;
}
