/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXA_NBD_LIST2
#define EXA_NBD_LIST2

#include <stdlib.h>
#include "os/include/os_semaphore.h"
#include "os/include/os_thread.h"
#include "os/include/os_atomic.h"
#include "os/include/os_inttypes.h"

struct nbd_root_list;

#define NBD_LIST_INFINITE UINT32_MAX

#define NO_MORE_ELT -1		/* last element of a list, or no more elt */
#define NOT_IN_LIST -2		/* element not in any list */

#define LIST_ALIVE     0	/* list alive normal mode */
#define LIST_DEAD      2	/* all elt post to the list go to free list
				 * all call of nbd_list_remove() will kill the caller
				 * thread */
struct nbd_list
{
  struct nbd_root_list *root;
  struct nbd_list *next;      /* an elt index or NO_MORE_ELT or NOT_IN_LIST */
  int first_elt;
  int last_elt;
  os_thread_mutex_t lock;

  volatile int must_die;      /* LIST_ALIVE or LIST_DEAD_POST or LIST_DEAD */

  /* Used to wait for select */
  os_sem_t select_sem;

  /* First list given in select */
  struct nbd_list *select_list;
  unsigned waiters; /*< numbers of waiters sleeping in select on the list */
  char init_place[256]; /*< used for debugging list starvion */
};

struct nbd_root_list
{
  int elt_size;
  int nb_elt;
  void *payload;
  int *next;
  long long *tag;
  struct nbd_list free;
};

#define LISTWAIT        1
#define LISTNOWAIT	0

#define nbd_list_remove(list, index, wait) \
    __nbd_list_remove(list, index, wait, __FILE__, __LINE__)
void *__nbd_list_remove(struct nbd_list *list, int *index, int wait, const char *file, int line);
void nbd_list_post(struct nbd_list *list, void *payload, int index);
#define nbd_init_list(root_list, list) \
    __nbd_init_list(root_list, list, __FILE__, __LINE__)
int __nbd_init_list(struct nbd_root_list *root_list, struct nbd_list *list, const char *file, int line);
#define nbd_init_root(nb_elt, elt_size, root_list) \
    __nbd_init_root(nb_elt, elt_size, root_list, __FILE__, __LINE__)
int __nbd_init_root(int nb_elt, int elt_size, struct nbd_root_list *root_list, const char *file, int line);
void nbd_close_root(struct nbd_root_list *root_list);
void *nbd_get_elt_by_num(int index, struct nbd_root_list *root);
int nbd_set_tag(const void *payload, long long tag, struct nbd_root_list *root);
int nbd_get_next_by_tag(void **payload, long long tag, struct nbd_root_list *root);
void nbd_close_list(struct nbd_list *list);
int nbd_list_select(struct nbd_list **lists, bool *lists_found,
                    unsigned int nb_list, unsigned ms_timeout);

#endif
