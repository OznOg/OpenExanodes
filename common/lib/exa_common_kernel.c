/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <linux/fs.h>
#include <linux/module.h>

#include "common/lib/exa_common_kernel.h"
#include "common/lib/exa_select_kernel.h"
#include "common/lib/exa_socket_kernel.h"


static int exa_common_major;


static int exa_common_open(struct inode *inode, struct file *filp)
{
    return 0;
}


static int exa_common_release(struct inode *inode, struct file *filp)
{
    exa_select_free(filp);

    return 0;
}

#if HAVE_UNLOCKED_IOCTL
static long exa_common_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int exa_common_ioctl(struct inode *inode, struct file *filp,
                            unsigned int cmd, unsigned long arg)
#endif
{
    int retval;

    switch (cmd)
    {
    case EXA_NOIO:
        retval = exa_socket_set_atomic_kernel(arg);
        break;

    case EXA_EMERGENCY_SIZE:
        exa_socket_tweak_emergency_pool_kernel(arg);
        retval = 0;
        break;

    case EXA_SELECT_MAL:
        retval = exa_select_alloc(filp);
        break;

    case EXA_SELECT_IN:
    case EXA_SELECT_OUT:
        retval = exa_select_kernel(filp, cmd, arg);
        break;

    default:
        retval = -EINVAL;
        break;
    }

    return retval;
}


static struct file_operations exa_common_fops = {
    .read    = NULL,
    .write   = NULL,
#if HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl = exa_common_ioctl,
#else
    .ioctl          = exa_common_ioctl,
#endif
    .open    = exa_common_open,
    .release = exa_common_release
};


int init_module(void)
{
    exa_common_fops.owner = THIS_MODULE;

    exa_common_major = register_chrdev(0, EXACOMMON_MODULE_NAME, &exa_common_fops);
    if (exa_common_major < 0)
    {
        printk(KERN_ERR "Could not register " EXACOMMON_MODULE_NAME "\n");
        return -1;
    }

    return 0;
}


void cleanup_module(void)
{
    unregister_chrdev(exa_common_major, EXACOMMON_MODULE_NAME);
    exa_socket_tweak_emergency_pool_kernel(0);
}


MODULE_AUTHOR("Seanodes <http://www.seanodes.com>");
MODULE_DESCRIPTION("Common lib for Seanodes Exanodes");
MODULE_LICENSE("Proprietary");
