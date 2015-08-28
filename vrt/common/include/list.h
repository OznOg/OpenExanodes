/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _KERNEL_ADAPT_LIST_H
#define _KERNEL_ADAPT_LIST_H

#include <stdlib.h>

struct list_head
	{
	  struct list_head * next;
	  struct list_head * prev;
	};

#define LIST_HEAD(a) struct list_head a = { .next = &(a) , .prev = &(a) }

#define INIT_LIST_HEAD(ptr) do { \
  (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

static inline void list_add_tail(struct list_head * up, struct list_head * head)
{
  struct list_head *old;

  old = head->prev;
  up->next = head;
  up->prev = old;
  old->next = up;
  head->prev = up;
}

static inline void list_add(struct list_head * down, struct list_head * head)
{
  struct list_head *old;

  old = head->next;
  down->next = old;
  down->prev = head;
  old->prev = down;
  head->next = down;
}

static inline void list_del (struct list_head * to_remove)
{
  struct list_head * down = to_remove->prev;
  struct list_head * up = to_remove->next;
  if (down != NULL)
    down->next = up;
  if (up != NULL)
    up->prev = down;
  to_remove->prev = to_remove->next = NULL;
}

static inline int list_empty (struct list_head * head)
{
  return (head->next != head ? 0 : 1);
}

#define LIST_HEAD_INIT(name) { &name, &name }
#define list_entry(ptr,member,type)     (type *)(((char *)(ptr))  - ((char *)(&((type *)NULL)->member)))

#define list_for_each(pos,head)\
            for (pos=(head)->next; pos != head; (pos) = (pos)->next)

#define list_for_each_safe(pos,n,head)\
            for (pos=(head)->next, n = (pos)->next; pos != head; (pos) = n, n = n->next)

#define list_next(head,member,type) list_entry(head->member.next,member,type)

#define list_for_each_entry(pos,head,member,type)         \
         for (pos=list_entry((head)->next,member,type);   \
            pos!=list_entry((head),member,type);          \
            pos = list_entry((pos)->member.next, member, type))

#define list_for_each_entry_safe(pos,n,head,member,type)         \
         for (pos=list_entry((head)->next,member,type), n = list_entry((pos)->member.next, member, type);   \
            pos!=list_entry((head),member,type);          \
            pos = n, n = list_entry((n)->member.next, member, type))

#endif  /* _KERNEL_ADAPT_LIST_H */
