/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "target/linux_bd_target/module/bd_list.h"

#include <linux/errno.h>
#include <linux/vmalloc.h>

/**
 * Get a element from a List, if the list was closed by bd_close_list() we can
 * continue to remove elt as long there are pending elt.
 *
 * @param list target list
 * @param wait LISTNOWAIT get an element from this list, return NULL if there are no element
 *             LISTWAIT get an element from this list
 *                      if the list is empty and not closed wait for a new element
 *                      if the list is empty and closed return NULL
 */
void *bd_list_remove(struct bd_list *list, int wait)
{
    int found;
    void *payload = NULL;
    unsigned long flags;

    down(&list->prot_remove);
    if (list->first_elt == NO_MORE_ELT) /* There are one or more element pending */
    {
        if (list->dead == 1)
            wait = LISTNOWAIT;

        if (wait == LISTWAIT || list->next_first != NO_MORE_ELT)
            down(&list->get);
        else
        {
            up(&list->prot_remove);
            return NULL;
        }
        if (list->dead == 1)
        {
            up(&list->get);
            up(&list->prot_remove);
            return NULL;
        }
        list->first_elt = list->next_first;
        list->next_first = NO_MORE_ELT;
    }

    if (list->first_elt != NO_MORE_ELT)
    {
        found = list->first_elt;
        payload = list->root->payload + found * list->root->elt_size;
        list->first_elt = list->root->next[found];
        if (list->first_elt == NO_MORE_ELT)
        {
            spin_lock_irqsave(&list->prot_add, flags);

            /* Perhaps bd_list_add have add an elt */
            if (list->root->next[found] >= 0)
                list->first_elt = list->root->next[found];
            else  /* we are really in starvation so clear LastElt */
                list->last_elt = NO_MORE_ELT;

            spin_unlock_irqrestore(&list->prot_add, flags);
        }
        list->root->next[found] = NOT_IN_LIST;
    }
    up(&list->prot_remove);
    return payload;
}


/**
 * Add an element on a list
 * Note that bd_list_post() only use os_sem_post() and it's not a cancelation
 * point so there are no need to change the cancel state.
 * @param list list
 * @param payload element to add or NULL
 * @return 0 ok
 *         -1 list already dead
 */
int bd_list_post(struct bd_list *list, void *payload)
{
    int old;
    unsigned long flags;
    long index =
        ((unsigned long) payload -
    (unsigned long) list->root->payload) / list->root->elt_size;

    spin_lock_irqsave(&list->prot_add, flags);

    if (list->dead == 1)
    {
        spin_unlock_irqrestore(&list->prot_add, flags);
        return -1;
    }

    list->root->next[index] = NO_MORE_ELT;

    old = list->last_elt;
    if (old == NO_MORE_ELT)
    {
        list->last_elt = index;
        list->next_first = index;
        up(&list->get);
    }
    else
        list->root->next[list->last_elt] = index;

    list->last_elt = index;
    spin_unlock_irqrestore(&list->prot_add, flags);
    return 0;
}


void *bd_get_and_lock_last_posted(struct bd_list *list)
{
    unsigned long flags;

    spin_lock_irqsave(&list->prot_add, flags);
    if (list->last_elt == NO_MORE_ELT)
    {
        spin_unlock_irqrestore(&list->prot_add, flags);
        return NULL;
    }
    list->flags = flags;
    return list->root->payload + list->root->elt_size * list->last_elt;
}


void bd_unlock_last_posted(struct bd_list *list)
{
    unsigned long flags = list->flags;

    spin_unlock_irqrestore(&list->prot_add, flags);
}


/**
 * Initialise a new list and attach it to a root_list
 * @param root_list list will be ataach to this root_list
 * @param list list to initialie and attach
 * @param list_name name of the list
 * @return 0 if ok
 */
int bd_init_list(struct bd_root_list *root_list, struct bd_list *list)
{
    spin_lock_init(&list->prot_add);
    sema_init(&list->prot_remove, 1);
    sema_init(&list->get, 0);
    list->root = root_list;
    list->dead = 0;
    list->first_elt = NO_MORE_ELT;
    list->last_elt = NO_MORE_ELT;
    list->next_first = NO_MORE_ELT;
    return 0;
}


/**
 * after this call, all bd_list_post() to this list will fail
 * @param list
 */
void bd_close_list(struct bd_list *list)
{
    unsigned long flags;

    spin_lock_irqsave(&list->prot_add, flags);
    list->dead = 1;
    spin_unlock_irqrestore(&list->prot_add, flags);
    up(&list->get);
}


/**
 * Close a root list :
 * - Close all list attach to this root list
 * - Exit all thread waiting for an element in all list attach to this root_list
 * @param root_list list root to close
 */
void bd_close_root(struct bd_root_list *root_list)
{
    /* close all list except the free list */
    vfree(root_list->payload);
}


/**
 * Create a root list, (m)allocate elements on this root list and create the
 * first list: root_list->free the list of free elements. All elt is in
 * root_list->free at the end of this function.
 *
 * @param nb_elt number of element in this root list
 * @param elt_size size of each element
 * @param root_list root_list to initialize
 * @return 0 OK
 *         -ENOMEM malloc failed
 */
int bd_init_root(int nb_elt, int elt_size, struct bd_root_list *root_list)
{
    int i;

    /* if elt_size is aligned on 8 bytes (sizeof long long) all other array
     * will be aligned on 8 bytes and so read/write of all  ->next will be
     * atomic */
    elt_size = (elt_size - 1 + 16);
    elt_size = elt_size - (elt_size & 15);

    root_list->payload = vmalloc(elt_size * nb_elt + nb_elt * sizeof(int));
    root_list->next = (int *)(root_list->payload + elt_size * nb_elt);
    root_list->elt_size = elt_size;
    root_list->nb_elt = nb_elt;

    if (root_list->payload == NULL)
        return -ENOMEM;

    for (i = 0; i < nb_elt; i++)
        root_list->next[i] = NOT_IN_LIST;

    /* We need one at least one list : the free element list */
    bd_init_list(root_list, &root_list->free);
    for (i = 0; i < nb_elt; i++)
        bd_list_post(&root_list->free, root_list->payload + elt_size * i);

    return 0;
}


