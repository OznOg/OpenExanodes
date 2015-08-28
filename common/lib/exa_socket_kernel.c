/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <asm/uaccess.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <net/sock.h>

#include "common/lib/exa_common_kernel.h"
#include "common/lib/exa_socket_kernel.h"

#define MAX_ADDITIONAL_MIN_FREE_SIZE 32768
#define READ_WRITE_BUFFER_SIZE 64


struct socket *exa_getsock(int fd)
{
    struct file *file;
    struct inode *inode;
    struct socket *sock;

    if (!(file = fget(fd)))
        return NULL;

    inode = file->f_dentry->d_inode;
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


/**
 * return emergency memory pool size (used for GFP_ATOMIC).
 * this is the number given by cat /proc/sys/vm/min_free_size
 */

static int get_min_free_size(void)
{
    struct file * file;
    char buffer[READ_WRITE_BUFFER_SIZE];
    int pos = 0;
    int number = 0;
    int ret;
    mm_segment_t oldfs;
    file = filp_open("/proc/sys/vm/min_free_kbytes",O_RDONLY,0);
    if (file == NULL)
        return 0;
    file->f_pos = 0;
    if (file->f_op->read)
    {
        oldfs = get_fs();
        set_fs(KERNEL_DS);
        ret = file->f_op->read(file, buffer, READ_WRITE_BUFFER_SIZE, &file->f_pos);
        if (ret > 0)
        {
            set_fs(oldfs);
            while ((pos < ret) && ((buffer[pos]<'0') || (buffer[pos]>'9')))
                pos++;
            while ((pos < ret) && (buffer[pos]>='0') && (buffer[pos]<='9'))
            {
                number = number * 10 + buffer[pos] - '0';
                pos++;
            }
        }
    }
    filp_close(file, NULL);
    return number;
}


/**
 * setting  emergency memory pool size (used for GFP_ATOMIC).
 * echo number > /proc/sys/vm/min_free_size
 */

static void set_min_free_size(int number)
{
    struct file * file;
    char buffer[READ_WRITE_BUFFER_SIZE];
    int pos = 0;
    int unit = 1000000000;
    int ret;
    mm_segment_t oldfs;
    if ((number > unit) || (number <= 0))
        return;
    file = filp_open("/proc/sys/vm/min_free_kbytes",O_RDWR,0);
    if (file == NULL)
        return;
    file->f_pos = 0;
    if (file->f_op->write)
    {
        while (unit > number)
            unit = unit / 10;
        while ((unit > 0) && (pos < READ_WRITE_BUFFER_SIZE - 1))
        {
            buffer[pos] = (number / unit) + '0';
            number = number - (number / unit) * unit;
            pos++;
            unit = unit / 10;
        }
        buffer[pos] = 0;
        pos++;
        oldfs = get_fs();
        set_fs(KERNEL_DS);
        ret = file->f_op->write(file, &buffer[0], pos, &file->f_pos);
        set_fs(oldfs);
    }
    filp_close(file, NULL);
}


/**
 * Set an additional amount to emergency pool size. A new call to this function
 * cancels the previous one. For example:
 *  - initial value: min_free_size_kb = 8M (additionnal = 0)
 *  - call to set_additional_min_free_size(4M) -> min_free_size_kb = 12M (additionnal = 4M)
 *  - call to set_additional_min_free_size(8M) -> min_free_size_kb = 16M (additionnal = 8M)
 *  - the user manually set min_free_size_kb to 20M (additionnal remains 8M)
 *  - call to set_additional_min_free_size(0) -> min_free_size_kb = 12M (additionnal = 0)
 */

void exa_socket_tweak_emergency_pool_kernel(int size)
{
    static int additional_min_free_size = 0;
    int old_min_free_size;
    int new_min_free_size;

    if (size > MAX_ADDITIONAL_MIN_FREE_SIZE)
        size = MAX_ADDITIONAL_MIN_FREE_SIZE;

    old_min_free_size = get_min_free_size();
    new_min_free_size = old_min_free_size - additional_min_free_size + size;

    set_min_free_size(new_min_free_size);

    additional_min_free_size = size;
}
