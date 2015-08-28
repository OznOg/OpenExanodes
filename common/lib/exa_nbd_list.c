/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_constants.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_nbd_list.h"

#include "log/include/log.h"

#include "os/include/os_error.h"
#include "os/include/os_stdio.h"
#include "os/include/os_mem.h"
#include "os/include/os_thread.h"

/**
 * Helper function used to remove a element from a list. This function assumes
 * that the caller has the lock of the List.
 *
 * @param[in]     list the list
 * @param[in:out] Index the index of the element found (ignored if NULL)
 *
 * @return a element pointer if one was found, otherwise NULL
 */
static void *nbd_list_remove_no_lock(struct nbd_list *list, int *index)
{
    int first_idx;
    void *payload = NULL;

    if (list->must_die == LIST_DEAD)
        return NULL;

    if (list->first_elt == NO_MORE_ELT)
        return NULL;

    first_idx = list->first_elt;
    /* FIXME: 'tag[]' is not an indirection and is not of the same type as
     * 'next[]', 'first_elt', 'last_elt' and so on. It is more than strange
     * that it could take the same value.
     */
    list->root->tag[first_idx] = NO_MORE_ELT;
    payload = (char *) list->root->payload + first_idx * list->root->elt_size;

    if (index != NULL)
        *index = first_idx;

    /* Take the next element in list as the first */
    list->first_elt = list->root->next[first_idx];
    if (list->first_elt == NO_MORE_ELT)
    {
        /* if there is no more element, the firt was also the last one */
        EXA_ASSERT(list->last_elt == first_idx);
        list->last_elt = NO_MORE_ELT;
    }
    list->root->next[first_idx] = NOT_IN_LIST;

    return payload;
}


/**
 * Get a element from a List Not
 *
 * @param list   target list
 * @param index  element index, used if Paylload is NULL
 * @param wait LISTNOWAIT : if one new element is present, remove it from
 *                          the list and return it, otherwise return NULL
 *                          without waiting
 *             LISTWAIT   : if one new element is present, remove it from
 *                          the list and return it, otherwise wait until a
 *                          new element is present, remove it from the list
 *                          and return it.
 *
 * @return pointer to the removed element or NULL if there is no element in
 *         list. When wait is LISTWAIT, this function NEVER returns NULL (it
 *         blocks the caller waiting for an element to arrive).
 */
void *__nbd_list_remove(struct nbd_list *list, int *index, int wait,
                        const char *file, int line)
{
    void *payload = NULL;

    if (index != NULL)
        *index = -1;

    do {
        struct nbd_list *lists_in[1];
        bool lists_out[1];

        os_thread_mutex_lock(&list->lock);

        payload = nbd_list_remove_no_lock(list, index);

        os_thread_mutex_unlock(&list->lock);

        if (wait != LISTWAIT || payload != NULL)
            break;

        exalog_debug("list %s is starving@%s:%d", list->init_place, file, line);
        lists_in[0] = list;

        nbd_list_select(lists_in, lists_out, 1, NBD_LIST_INFINITE);

        /* the while is necessary because there is a race between the select
         * and the remove: there is no guaranty that the element that made
         * select exit was not removed by some other waiter meanwhile */
    } while (payload == NULL);

    return payload;
}


/**
 * Helper function used to check if a list is empty. This function assumes
 * the caller has the lock of the list.
 *
 * @param      list   The list to get an element from
 *
 * @return true if the list is empty, false if the list is not empty
 */
static bool nbd_list_is_empty(const struct nbd_list *list)
{
    if (list->must_die  == LIST_DEAD)
        return true;

    /* If first and next_first are NO_MORE_ELT, the list is empty (no element
     * and no pending element) */
    return list->first_elt == NO_MORE_ELT;
}


/**
 * Wait for an element to arrive in one one of the lists given in a set of
 * lists. When an element is already present in list, this function returns
 * immediatly.
 * Having this function return 0 means that the timeout was reached and no
 * element was added to any of the lists given in input.
 *
 * @param[in]  lists        The set of lists to check
 * @param[out] lists_found  Lists_found[i] is true if lists[i] is not empty.
 * @param      nb_list      Number of lists
 * @param      ms_timeout   Time to wait for lists to become not empty
 *                          NBD_LIST_INFINITE      : Wait until a list is no
 *                                                   more empty
 *                          ]0..NBD_LIST_INFINITE[ : Wait until a list is no
 *                                                   more empty or
 *                                                   ms_timeout milliseconds
 *                          0                      : Don't wait, just give the
 *                                                   no empty list.
 * @return number of non empty lists in given lists
 */
int nbd_list_select(struct nbd_list **lists, bool *lists_found,
                    unsigned int nb_list, unsigned ms_timeout)
{
    int i;
    int count = 0;
    bool wait = (ms_timeout != 0);

    EXA_ASSERT(lists != NULL);
    EXA_ASSERT(lists_found != NULL);

    for (i = 0; i < nb_list; i++)
        os_thread_mutex_lock(&lists[i]->lock);

    /* update the list not empty */
    for (i = 0; i < nb_list; i++)
        if (!nbd_list_is_empty(lists[i]))
        {
            lists_found[i] = true;
            count++;
        }

    if (count == 0 && wait)
    {
        int err;

        /* All lists are empty, so register to wait on list[0] */
        for (i = 0; i < nb_list; i++)
        {
            EXA_ASSERT(lists[i]->select_list == NULL
                       || lists[i]->select_list == lists[0]);
            lists[i]->select_list = lists[0];
            lists[0]->waiters++;
        }

        /* wait if needed */
        for (i = 0; i < nb_list; i++)
            os_thread_mutex_unlock(&lists[i]->lock);

        if (ms_timeout == NBD_LIST_INFINITE)
            err = os_sem_wait(&lists[0]->select_sem);
        else
            err = os_sem_waittimeout(&lists[0]->select_sem, ms_timeout);

        EXA_ASSERT(err == 0 || err == -EINTR || err == -ETIMEDOUT);

        for (i = 0; i < nb_list; i++)
            os_thread_mutex_lock(&lists[i]->lock);

        /* update the list not empty */
        for (i = 0; i < nb_list; i++)
            if (!nbd_list_is_empty(lists[i]))
            {
                lists_found[i] = true;
                count++;
            }

        /* unregister from all lists */
        for (i = 0; i < nb_list; i++)
            lists[i]->select_list->waiters--;

        for (i = 0; i < nb_list; i++)
            if (lists[i]->select_list->waiters == 0)
                lists[i]->select_list = NULL;
    }

    for (i = 0; i < nb_list; i++)
        os_thread_mutex_unlock(&lists[i]->lock);

    return count;
}


/**
 * Add an element on a list Note that nbd_list_post() only use
 * os_sem_post() and it's not a cancelation point so there are no need
 * to change the cancel state.
 *
 * @param list list
 * @param payload element to add or NULL
 * @param index if Payload is NULL, this is the index of the element to add
 *      if Payload is not NULL, Index is ignored
 */
void nbd_list_post(struct nbd_list *list, void *payload, int index)
{
    os_thread_mutex_lock(&list->lock);
    if (payload != NULL)
        index =
            ((unsigned long) payload -
             (unsigned long) list->root->payload) / list->root->elt_size;

    EXA_ASSERT(index >= 0 && index < list->root->nb_elt);
    EXA_ASSERT(list->root->next[index] == NOT_IN_LIST);
    list->root->tag[index] = -1;
    list->root->next[index] = NO_MORE_ELT;


    if (list->must_die != LIST_ALIVE)
    {
        os_thread_mutex_unlock(&list->lock);

        /* this list will die, but we must not forbid this elt
         * so we post it in the free list */
        if (list != &list->root->free)
            nbd_list_post(&list->root->free, payload, index);

        return;
    }

    /* list is empty */
    if (list->first_elt == NO_MORE_ELT)
    {
        EXA_ASSERT(list->last_elt == NO_MORE_ELT);

        list->first_elt = index;

        /* As there was no element in the list, we signal any waiter that
         * there is something to read right now */
        if (list->select_list != NULL)
        {
            unsigned int i;
            for (i = list->select_list->waiters; i > 0; i--)
                os_sem_post(&list->select_list->select_sem);
        }
    }
    else
    {
        EXA_ASSERT(list->last_elt != NO_MORE_ELT);
        EXA_ASSERT(list->root->next[list->last_elt] == NO_MORE_ELT);
        /* add element at the end of list */
        list->root->next[list->last_elt] = index;

        /* Do not signal the list's semaphore if there were already element in
         * it: the select is not supposed to sleep if lists are not empty */
    }

    list->last_elt = index;

    os_thread_mutex_unlock(&list->lock);
}


/**
 * Close a list
 *
 * All elt in this list are sent back to free list
 *
 * This cannot be used with free list
 *
 * After this call:
 * - All thread waiting from a nbd_list_remove() will be autocanceled.
 * - All thread adding elt on this will add elt on free list
 *   and will call os_thread_testcancel().
 *
 * @param list
 */
void nbd_close_list(struct nbd_list *list)
{
    int index_find, index_temp;
    struct nbd_list *list_temp;
    struct nbd_root_list *root = list->root;

    if (&root->free == list)
        return;

    if (list->must_die != LIST_ALIVE)
        return;

    /* We will stop all nbd_list_remove() of this list, so all thread that call
     * nbd_list_remove() of this list will exit
     */
    os_thread_mutex_lock(&list->lock);

    list->must_die = LIST_DEAD;

    os_thread_mutex_unlock(&list->lock);

    /* We will get back all elt to the free list so because nbd_list_post() to
     * this list was forbiden, this list will be empty we cannot use
     * nbd_list_remove() because we forbid all nbd_list_remove() in previous
     * step
     */
    index_find = list->first_elt;

    while (index_find != NO_MORE_ELT)
    {
        index_temp = root->next[index_find];
        root->next[index_find] = NOT_IN_LIST;
        nbd_list_post(&root->free, NULL, index_find);
        index_find = index_temp;
    }

    /* the list will be removed from all the list of the root list */
    os_thread_mutex_lock(&root->free.lock);

    list_temp = &root->free;
    while (list_temp->next != NULL)
    {
        if (list_temp->next == list)
            list_temp->next = list->next;

        list_temp = list_temp->next;
        if (list_temp == NULL)
            break;
    }

    os_thread_mutex_unlock(&root->free.lock);
}


/**
 * Initialise a new list and attach it to a root_list
 *
 * @param root_list list will be ataach to this root_list
 * @param list list to initialie and attach
 *
 * @return 0 if ok
 */
int __nbd_init_list(struct nbd_root_list *root_list, struct nbd_list *list,
                    const char *file, int line)
{
    os_thread_mutex_init(&list->lock);

    os_snprintf(list->init_place, sizeof(list->init_place), "%s:%d", file, line);
    os_sem_init(&list->select_sem, 0);
    list->select_list = NULL;
    list->waiters = 0;

    list->root = root_list;
    list->first_elt = NO_MORE_ELT;
    list->last_elt = NO_MORE_ELT;
    /* only one list can be added at one time */
    if (&list->root->free != list)
    {
        os_thread_mutex_lock(&list->root->free.lock);
        list->next = list->root->free.next;
        list->root->free.next = list;
        os_thread_mutex_unlock(&list->root->free.lock);
    }
    list->must_die = LIST_ALIVE;

    return 0;
}


/**
 * Close a root list :
 * - Close all list attach to this root list
 * - Exit all thread waiting for an element in all list attach to this root_list
 *
 * @param root_list list root to close
 */
void nbd_close_root(struct nbd_root_list *root_list)
{
    /* close all list except the free list */
    while (root_list->free.next != NULL)
        nbd_close_list(root_list->free.next);

    /* close the free list  */
    root_list->free.must_die = LIST_DEAD;

    os_aligned_free(root_list->payload);
}


/**
 * Create a root list, (m)allocate elements on this root list and
 * create the first list : root_list->free the list of free
 * elements. All elt is in root_list->free at the end of this
 * function.
 *
 * @param nb_elt number of element in this root list
 * @param elt_size size of each element
 * @param root_list root_list to initialize
 *
 * @return 0 OK
 *         -1 malloc failed
 */
int __nbd_init_root(int nb_elt, int elt_size, struct nbd_root_list *root_list,
                    const char *file, int line)
{
    int i;

    elt_size = elt_size + 7;
    elt_size = elt_size - (elt_size & 7);

    root_list->payload = os_aligned_malloc(nb_elt * elt_size, 4096, NULL);
    root_list->tag  = os_malloc(nb_elt * sizeof(*root_list->tag));
    root_list->next = os_malloc(nb_elt * sizeof(*root_list->next));

    root_list->elt_size = elt_size;
    root_list->nb_elt   = nb_elt;

    if (root_list->payload == NULL)
        return -1;

    for (i = 0; i < nb_elt; i++)
    {
        root_list->tag[i] = -1;
        root_list->next[i] = NOT_IN_LIST;
    }
    /* We need one at least one list : the free element list */
    root_list->free.next = NULL;
    __nbd_init_list(root_list, &root_list->free, file, line);

    for (i = 0; i < nb_elt; i++)
        nbd_list_post(&root_list->free, NULL, i);

    root_list->free.next = NULL;
    return 0;
}


/**
 * Get the addresse of an nonfree elements of a list
 *
 * @param index index of the element
 * @param root target list
 *
 * @return addresse of the elements, NULL if the index is out of
 * bound, if it's an freed element or if ot's an invalid element
 */
void *nbd_get_elt_by_num(int index, struct nbd_root_list *root)
{
    void *payload;

    if (index < 0 || index >= root->nb_elt)
        return NULL;

    if (root->next[index] != NOT_IN_LIST)
        return NULL;

    payload = (char *) root->payload + index * root->elt_size;
    return payload;
}


/**
 * Tag of a nonfree elements of a list
 *
 * @param payload   buffer from a nbd_list that has to be tagged
 * @param tag tag
 * @param root target list
 *
 * @return 0 ok, -1 error
 */
int nbd_set_tag(const void *payload, long long tag, struct nbd_root_list *root)
{
    int index;
    EXA_ASSERT(payload != NULL);

    index = ((unsigned long) payload -
             (unsigned long) root->payload) / root->elt_size;

    EXA_ASSERT(index >= 0 && index < root->nb_elt);

    root->tag[index] = tag;

    return 0;
}

/**
 * Next elt with this Tag
 *
 * @param[inout] in : Elt first elt to begin search, out Elt
 * @param tag tag
 * @param list target list
 *
 * @return 0 ok, -1 error
 */
int nbd_get_next_by_tag(void **payload, long long tag,
                        struct nbd_root_list *root)
{
    int index;
    if (payload != NULL)
        *payload = NULL;

    for (index = 0; index < root->nb_elt; index++)
    {
        /* FIXME this kind of tagging is crappy, there is no check that a tag
         * was actually set. */
        if (root->tag[index] == tag
            && root->next[index] == NOT_IN_LIST)
        {
            if (payload != NULL)
                *payload = (char *) root->payload + ((root->elt_size) * index);
            return 0;
        }
    }
    return -1;
}


