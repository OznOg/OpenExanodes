/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_ATOMIC_H
#define _OS_ATOMIC_H
#include "os_inttypes.h"

typedef struct os_atomic
{
/*
 * Windows require a long (always 32bits)
 * and our implementation in linux use 32bits.
 */
/* FIXME WIN32
 * This structure declaration is doing exactly the contrary of what is
 * asked in os lib: abstract the os.
 * Either it is made private, and only accessed thru accessors, either it
 * has the SAME implementation ON BOTH platforms...
 * If the used of libos has to wonder which platform he is using, there is
 * something f**** up... */
#ifndef WIN32
    volatile int32_t val;
#else
    volatile long val;
#endif
} os_atomic_t;

/**
 *
 * @os_replace{Linux, __asm__, lock, xchg}
 */
void os_atomic_set(os_atomic_t *atomic, int value);

/* FIXME WIN32
 * os_atomic_read actually returns the value of a os_atomic
 * which is NEVER a int (for now a long or a int32_t)...
 * should very likely be passed to uint32_t */
/**
 *
 */
int os_atomic_read(const os_atomic_t *atomic);

/**
 * Atomically decrements and test an os_atomic_t.
 *
 * @param atomic   the atomic variable.
 *
 * @return true if the the atomic variable is 0 once decremented,
 *         or false for all other cases (negative or positive).
 *
 * @os_replace{Linux, __asm__, lock, decl}
 * @os_replace{Windows, InterlockedDecrement}
 */
bool os_atomic_dec_and_test(os_atomic_t *atomic);

/**
 *
 * @os_replace{Linux, __asm__, lock, decl}
 * @os_replace{Windows, InterlockedDecrement}
 */
void os_atomic_dec(os_atomic_t *atomic);

/**
 *
 * @os_replace{Linux, __asm__, lock, incl}
 * @os_replace{Windows, InterlockedIncrement}
 */
void os_atomic_inc(os_atomic_t *atomic);

/*
 * Atomic compare and exchange of the value match the value expected
 * @param ptr atomic variable
 * @param old_value value expected for ptr
 * @param new_value this value will be the new value of *ptr if value of *ptr was old_value
 * @return value of *ptr
 *
 * @os_replace{Linux, cmpxchgl}
 * @os_replace{Windows, InterlockedCompareExchange}
 */
int os_atomic_cmpxchg(os_atomic_t *ptr, int old_value, int new_value);

#endif /* _OS_ATOMIC_H */

