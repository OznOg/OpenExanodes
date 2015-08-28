/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "examsg/src/objpoolapi.h"

ut_setup()
{
}

ut_cleanup()
{
}

ut_test(create_a_pool_too_small)
{
    int i;
    ExaMsgReservedObj *pool = examsgPoolInit(&i, sizeof(i));
    UT_ASSERT(!pool);
}

ut_test(create_a_correct_pool)
{
    char memory[10*1024];
    ExaMsgReservedObj *pool;
    pool = examsgPoolInit(memory, sizeof(memory));
    UT_ASSERT(pool);
}

ut_test(create_chunk_for_all_slots_possibles_and_delete_them)
{
    ExamsgID id;
    char memory[(EXAMSG_LAST_ID + 1) * 1024];
    ExaMsgReservedObj *pool;
    pool = examsgPoolInit(memory, sizeof(memory));

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjCreate(pool, id, (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1));
	UT_ASSERT(ret == 0);
    }

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjDelete(pool, id);
	UT_ASSERT(ret == 0);
    }
}

ut_test(fill_all_chunk_with_data_and_check_them)
{
    ExamsgID id;
    char memory[(EXAMSG_LAST_ID + 1) * 1024];
    ExaMsgReservedObj *pool;
    pool = examsgPoolInit(memory, sizeof(memory));

    /* Create and write data */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	void *data;
	size_t size = (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1);
	int ret = examsgObjCreate(pool, id, size);
	UT_ASSERT(ret == 0);
	data = examsgObjAddr(pool, id);
	memset(data, (char)id, size);
    }

   for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int i;
	char *data;
	size_t size = (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1);
	data = examsgObjAddr(pool, id);
	for (i = 0; i < size; i++)
	    UT_ASSERT(data[i] == (char)id);
    }

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjDelete(pool, id);
	UT_ASSERT(ret == 0);
    }
}

ut_test(create_all_and_recreate_them_good_size)
{
    ExamsgID id;
    char memory[(EXAMSG_LAST_ID + 1) * 1024];
    ExaMsgReservedObj *pool;
    pool = examsgPoolInit(memory, sizeof(memory));

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjCreate(pool, id, (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1));
	UT_ASSERT(ret == 0);
    }

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjDelete(pool, id);
	UT_ASSERT(ret == 0);
    }

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjCreate(pool, id, (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1));
	UT_ASSERT(ret == 0);
    }
}

ut_test(create_all_and_recreate_them_different_size)
{
    ExamsgID id;
    char memory[(EXAMSG_LAST_ID + 1) * 1024];
    ExaMsgReservedObj *pool;
    pool = examsgPoolInit(memory, sizeof(memory));

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjCreate(pool, id, (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1));
	UT_ASSERT(ret == 0);
    }

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjDelete(pool, id);
	UT_ASSERT(ret == 0);
    }

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjCreate(pool, id, 42);
	UT_ASSERT(ret == 0);
    }
}

ut_test(check_the_pool_does_not_get_fragmented)
{
    ExamsgID id;
    char memory[(EXAMSG_LAST_ID + 1) * 1024];
    ExaMsgReservedObj *pool;
    pool = examsgPoolInit(memory, sizeof(memory));

    /* Create all */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	void *data;
	size_t size = (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1);
	int ret = examsgObjCreate(pool, id, size);
	UT_ASSERT(ret == 0);
	/* fill with really interesting data */
	data = examsgObjAddr(pool, id);
	memset(data, id, size);
    }

    /* delete half of boxes */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id += 2)
    {
	int ret = examsgObjDelete(pool, id);
	UT_ASSERT(ret == 0);
    }

    /* Check data of boxes is still good */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	char *c, *data;
	size_t size = (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1);
	data = examsgObjAddr(pool, id);
	UT_ASSERT(!data || id % 2);
	if (!data)
	    continue;
	for (c = data; c < data + size; c++)
	    UT_ASSERT(*c == id);
    }

    /* recreate boxes */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id += 2)
    {
	void *data;
	int ret = examsgObjCreate(pool, id, 42);
	UT_ASSERT(ret == 0);
	/* fill with really interesting data */
	data = examsgObjAddr(pool, id);
	memset(data, id, 42);
    }

    /* Recheck all */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	char *c, *data;
	size_t size = id % 2 ? (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1) : 42;
	data = examsgObjAddr(pool, id);
	UT_ASSERT(data);
	for (c = data; c < data + size; c++)
	    UT_ASSERT(*c == id);
    }
    /* Delete all */
    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjDelete(pool, id);
	UT_ASSERT(ret == 0);
    }
}

ut_test(create_already_existant_objects)
{
    ExamsgID id;
    char memory[(EXAMSG_LAST_ID + 1) * 1024];
    ExaMsgReservedObj *pool;
    pool = examsgPoolInit(memory, sizeof(memory));

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjCreate(pool, id, (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1));
	UT_ASSERT(ret == 0);
    }

    for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	int ret = examsgObjCreate(pool, id, (sizeof(memory) - 4096) / (EXAMSG_LAST_ID + 1));
	UT_ASSERT(ret == -EEXIST);
    }
}

ut_test(access_inexistant_data)
{
    ExamsgID id;
    char memory[(EXAMSG_LAST_ID + 1) * 1024];
    ExaMsgReservedObj *pool;
    pool = examsgPoolInit(memory, sizeof(memory));

   for (id = EXAMSG_FIRST_ID; id <= EXAMSG_LAST_ID; id++)
    {
	void *data = examsgObjAddr(pool, id);
	UT_ASSERT(data == NULL);
    }
}


