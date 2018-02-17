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
#include <linux/version.h>
#include <net/sock.h>

#include "common/lib/exa_common_kernel.h"
#include "common/lib/exa_socket_kernel.h"

struct socket *exa_getsock(int fd)
{
    struct file *file;
    struct inode *inode;
    struct socket *sock;

    if (!(file = fget(fd)))
        return NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
    inode = file->f_dentry->d_inode;
#else
    inode = file->f_path.dentry->d_inode;
#endif
    sock = SOCKET_I(inode);
    if (!S_ISSOCK(inode->i_mode))
    {
        fput(file);
        return NULL;
    }

    if (sock->sk == NULL)
    {
        printk("Error: noio called before connect/bind\n");
        fput(file);
        return NULL;
    }

    return sock;
}


/**
 * Set the allocation mode of a socket to GFP_ATOMIC instead of
 * GFP_KERNEL/GFP_USER
 *
 * @param[in] fd:  user file descriptor of a socket
 */

int exa_socket_set_atomic_kernel(int fd)
{
    struct socket *socket = exa_getsock(fd);

    if (socket == NULL)
        return -ENOTSOCK;

    socket->sk->sk_allocation = GFP_ATOMIC;
    fput(socket->file);

    return 0;
}

