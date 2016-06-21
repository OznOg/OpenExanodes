/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "target/linux_bd_target/module/bd_user_fops.h"

#include "target/linux_bd_target/include/bd_user.h"

#include "target/linux_bd_target/module/bd_user_bd.h"

#include "target/linux_bd_target/module/bd_user_kernel.h"

#include "target/linux_bd_target/module/bd_log.h"

#include "common/include/exa_names.h"

#include <linux/uaccess.h>

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/fs.h> /* For struct file */

/*
 * There are two area mapped in user mode
 * - the first contain bd_kernel_queue with all info of request mapped in user
 *   mode by kernel, this area is read-only
 * - the seconde contain bd_user_queue with info manage by client for each i
 *   requesrt BdEndTab, a tab used to tell to kernel wich request have ended
 *   and the mapped buffer.
 *
 * This file handle the mapping of these file, bd_kernel_queue, bd_user_queue,
 * BdEndTab are always mapped but buffer are mapped on demand.
 *
 * This file also handling the ioctl from user and passe it to the kernel
 * thread in bd_kernel_queue file.
 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0))
static int bd_mops_vmfault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
#else
static int bd_mops_vmfault(struct vm_fault *vmf)
{
  struct vm_area_struct *vma = vmf->vma;
#endif
  struct bd_session *session = vma->vm_file->private_data;
  if (session == NULL)
  {
      bd_log_error ("NoPage with NULL session at address : %p\n", (void *)vmf->pgoff);
      return VM_FAULT_OOM;
  }

  if (vma->vm_end - vma->vm_start == 2 * session->bd_page_size)
  {
      if (vmf->pgoff * PAGE_SIZE > session->bd_page_size)
          return VM_FAULT_SIGBUS;
      else
          vmf->page = vmalloc_to_page(((char *) session->bd_kernel_queue)
                                      + (vmf->pgoff * PAGE_SIZE));

      if (vmf->page == NULL)
          return VM_FAULT_OOM;

      get_page(vmf->page);

      return 0;
  }

  if (vma->vm_end - vma->vm_start == 3 * session->bd_page_size)
  {
      if (vmf->pgoff * PAGE_SIZE > session->bd_page_size)
          return VM_FAULT_SIGBUS;
      else
          vmf->page = vmalloc_to_page(((char *) session->bd_user_queue)
                                      + (vmf->pgoff * PAGE_SIZE));

      if (vmf->page == NULL)
          return VM_FAULT_OOM;

      get_page(vmf->page);

      return 0;
  }
  return VM_FAULT_SIGBUS;
}

struct vm_operations_struct bd_mops;

static void bd_mops_open(struct vm_area_struct *area)
{
    area->vm_ops = &bd_mops;
}

struct vm_operations_struct bd_mops =
{
    .open   = bd_mops_open,
    .fault = bd_mops_vmfault
};

static int bd_fops_open(struct inode *I, struct file *F)
{
    F->private_data = NULL;
    /* private_data will own session number */
    F->f_mode |= FMODE_READ | FMODE_WRITE;
    return 0;
};

/**
 * Create and start a Session for one major
 * @param cmd   value
 * @param arg   meaning result meaning
 *
 * BD_IOCTL_INIT     Major >=0 Ok it's major nb
 *                  ,-1=Error   create a new session to handle
 *                          "Major" block device ; all minor are disable (size=0)
 *                          if "Major==0" Major return the new Major
 * BD_IOCTL_SEM_ACK     0=Ok,-1=Error        New ack event (wake up kernel thread if necessary
 * BD_IOCTL_SEM_NEW     """""""""""""        Wait for a new event "new request"
 * BD_IOCTL_NEWMINOR                         Add a new minor device with state supend and new state
 * BD_IOCTL_DELMINOR                         Remove a minor
 *                 */
#if HAVE_UNLOCKED_IOCTL
static long bd_fops_ioctl(struct file *F, unsigned int cmd, unsigned long arg)
#else
static int bd_fops_ioctl(struct inode *I, struct file *F,
                         unsigned int cmd, unsigned long arg)
#endif
{
    struct bd_session *session = F->private_data;
    struct bd_event_msg msg;

    /* If no Session initialisated
     * you can only initialised a new one ! */
    if ((cmd != BD_IOCTL_INIT && session == NULL)
        || (cmd == BD_IOCTL_INIT && session != NULL))
        return -1;

    /* Can only do one initialization per file */
    switch (cmd)
    {
    case BD_IOCTL_INIT:
        {
            struct bd_init init;

            if (copy_from_user(&init, (void __user *)arg, sizeof(init)) != 0)
                return -EFAULT;

            session = bd_launch_session(&init);
            if (session == NULL)
            {
                bd_log_error("BD : Error : cannont create a session\n");
                return -1;
            }
            init.bd_page_size = session->bd_page_size;
            if (copy_to_user((void __user *)arg, &init, sizeof(init)) != 0)
                return -EFAULT;

            F->private_data = session;
            return session->bd_major; /* All is ok */
        }

    case BD_IOCTL_SEM_ACK:
        bd_new_event(session->bd_thread_event, BD_EVENT_ACK_NEW);
        return 0;

    case BD_IOCTL_SEM_NEW_UP:
        bd_new_event(session->bd_new_rq, BD_EVENT_ACK_NEW);
        return 0;

    case BD_IOCTL_SEM_NEW:
        {
            unsigned long temp = 0;
            int err = bd_wait_event(session->bd_new_rq, &temp, NULL);
            if (temp == BD_EVENT_TIMEOUT)
                return -EAGAIN;

            if (err == 1)
                return -EINTR;

            if (err == 2)
                return -EBADF;

        }
        return 0;

    case BD_IOCTL_SETSIZE:
    case BD_IOCTL_NEWMINOR:
    {
        struct bd_new_minor minor;
        if (copy_from_user(&minor, (void __user *)arg,
                           sizeof(struct bd_new_minor)) != 0)
        {
            bd_log_error("BD : Error : Cannot read New Minor \n");
            return -EFAULT;
        }

        if (cmd == BD_IOCTL_NEWMINOR)
            msg.bd_type = BD_EVENT_NEW;
        else
            msg.bd_type = BD_EVENT_SETSIZE;

        msg.bd_minor_size_in512_bytes = minor.size_in512_bytes;
        msg.bd_minor = minor.bd_minor;
        msg.bd_minor_readonly = minor.readonly;

        bd_new_event_msg_wait_processed(session->bd_thread_event, &msg);
        return msg.bd_result;
    }

    case BD_IOCTL_DELMINOR:
        msg.bd_type = BD_EVENT_DEL;
        msg.bd_minor = arg;
        bd_new_event_msg_wait_processed(session->bd_thread_event, &msg);
        return msg.bd_result;

    case BD_IOCTL_IS_INUSE:
        msg.bd_type = BD_EVENT_IS_INUSE;
        msg.bd_minor = arg;
        bd_new_event_msg_wait_processed(session->bd_thread_event, &msg);
        return msg.bd_result;

    default:
        return -ENOIOCTLCMD;  /* Unimplemented IOCTL */
    }
}


/* BdFopsRelease trap a process close or the close of the vm like with kill -9 */
static int bd_fops_release(struct inode *I, struct file *F)
{
    struct bd_session *session = F->private_data;
    struct bd_event_msg msg;

    if (F->private_data == 0)
        return 0; /* No need to do anything */

    msg.bd_type = BD_EVENT_KILL;

    /* Send a Kill event to the thread and wait for it's end of processing */
    bd_new_event_msg_wait_processed(session->bd_thread_event, &msg);
    wait_for_completion(&session->bd_end_completion);

    /* now Session wil be no more used because block device are down */
    bd_put_session(&session);
    F->private_data = NULL;
    return 0;
};

static int bd_fops_mmap(struct file *F, struct vm_area_struct *vma)
{
    struct bd_session *session = (struct bd_session *) F->private_data;

    if (F->private_data == NULL)
        return -1;  /* Session have not been created */

    if ((vma->vm_end - vma->vm_start == 3 * session->bd_page_size)
        && (vma->vm_flags & VM_WRITE) != 0
        && session->bd_user_queue_user == NULL)
    {
        /* BdMops.owner=THIS_MODULE; */
        vma->vm_file = F;
        vma->vm_ops = &bd_mops;
#ifdef VM_ALWAYSDUMP
        vma->vm_flags |= VM_ALWAYSDUMP; /* always include in coredump */
#endif
#ifdef VM_RESERVED
        vma->vm_flags |= VM_RESERVED /*| VM_IO; is not good | VM_DONTCOPY) */;
#else
	/* FIXME VM_RESERVED was originally set here, but I don't really know
	 * what was the original purpose as the meaning of VM_RESERVED moved a
	 * lot since kernel 2.4 */
	vma->vm_flags |= VM_IO | VM_DONTEXPAND;
#endif
        /* Nothing was mapped but nopage will populate
         * when something will be mapped, it must never swapout */
        /* Initiate user pointer */
        session->bd_user_queue_user = (void *)(vma->vm_start);
        return 0;
    }

    if ((vma->vm_end - vma->vm_start == 2 * session->bd_page_size)
        && (vma->vm_flags & VM_WRITE) == 0
        && session->bd_kernel_queue_user == NULL)
    {
        session->bd_kernel_queue_user = (void *) vma->vm_start;
        vma->vm_ops = &bd_mops;
        vma->vm_file = F;
#ifdef VM_ALWAYSDUMP
        vma->vm_flags |= VM_ALWAYSDUMP; /* always include in coredump */
#endif
#ifdef VM_RESERVED
        vma->vm_flags |= VM_RESERVED /*| VM_IO; is not good | VM_DONTCOPY) */;
#else
	/* FIXME VM_RESERVED was originally set here, but I don't really know
	 * what was the original purpose as the meaning of VM_RESERVED moved a
	 * lot since kernel 2.4 */
	vma->vm_flags |= VM_IO | VM_DONTEXPAND;
#endif
        /* Nothing was mapped but nopage will populate
         * when something will be mapped, it must never swapout */
        return 0;
    }

    bd_log_error("BD : Error : mmap : invalid size %lu writeable %s "
                 "bd_kernel_queue_user %p bd_user_queue_user %p\n",
                 (unsigned long)(vma->vm_end - vma->vm_start),
                 ((vma->vm_flags & VM_WRITE) == 0) ? "READ" : "WRITE",
                 session->bd_kernel_queue_user, session->bd_user_queue_user);

    return -1; /* each area can only be mapped once per session */
}


struct file_operations bd_fops =
{
    .open    = bd_fops_open,
    .mmap    = bd_fops_mmap,
#if HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl   = bd_fops_ioctl,
#else
    .ioctl   = bd_fops_ioctl,
#endif
    .release = bd_fops_release
};

static int bd_fops_major = 0;

int bd_register_fops(void)
{
    bd_fops.owner = THIS_MODULE;
    bd_fops_major = register_chrdev(0, NBD_MODULE_NAME, &bd_fops);
    if (bd_fops_major < 0)
    {
        printk(KERN_ERR "Could not register exa_bd char device\n");
        return bd_fops_major;
    }

    return 0;
}


void bd_unregister_fops(void)
{
    unregister_chrdev(bd_fops_major, NBD_MODULE_NAME);
}


