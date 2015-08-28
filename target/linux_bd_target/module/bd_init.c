/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "target/linux_bd_target/module/bd_user_bd.h"
#include "target/linux_bd_target/module/bd_user_fops.h"

#include <linux/module.h>

/*
 *
 * Description of all module :
 *      bd_init.c       : this file, init module structures.
 *  bd_user_fops.c  : user interface receive command (ioctl, char device, mapping kernel data)
 *  bd_user_bd.c    : blk handling interface with the kernel, handle all blk operation like
 *  register, receive request ending request.
 *  bd_user_kernel.c: request exchange communication with user : add new request, complete
 *          one request, launch/kill new Session of communication with user
 *
 *
 *	bd_user.h      : structure mapped in user and kernel, session specific structure
 *
 *	bd_user_user.c  : Function for user mode
 *	bd_user_user.h  : structure for user mode not shared with kernel
 *
 *
 *  bd_example.c    : Simple example of how to handling a block device.
 */

MODULE_AUTHOR("Seanodes <http://www.seanodes.com>");
MODULE_DESCRIPTION("Network Block device for Seanodes Exanodes");
MODULE_LICENSE("Proprietary");

int init_module(void)
{
    return bd_register_fops();
}


void cleanup_module(void)
{
    bd_unregister_fops();
}


