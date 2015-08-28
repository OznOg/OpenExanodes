/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "token_manager/tm_server/include/tm_file.h"
#include "token_manager/tm_server/src/tm_err.h"

#include "os/include/os_dir.h"
#include "os/include/os_random.h"
#include "os/include/os_file.h"

#include <sys/stat.h>

#define TOKEN_FILE  "__tokens__"
#define UNEXISTING_FILE  "__not_there__"

#define TOKEN_UUID  "12345678:12345678:12345678:12345678"

#define NODE_1  3
#define NODE_2  0

#define N_TOKENS  5

static token_t tokens[N_TOKENS];

ut_setup()
{
    int i;

    os_random_init();
    unlink(TOKEN_FILE);

    for (i = 0; i < N_TOKENS; i++)
    {
        uuid_zero(&tokens[i].uuid);
        tokens[i].holder = EXA_NODEID_NONE;
    }
}

ut_cleanup()
{
    unlink(TOKEN_FILE);
    os_random_cleanup();
}

ut_test(saving_and_reading_no_tokens)
{
    struct stat st;
    uint64_t count;

    UT_ASSERT_EQUAL(0, tm_file_save(TOKEN_FILE, tokens, 0));

    /* Check file is gone */
    UT_ASSERT_EQUAL(-1, stat(TOKEN_FILE, &st));
    UT_ASSERT_EQUAL(ENOENT, errno);

    count = N_TOKENS;
    UT_ASSERT_EQUAL(0, tm_file_load(TOKEN_FILE, tokens, &count));
    UT_ASSERT_EQUAL(0, count);
}

ut_test(saving_and_reading_one_token)
{
    token_t tokens2[N_TOKENS];
    uint64_t count;
    struct stat st;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &tokens[0].uuid) == 0);
    tokens[0].holder = NODE_1;

    UT_ASSERT_EQUAL(0, tm_file_save(TOKEN_FILE, tokens, N_TOKENS));

    /* Check file is non-empty */
    UT_ASSERT_EQUAL(0, stat(TOKEN_FILE, &st));
    UT_ASSERT(st.st_size > 0);

    count = N_TOKENS;
    UT_ASSERT_EQUAL(0, tm_file_load(TOKEN_FILE, tokens2, &count));
    UT_ASSERT_EQUAL(1, count);

    UT_ASSERT(uuid_is_equal(&tokens2[0].uuid, &tokens[0].uuid));
    UT_ASSERT(tokens2[0].holder == NODE_1);
}

ut_test(reading_from_unexisting_file_reads_zero_tokens)
{
    uint64_t count;

    unlink(UNEXISTING_FILE);

    count = N_TOKENS;
    UT_ASSERT_EQUAL(0, tm_file_load(UNEXISTING_FILE, tokens, &count));
    UT_ASSERT_EQUAL(0, count);
}

ut_test(saving_to_unexisting_path_creates_path_and_succeeds)
{
#ifndef WIN32
#define UNEXISTENT_PREFIX  "/tmp"
#else
#define UNEXISTENT_PREFIX  "C:"
#endif

#define UNEXISTENT_ROOT  UNEXISTENT_PREFIX OS_FILE_SEP "plif"
#define UNEXISTENT_PATH  UNEXISTENT_ROOT OS_FILE_SEP "plaf" OS_FILE_SEP "plouf.toks"

    token_t tokens2[N_TOKENS];
    uint64_t count;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &tokens[0].uuid) == 0);
    tokens[0].holder = NODE_1;

    /* Ensure the path does not exist by deleting the whole tree */
    UT_ASSERT_EQUAL(0, os_dir_remove_tree(UNEXISTENT_ROOT));

    UT_ASSERT_EQUAL(0, tm_file_save(UNEXISTENT_PATH, tokens, N_TOKENS));

    /* Check the file was saved indeed */
    count = N_TOKENS;
    UT_ASSERT_EQUAL(0, tm_file_load(UNEXISTENT_PATH, tokens2, &count));

    os_dir_remove_tree(UNEXISTENT_ROOT);
}

/* This function writes a broken token file consisting of:
 *   - start_magic
 *   - tok_num
 *   - (zero tokens, even if tok_num != 0)
 *   - end_magic
 */
static int break_file(const char *filename, uint64_t start_magic,
                      tm_tokens_file_version_t file_version, uint64_t tok_num,
                      uint64_t end_magic)
{
    FILE *f = fopen(filename, "wb");
    int r;

    if (f == NULL)
        return -errno;

    r = fwrite(&start_magic, sizeof(start_magic), 1, f);
    if (r != 1)
    {
        r = -errno;
        fclose(f);
        return r;
    }

    r = fwrite(&file_version, sizeof(file_version), 1, f);
    if (r != 1)
    {
        r = -errno;
        fclose(f);
        return r;
    }

    r = fwrite(&tok_num, sizeof(tok_num), 1, f);
    if (r != 1)
    {
        r = -errno;
        fclose(f);
        return r;
    }

    r = fwrite(&end_magic, sizeof(end_magic), 1, f);
    if (r != 1)
    {
        r = -errno;
        fclose(f);
        return r;
    }

    r = fclose(f);
    if (r != 0)
        return -errno;

    return 0;
}

ut_test(saving_with_wrong_contents_makes_reading_fail)
{
    uint64_t wrong_magic = 0x1234123412341234;
    uint64_t count;

    /* Manually write a good file */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE,
                                  TM_TOKENS_FILE_MAGIC_NUMBER,
                                  TM_TOKENS_FILE_FORMAT_VERSION,
                                  0,
                                  TM_TOKENS_FILE_MAGIC_NUMBER));

    count = N_TOKENS;
    UT_ASSERT_EQUAL(0, tm_file_load(TOKEN_FILE, tokens, &count));

    /* Manually write a file with broken start magic */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE,
                                  wrong_magic,
                                  TM_TOKENS_FILE_FORMAT_VERSION,
                                  0,
                                  TM_TOKENS_FILE_MAGIC_NUMBER));

    count = N_TOKENS;
    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_MAGIC,
                    tm_file_load(TOKEN_FILE, tokens, &count));

    /* Manually write a file with broken end magic */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE,
                                  TM_TOKENS_FILE_MAGIC_NUMBER,
                                  TM_TOKENS_FILE_FORMAT_VERSION,
                                  0,
                                  wrong_magic));

    count = N_TOKENS;
    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_MAGIC,
                    tm_file_load(TOKEN_FILE, tokens, &count));

    /* Manually write a file with wrong token count */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE,
                                  TM_TOKENS_FILE_MAGIC_NUMBER,
                                  TM_TOKENS_FILE_FORMAT_VERSION,
                                  N_TOKENS,
                                  TM_TOKENS_FILE_MAGIC_NUMBER));

    count = N_TOKENS;
    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_TOKENS_NUMBER,
                    tm_file_load(TOKEN_FILE, tokens, &count));

    /* Manually write a file with negative version */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE,
                                  TM_TOKENS_FILE_MAGIC_NUMBER,
                                  -1,
                                  0,
                                  TM_TOKENS_FILE_MAGIC_NUMBER));

    count = N_TOKENS;
    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_FORMAT_VERSION,
                    tm_file_load(TOKEN_FILE, tokens, &count));

    /* Manually write a file with a too big version */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE,
                                  TM_TOKENS_FILE_MAGIC_NUMBER,
                                  TM_TOKENS_FILE_FORMAT_VERSION + 1,
                                  0,
                                  TM_TOKENS_FILE_MAGIC_NUMBER));

    count = N_TOKENS;
    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_FORMAT_VERSION,
                    tm_file_load(TOKEN_FILE, tokens, &count));
}
