/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file
 * \brief Ring buffer test routines
 */

#include <unit_testing.h>

#include "examsg/src/ringbuf.h"
#include "examsg/include/examsg.h"
#include "log/include/log.h"

#include "os/include/os_error.h"
#include "os/include/os_random.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define RNG_SIZE	10240

#define MSG_SIZE(x) \
    (sizeof(struct examsg_blkhead) + sizeof(examsg_blktail) + x)

/* Code intentionnally left (and #ifdef'd out) to ease the debugging
 * of the ring buffer in the event of the unit test failing. */
#if 0
/**
 * Dump a message taken from the ring buffer.
 *
 * \param[in] idx   Index in the ring buffer
 * \param[in] msg   Message
 * \param[in] size  Message size
 * \param[in] mid   Message id
 *
 * Use this function as follows:
 *     examsgRngDump(r, (ExaRingDumpMsgFn)&dump_message);
 * where r is the ring buffer to dump.
 */
static void dump_message(int idx, const char *msg, size_t size,
                         const ExamsgMID *mid)
{
    /* Size chosen so that we can print even a message as big as the ring
     * buffer */
    char str[RNG_SIZE + 1];

    memcpy(str, msg, size);
    str[size] = '\0';

    ut_printf("%d: msg='%s' size=%"PRIzu, idx, str, size);
}
#endif

ut_setup()
{
    exalog_as(EXAMSG_TEST_ID);
    os_random_init();
}

ut_cleanup()
{
    /* Nothing to do */
}

ut_test(filling_ringbuffer_and_checking_size_is_correct)
{
#define MAX_DUMMY_MSG  10
    typedef struct
    {
        char data[20];
    } dummy_msg_t;
    dummy_msg_t dummy_msg;
    size_t ringbuf_size;
    exa_ringbuf_t *r;
    int count;

    ringbuf_size = examsgRngMemSize(MAX_DUMMY_MSG, sizeof(dummy_msg_t));
    r = malloc(ringbuf_size);
    UT_ASSERT_VERBOSE(r != NULL, "alloc ringbuf %d messages x %"PRIzu" bytes"
                      " => %"PRIzu" bytes", MAX_DUMMY_MSG, sizeof(dummy_msg_t),
                      ringbuf_size);

    examsgRngInit(r, ringbuf_size);
    for (count = 0; count < MAX_DUMMY_MSG; count++)
    {
        int s = examsgRngPut(r, (const char *)&dummy_msg, sizeof(dummy_msg), NULL);
        UT_ASSERT_VERBOSE(s == sizeof(dummy_msg),
                          "ring buffer able to store only %d messages"
                          " instead of required %d", count, MAX_DUMMY_MSG);
    }
    UT_ASSERT_VERBOSE(count == MAX_DUMMY_MSG,
                      "ring buffer not able to store %d messages",
                      MAX_DUMMY_MSG);

    free(r);
}

ut_test(put_and_get_of_randomly_sized_messages)
{
    exa_ringbuf_t *r;
    char buf[2*RNG_SIZE];
    int cRd, cWr;
    int s;
    int count;

    char *set1;
    size_t size1;
    char *set2;
    size_t size2;
    char *set3;
    size_t size3;

    r = malloc(examsgRngMemSize(1, RNG_SIZE));
    UT_ASSERT(r != NULL);

    examsgRngInit(r, RNG_SIZE);
    cRd = r->pRd;
    cWr = r->pWr;

    /* 1000 tests (value arbitrarily chosen) */
    for (count = 0; count < 1000; count++)
    {
        s = examsgRngPut(r, buf, RNG_SIZE * 2, NULL);
        UT_ASSERT_VERBOSE(s == -ENOSPC, "too big a message (%d bytes)"
                          " was able to fit in ring buffer (%d bytes)!",
                          RNG_SIZE * 2, RNG_SIZE);

        size1 = (os_drand() * RNG_SIZE) / 4 + 2; /* 2 for testing buffer too small */
        set1 = malloc(size1);
        os_get_random_bytes(set1, size1);

        size2 = (os_drand() * RNG_SIZE) / 4 + 1;
        set2 = malloc(size2);
        os_get_random_bytes(set2, size2);

        size3 = (os_drand() * RNG_SIZE) / 4 + 1;
        set3 = malloc(size3);
        os_get_random_bytes(set3, size3);

        s = examsgRngPut(r, set1, size1, NULL);
        UT_ASSERT_VERBOSE(s == size1, "examsgRngPut: returned %d, expected %"PRIzu,
                          s, size1);

        cWr += MSG_SIZE(size1);
        cWr %= r->size;

        UT_ASSERT_VERBOSE(r->pRd == cRd, "read pointer is %d, expected %d", r->pRd, cRd);
        UT_ASSERT_VERBOSE(r->pWr == cWr, "write pointer is %d, expected %d", r->pWr, cWr);

        /* Try to get message in a too small buffer */
        s = examsgRngGet(r, buf, size1 - 1 /* too small*/, NULL);
        UT_ASSERT_VERBOSE(s == -EMSGSIZE, "was able to get message (%"PRIzu" bytes)"
                          " in too small a buffer (%"PRIzu" bytes)",
                          size1, size1 - 1);

        s = examsgRngGet(r, buf, sizeof(buf), NULL);
        UT_ASSERT_VERBOSE(s == size1, "examsgRngGet: returned %d, expected %"PRIzu,
                          s, size1);
        UT_ASSERT_VERBOSE(memcmp(buf, set1, size1) == 0,
                          "got unexpected message contents");

        cRd += MSG_SIZE(size1);
        cRd %= r->size;

        UT_ASSERT_VERBOSE(r->pRd == cRd, "read pointer is %d, expected %d", r->pRd, cRd);
        UT_ASSERT_VERBOSE(r->pWr == cWr, "write pointer is %d, expected %d", r->pWr, cWr);

        s = examsgRngGet(r, buf, sizeof(buf), NULL);
        UT_ASSERT_VERBOSE(s == 0, "supposedly read all messages,"
                          " but ring buffer seems to be not empty");

        UT_ASSERT_VERBOSE(r->pRd == cRd, "read pointer is %d, expected %d", r->pRd, cRd);
        UT_ASSERT_VERBOSE(r->pWr == cWr, "write pointer is %d, expected %d", r->pWr, cWr);

        free(set1);
        free(set2);
        free(set3);
    }

    free(r);
}

ut_test(put_message_one_byte_at_a_time_and_read_it_whole_at_once)
{
    exa_ringbuf_t *r;
    int s;
    char str[] = "Hello World !";
    char res[sizeof(str)];
    size_t un = 1; /**<  Param size MUST BE a size_t; check function proto */

    r = malloc(examsgRngMemSize(1, RNG_SIZE));
    UT_ASSERT(r != NULL);

    examsgRngInit(r, RNG_SIZE);

    s = examsgRngPut(r,
                     str, un,
                     str + 1, un,
                     str + 2, un,
                     str + 3, un,
                     str + 4, un,
                     str + 5, un,
                     str + 6, un,
                     str + 7, un,
                     str + 8, un,
                     str + 9, un,
                     str + 10, un,
                     str + 11, un,
                     str + 12, un,
                     str + 13, un);
    UT_ASSERT_VERBOSE(s == sizeof(str), "examsgRngPut: got %d, expected %"PRIzu,
                      s, sizeof(str));

    memset(res, 0, sizeof(res));
    s = examsgRngGet(r, res, sizeof(res), NULL);
    UT_ASSERT(s == sizeof(str));
    UT_ASSERT_VERBOSE(strncmp(str, res, sizeof(str)) == 0,
                      "examsgRngGet: got '%s', expected '%s'", res, str);

    free(r);
}

ut_test(put_whole_message_at_once_and_read_it_one_byte_at_a_time)
{
    exa_ringbuf_t *r;
    int s;
    char str[] = "Hello World !";
    char res[sizeof(str)];
    size_t un = 1; /**<  Param size MUST be a size_t; check function proto */

    r = malloc(examsgRngMemSize(1, RNG_SIZE));
    UT_ASSERT(r != NULL);

    examsgRngInit(r, RNG_SIZE);

    s = examsgRngPut(r, str, sizeof(str), NULL);
    UT_ASSERT_VERBOSE(s == sizeof(str), "examsgRngPut: got %d, expected %"PRIzu,
                      s, sizeof(str));

    memset(res, 0, sizeof(res));
    s = examsgRngGet(r,
                     res, un,
                     res + 1, un,
                     res + 2, un,
                     res + 3, un,
                     res + 4, un,
                     res + 5, un,
                     res + 6, un,
                     res + 7, un,
                     res + 8, un,
                     res + 9, un,
                     res + 10, un,
                     res + 11, un,
                     res + 12, un,
                     res + 13, un);
    UT_ASSERT(s == sizeof(str));
    UT_ASSERT_VERBOSE(strncmp(str, res, sizeof(str)) == 0,
                      "examsgRngGet: got '%s', expected '%s'", res, str);

    free(r);
}
