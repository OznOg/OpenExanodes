/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/* FIXME Split this unit test - e.g. the error_messages section should be in
         a separate unit test */

#include <unit_testing.h>

#include "token_manager/tm_server/include/tm_server.h"
#include "token_manager/tm_server/include/tm_file.h"
#include "token_manager/tm_server/src/tm_tokens.h"
#include "token_manager/tm_server/src/tm_err.h"

#include "os/include/os_dir.h"
#include "os/include/os_random.h"
#include "os/include/os_file.h"

#include <sys/stat.h>

#define TOKEN_UUID  "12345678:12345678:12345678:12345678"

#define NODE_1  3
#define NODE_2  0

/* Helper function to avoid setting IP */
int __set_holder(const exa_uuid_t *uuid, exa_nodeid_t node_id)
{
    return tm_tokens_set_holder(uuid, node_id, "127.0.0.1");
}

UT_SECTION(error_messages)

ut_test(all_codes_have_a_different_message)
{
    const char *messages[TM_ERR__LAST - TM_ERR__FIRST + 1];
    tm_err_t e;

    for (e = TM_ERR__FIRST; e <= TM_ERR__LAST; e++)
    {
        tm_err_t k;

        messages[e] = tm_err_str(e);
        UT_ASSERT(messages[e] != NULL);
        for (k = TM_ERR__FIRST; k < e; k++)
            UT_ASSERT(strcmp(messages[k], messages[e]) != 0);
    }
}

ut_test(wrong_error_returns_null)
{
    UT_ASSERT(tm_err_str(TM_ERR__LAST + 1) == NULL);
    UT_ASSERT(tm_err_str(TM_ERR__FIRST - 1) == NULL);
}

UT_SECTION(setting_holder)

ut_setup()
{
    os_random_init();
    tm_tokens_init();
}

ut_cleanup()
{
    tm_tokens_cleanup();
    os_random_cleanup();
}

ut_test(set_holder_of_non_existent_token_succeeds)
{
    exa_uuid_t uuid;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);

    UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(1, tm_tokens_count());
}

ut_test(set_same_holder_twice_succeeds)
{
    exa_uuid_t uuid;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);

    UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(1, tm_tokens_count());
}

ut_test(set_holder_while_token_already_held_returns_ANOTHER_HOLDER)
{
    exa_uuid_t uuid;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);

    UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(-TM_ERR_ANOTHER_HOLDER, __set_holder(&uuid, NODE_2));
}

ut_test(setting_too_many_tokens_returns_TOO_MANY_TOKENS)
{
    exa_uuid_t uuid;
    int i;

    /* Using the same node id for different tokens/cluster doesn't matter */
    for (i = 0; i < TM_TOKENS_MAX; i++)
    {
        os_get_random_bytes(&uuid, sizeof(uuid));
        UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    }
    UT_ASSERT_EQUAL(TM_TOKENS_MAX, tm_tokens_count());

    os_get_random_bytes(&uuid, sizeof(uuid));
    UT_ASSERT_EQUAL(-TM_ERR_TOO_MANY_TOKENS, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(TM_TOKENS_MAX, tm_tokens_count());
}

UT_SECTION(getting)

ut_setup()
{
    tm_tokens_init();
}

ut_cleanup()
{
    tm_tokens_cleanup();
}

ut_test(getting_unexisting_token_holder_returns_NO_SUCH_TOKEN)
{
    exa_uuid_t uuid;
    exa_nodeid_t holder_id;

    /* cleanup existing tokens */
    tm_tokens_cleanup();

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);
    UT_ASSERT(tm_tokens_get_holder(&uuid, &holder_id) == -TM_ERR_NO_SUCH_TOKEN);
}

UT_SECTION(releasing)

ut_setup()
{
    tm_tokens_init();
}

ut_cleanup()
{
    tm_tokens_cleanup();
}

ut_test(releasing_non_existent_token_returns_NO_SUCH_TOKEN)
{
    exa_uuid_t uuid;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);

    UT_ASSERT_EQUAL(-TM_ERR_NO_SUCH_TOKEN, tm_tokens_release(&uuid, NODE_1));
    UT_ASSERT_EQUAL(0, tm_tokens_count());
}

ut_test(releasing_token_by_another_than_holder_returns_NOT_HOLDER)
{
    exa_uuid_t uuid;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);

    UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(-TM_ERR_NOT_HOLDER, tm_tokens_release(&uuid, NODE_2));
    UT_ASSERT_EQUAL(1, tm_tokens_count());
}

ut_test(releasing_token_by_holder_succeeds)
{
    exa_uuid_t uuid;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);

    UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(1, tm_tokens_count());

    UT_ASSERT_EQUAL(0, tm_tokens_release(&uuid, NODE_1));
    UT_ASSERT_EQUAL(0, tm_tokens_count());
}

ut_test(force_releasing_token_always_succeeds)
{
    exa_uuid_t uuid;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);

    UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(1, tm_tokens_count());

    tm_tokens_force_release(&uuid);
    UT_ASSERT_EQUAL(0, tm_tokens_count());

    tm_tokens_force_release(&uuid);
    UT_ASSERT_EQUAL(0, tm_tokens_count());
}

UT_SECTION(saving_and_loading)

#define TOKEN_FILE  "__tokens__"
#define UNEXISTING_FILE  "__not_there__"

ut_setup()
{
    os_random_init();
    tm_tokens_init();

    unlink(TOKEN_FILE);
}

ut_cleanup()
{
    unlink(TOKEN_FILE);

    tm_tokens_cleanup();
    os_random_cleanup();
}

ut_test(saving_and_reading_no_tokens)
{
    struct stat st;

    UT_ASSERT_EQUAL(0, tm_tokens_save(TOKEN_FILE));

    /* Check file is gone */
    UT_ASSERT_EQUAL(-1, stat(TOKEN_FILE, &st));
    UT_ASSERT_EQUAL(ENOENT, errno);

    UT_ASSERT_EQUAL(0, tm_tokens_load(TOKEN_FILE));
    UT_ASSERT_EQUAL(0, tm_tokens_count());
}

ut_test(saving_and_reading_one_token)
{
    exa_uuid_t uuid;
    exa_nodeid_t holder_id;
    struct stat st;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);

    UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(0, tm_tokens_save(TOKEN_FILE));

    /* Check file is non-empty */
    UT_ASSERT_EQUAL(0, stat(TOKEN_FILE, &st));
    UT_ASSERT(st.st_size > 0);

    /* cleanup existing tokens */
    tm_tokens_cleanup();

    UT_ASSERT_EQUAL(0, tm_tokens_load(TOKEN_FILE));
    UT_ASSERT_EQUAL(1, tm_tokens_count());
    UT_ASSERT(tm_tokens_get_holder(&uuid, &holder_id) == 0 && holder_id == NODE_1);
}

ut_test(reading_from_unexisting_file_reads_zero_tokens)
{
    unlink(UNEXISTING_FILE);
    UT_ASSERT_EQUAL(0, tm_tokens_load(UNEXISTING_FILE));
    UT_ASSERT_EQUAL(0, tm_tokens_count());
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

    exa_uuid_t uuid;

    UT_ASSERT(uuid_scan(TOKEN_UUID, &uuid) == 0);

    UT_ASSERT_EQUAL(0, __set_holder(&uuid, NODE_1));
    UT_ASSERT_EQUAL(1, tm_tokens_count());

    /* Ensure the path does not exist by deleting the whole tree */
    UT_ASSERT_EQUAL(0, os_dir_remove_tree(UNEXISTENT_ROOT));

    UT_ASSERT_EQUAL(0, tm_tokens_save(UNEXISTENT_PATH));

    /* Check the file was saved indeed */
    tm_tokens_cleanup();
    UT_ASSERT_EQUAL(0, tm_tokens_load(UNEXISTENT_PATH));

    os_dir_remove_tree(UNEXISTENT_ROOT);
}

/* This function writes a broken token file consisting of:
 * start_magic
 * tok_num
 * (zero tokens, even if tok_num != 0)
 * end_magic
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

    /* cleanup existing tokens */
    tm_tokens_cleanup();

    /* Manually write a good file */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE, TM_TOKENS_FILE_MAGIC_NUMBER,
                       TM_TOKENS_FILE_FORMAT_VERSION,
                       0, TM_TOKENS_FILE_MAGIC_NUMBER));

    UT_ASSERT_EQUAL(0, tm_tokens_load(TOKEN_FILE));
    UT_ASSERT_EQUAL(0, tm_tokens_count());

    /* Manually write a file with broken start magic */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE, wrong_magic,
                       TM_TOKENS_FILE_FORMAT_VERSION,
                       0, TM_TOKENS_FILE_MAGIC_NUMBER));

    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_MAGIC, tm_tokens_load(TOKEN_FILE));
    UT_ASSERT_EQUAL(0, tm_tokens_count());

    /* Manually write a file with broken end magic */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE, TM_TOKENS_FILE_MAGIC_NUMBER,
                       TM_TOKENS_FILE_FORMAT_VERSION,
                       0, wrong_magic));

    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_MAGIC, tm_tokens_load(TOKEN_FILE));
    UT_ASSERT_EQUAL(0, tm_tokens_count());

    /* Manually write a file with wrong token count */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE, TM_TOKENS_FILE_MAGIC_NUMBER,
                       TM_TOKENS_FILE_FORMAT_VERSION,
                       10, TM_TOKENS_FILE_MAGIC_NUMBER));

    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_TOKENS_NUMBER, tm_tokens_load(TOKEN_FILE));
    UT_ASSERT_EQUAL(0, tm_tokens_count());

    /* Manually write a file with negative version */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE, TM_TOKENS_FILE_MAGIC_NUMBER,
                       -1,
                       0, TM_TOKENS_FILE_MAGIC_NUMBER));

    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_FORMAT_VERSION, tm_tokens_load(TOKEN_FILE));
    UT_ASSERT_EQUAL(0, tm_tokens_count());

    /* Manually write a file with a too big version */
    UT_ASSERT_EQUAL(0, break_file(TOKEN_FILE, TM_TOKENS_FILE_MAGIC_NUMBER,
                       TM_TOKENS_FILE_FORMAT_VERSION + 1,
                       0, TM_TOKENS_FILE_MAGIC_NUMBER));

    UT_ASSERT_EQUAL(-TM_ERR_WRONG_FILE_FORMAT_VERSION, tm_tokens_load(TOKEN_FILE));
    UT_ASSERT_EQUAL(0, tm_tokens_count());
}
