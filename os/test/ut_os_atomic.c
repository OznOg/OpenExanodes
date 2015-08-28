/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/*
 * IMPORTANT!
 *
 * This unit test *must* be common to both Linux and Windows: we want the very
 * same test cases to work identically on both platforms.
 */

#include "os/include/os_atomic.h"
#include <unit_testing.h>


UT_SECTION(atomic)

ut_test(atomic_set_read_inc_dec)
{
    os_atomic_t var;
    int i;
    for (i = -10 ; i < 32768; i ++)
    {
        os_atomic_set(&var, i);
	UT_ASSERT(os_atomic_read(&var) == i);
	os_atomic_inc(&var);
	UT_ASSERT(os_atomic_read(&var) == i + 1);
	os_atomic_dec(&var);
	os_atomic_dec(&var);
	UT_ASSERT(os_atomic_read(&var) == i - 1);
    }
}

ut_test(atomic_dec_and_test)
{
    os_atomic_t var;
    int i;
    for (i = -10 ; i < 32768; i ++)
    {
        os_atomic_set(&var, i);
        if (i - 1 == 0)
            UT_ASSERT(os_atomic_dec_and_test(&var) != 0);
        else
	    UT_ASSERT(os_atomic_dec_and_test(&var) == 0);
        UT_ASSERT(os_atomic_read(&var) == i -1);
    }
}

ut_test(cmpxchg)
{
    os_atomic_t var;
    int i;
    for (i = -10 ; i < 32768; i ++)
    {
        os_atomic_set(&var, i);

	UT_ASSERT(os_atomic_cmpxchg(&var, i + 1, i + 2) == i);
	UT_ASSERT(os_atomic_read(&var) == i);

	UT_ASSERT(os_atomic_cmpxchg(&var, i, i + 2) == i);
	UT_ASSERT(os_atomic_read(&var) == i + 2);
    }
}
