/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "token_manager/tm_server/include/tm_file.h"
#include "token_manager/tm_server/src/tm_err.h"

#include "os/include/os_assert.h"
#include "os/include/os_dir.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/os_string.h"

#include <stdlib.h>
#include <string.h>  /* for memcpy() */

static int __file_read(FILE *f, void *data, size_t size)
{
    int r;

    do
        r = fread(data, size, 1, f);
    while (r != 1 && errno == EINTR);

    return r;
}

static int __file_write(FILE *f, const void *data, size_t size)
{
    int r;

    do
        r = fwrite(data, size, 1, f);
    while (r != 1 && errno == EINTR);

    return r;
}

/**
 * Read a 64-bits number from a file descriptor. This function
 * handles EINTR itself.
 *
 * @param[in] f         The file descriptor
 * @param[out] number   The number read
 *
 * @return fread's return value (1 if the read
 *         completed, 0 otherwise)
 */
static int read_number(FILE *f, uint64_t *number)
{
    OS_ASSERT(number != NULL);
    return __file_read(f, number, sizeof(*number));
}

/**
 * Read a tm_tokens_file_version_t from a file descriptor. This function
 * handles EINTR itself.
 *
 * @param[in] f         The file descriptor
 * @param[out] version   The version read
 *
 * @return fread's return value (1 if the read
 *         completed, 0 otherwise)
 */
static int read_version(FILE *f, tm_tokens_file_version_t *version)
{
    OS_ASSERT(version != NULL);
    return __file_read(f, version, sizeof(*version));
}

/**
 * Read a token from a file descriptor. This function
 * handles EINTR itself.
 *
 * @param[in] f         The file descriptor
 * @param[in] token     The token read
 *
 * @return fread's return value (1 if the read
 *         completed, 0 otherwise)
 */
static int read_token(FILE *f, token_t *token)
{
    OS_ASSERT(token != NULL);
    return __file_read(f, token, sizeof(*token));
}

/**
 * Write a 64-bits number to a file descriptor. This function
 * handles EINTR itself.
 *
 * @param[in] f         The file descriptor
 * @param[in] number    The version to write
 *
 * @return fwrite's return value (1 if the write
 *         completed, 0 otherwise)
 */
static int write_number(FILE *f, uint64_t number)
{
    return __file_write(f, &number, sizeof(number));
}

/**
 * Write a 64-bits tm_tokens_file_version_t to a file descriptor. This function
 * handles EINTR itself.
 *
 * @param[in] f         The file descriptor
 * @param[in] version    The version to write
 *
 * @return fwrite's return value (1 if the write
 *         completed, 0 otherwise)
 */
static int write_version(FILE *f, tm_tokens_file_version_t version)
{
    return __file_write(f, &version, sizeof(version));
}

/**
 * Write a token to a file descriptor. This function
 * handles EINTR itself.
 *
 * @param[in] f         The file descriptor
 * @param[in] token     The token to write
 *
 * @return fwrite's return value (1 if the write
 *         completed, 0 otherwise)
 */
static int write_token(FILE *f, const token_t *token)
{
    return __file_write(f, token, sizeof(*token));
}

/* As of version 1, tokens file format is :
 * TM_TOKENS_FILE_MAGIC_NUMBER
 * TM_TOKENS_FILE_FORMAT_VERSION
 * num_tokens
 * token[0]
 * token[...]
 * token[num_tokens-1]
 * TM_TOKENS_FILE_MAGIC_NUMBER
 *
 * Stuff between the format version and the ending magic number
 * can be changed when incrementing format version.
 */
int tm_file_load(const char *filename, token_t *tokens, uint64_t *count)
{
    FILE *f;
    token_t tok;
    int i, r, result;
    uint64_t magic;
    uint64_t num_tokens_in_file, num_tokens_read = 0;
    tm_tokens_file_version_t file_version;

    OS_ASSERT(filename != NULL);
    OS_ASSERT(*count > 0);

    /* Open file */
    do
        f = fopen(filename, "rb");
    while (f == NULL && errno == EINTR);

    /* It's OK if it doesn't exist : there are no tokens */
    if (f == NULL && errno == ENOENT)
    {
        *count = 0;
        return 0;
    }

    if (f == NULL)
        return -errno;

    /* Read magic number */
    r = read_number(f, &magic);
    if (r != 1)
    {
        result = -errno;
        goto out_err_read;
    }

    /* verify it */
    if (magic != TM_TOKENS_FILE_MAGIC_NUMBER)
    {
        result = -TM_ERR_WRONG_FILE_MAGIC;
        goto out_err_read;
    }

    /* Read format version */
    r = read_version(f, &file_version);
    if (r != 1)
    {
        result = -errno;
        goto out_err_read;
    }

    if (!TM_TOKENS_FILE_FORMAT_VERSION_IS_VALID(file_version))
    {
        result = -TM_ERR_WRONG_FILE_FORMAT_VERSION;
        goto out_err_read;
    }

    /* Read number of tokens to load */
    r = read_number(f, &num_tokens_in_file);
    if (r != 1)
    {
        result = -errno;
        goto out_err_read;
    }
    if (num_tokens_in_file > *count)
    {
        /* FIXME Use meaningful error code */
        result = -EMSGSIZE;
        goto out_err_read;
    }
    *count = num_tokens_in_file;

    /* load them */
    for (i = 0; i < num_tokens_in_file; i++)
    {
        r = read_token(f, &tok);
        if (r != 1)
        {
            /* Don't rely on errno. Rather, consider the reading went well,
               and the fact that we didn't read as many tokens as expected
               will be catched at out_err_read. */
            result = 0;
            goto out_err_read;
        }

        memcpy(&tokens[i], &tok, sizeof(tokens[i]));
        num_tokens_read++;
    }

    /* Check the file ends with the magic number */
    r = read_number(f, &magic);
    if (r != 1)
    {
        result = -errno;
        goto out_err_read;
    }

    /* And verify it. */
    if (magic != TM_TOKENS_FILE_MAGIC_NUMBER)
    {
        result = -TM_ERR_WRONG_FILE_MAGIC;
        goto out_err_read;
    }

    result = 0;

out_err_read:
    /* Check we've read the planned number of tokens */
    if (result == 0 && num_tokens_read != num_tokens_in_file)
        result = -TM_ERR_WRONG_FILE_TOKENS_NUMBER;

    fclose(f);
    return result;
}

int tm_file_save(const char *filename, const token_t *tokens, uint64_t count)
{
    FILE *f;
    int i, r = 0;
    int write_error = 0;
    int fclose_error = 0;
    char *tempfile;
    int tempfile_len;
    char filepath[OS_PATH_MAX];
    uint64_t magic = TM_TOKENS_FILE_MAGIC_NUMBER;
    tm_tokens_file_version_t file_version = TM_TOKENS_FILE_FORMAT_VERSION;
    uint64_t non_zero_count;

    OS_ASSERT(filename != NULL);
    OS_ASSERT(tokens != NULL);

    non_zero_count = 0;
    for (i = 0; i < count; i++)
        if (!uuid_is_zero(&tokens[i].uuid))
            non_zero_count++;

    if (non_zero_count == 0)
    {
        r = unlink(filename);
        if (r == 0 || errno == ENOENT)
            return 0;

        return -errno;
    }

    tempfile_len = strlen(filename) + strlen(".tmp");
    tempfile = os_malloc(tempfile_len + 1);

    OS_ASSERT(tempfile != NULL);
    OS_ASSERT(os_snprintf(tempfile, tempfile_len + 1, "%s.tmp", filename) == tempfile_len);

    r = os_strlcpy(filepath, filename, OS_PATH_MAX);
    OS_ASSERT(r < OS_PATH_MAX);

    r = os_dir_create_recursive(os_dirname(filepath));
    if (r != 0)
        return -r;

    /* Open the file */
    do
        f = fopen(tempfile, "wb");
    while (f == NULL && errno == EINTR);

    if (f == NULL)
    {
        r = -errno;
        goto free_tempfile;
    }

    /* Write the magic number */
    r = write_number(f, magic);
    if (r != 1)
    {
        write_error = -errno;
        goto out_err_write;
    }

    /* Write the file version */
    r = write_version(f, file_version);
    if (r != 1)
    {
        write_error = -errno;
        goto out_err_write;
    }

    r = write_number(f, non_zero_count);
    if (r != 1)
    {
        write_error = -errno;
        goto out_err_write;
    }

    /* Write non-zero tokens */
    for (i = 0; i < count; i++)
    {
        const token_t *t = &tokens[i];

        if (uuid_is_zero(&t->uuid))
            continue;

        r = write_token(f, t);
        if (r != 1)
        {
            write_error = -errno;
            goto out_err_write;
        }
    }

    /* Write magic number again */
    r = write_number(f, magic);
    if (r != 1)
    {
        write_error = -errno;
    }

out_err_write:
    /* Close file */
    do
        r = fclose(f);
    while (r != 0 && errno == EINTR);

    if (r != 0)
        fclose_error = -errno;

    /* if we got errors both on write and close, we choose to
     * return the first one (the write error).
     */
    if (write_error != 0)
    {
        r = write_error;
        goto free_tempfile;
    }
    if (fclose_error != 0)
    {
        r = fclose_error;
        goto free_tempfile;
    }

    r = os_file_rename(tempfile, filename);
    if (r != 0)
        unlink(tempfile);

free_tempfile:
    os_free(tempfile);

    return r;
}

