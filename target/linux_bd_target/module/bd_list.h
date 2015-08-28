/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXA_BD_LIST
#define EXA_BD_LIST

#ifndef __KERNEL__
# error "This is kernel code ; it needs kernel header."
#endif

#include <linux/spinlock.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif

#define NO_MORE_ELT -1 /* last element of a list, or no more elt */
#define NOT_IN_LIST -2  /* element not in any list */

struct bd_root_list;

struct bd_list
{
    struct bd_root_list *root;
    int first_elt;
    int next_first;
    int last_elt;
    int dead;
    struct semaphore get;
    spinlock_t       prot_add;
    struct semaphore prot_remove;
    unsigned long    flags;
};

struct bd_root_list
{
    int elt_size;
    int nb_elt;
    void          *payload;
    int           *next;
    struct bd_list free;
};

#define LISTWAIT   1
#define LISTNOWAIT 0

void *bd_list_remove(struct bd_list *list, int wait);
int bd_list_post(struct bd_list *list, void *payload);
int bd_init_list(struct bd_root_list *root_list, struct bd_list *list);
int bd_init_root(int nb_elt, int elt_size, struct bd_root_list *root_list);
void bd_close_root(struct bd_root_list *root_list);
void bd_close_list(struct bd_list *list);
void *bd_get_and_lock_last_posted(struct bd_list *list);
void bd_unlock_last_posted(struct bd_list *list);

#endif
