/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "../include/os_atomic.h"
#include "../include/os_inttypes.h"

int os_atomic_cmpxchg(os_atomic_t *ptr, int old_value, int new_value)
{
    int32_t prev, old = old_value, new = new_value;
    volatile int32_t *memory = &(ptr->val);
    __asm__ volatile("cmpxchgl %1,%2"
                     : "=a"(prev)
                     : "r"(new), "m"(*memory), "0"(old)
                     : "memory");
   return prev;
}

void os_atomic_set(os_atomic_t *atomic, int value)
{
    __asm__ __volatile__(
        "lock xchg %1,%0\n"
        :"=m" (atomic->val) :"ir" (value) : "memory");
}

int os_atomic_read(const os_atomic_t *atomic)
{
    return atomic->val;
}

bool os_atomic_dec_and_test(os_atomic_t *atomic)
{
    unsigned char c;

    __asm__ __volatile__(
        "lock decl %0\n"
        "sete %1"
        :"=m" (atomic->val), "=qm" (c)
        :"m" (atomic->val) : "memory");

    return c != 0;
}

void os_atomic_dec(os_atomic_t *atomic)
{
    __asm__ __volatile__(
        "lock decl %0\n"
        :"=m" (atomic->val)
        :"m" (atomic->val) : "memory");
}

void os_atomic_inc(os_atomic_t *atomic)
{
    __asm__ __volatile__(
        "lock incl %0\n"
        :"=m" (atomic->val)
        :"m" (atomic->val) : "memory");
}

