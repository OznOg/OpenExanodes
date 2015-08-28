/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <sys/stat.h>
#include <string.h>
#include "os/include/os_error.h"
#include "os/include/os_dir.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "admind/src/adm_file_ops.h"

#include "log/include/log.h"

char *adm_file_read_to_str(const char *path, cl_error_desc_t *err_desc)
{
    struct stat stat;
    FILE *fp;
    char *buffer;

    EXA_ASSERT(err_desc != NULL);

    set_error(err_desc, EXA_SUCCESS, "Success");

    if (path == NULL)
    {
        set_error(err_desc, -ENOENT, "Can not open NULL path");
        return NULL;
    }

    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        set_error(err_desc, -errno, "Failed to open the file %s.", path);
        return NULL;
    }

    if (fstat(fileno(fp), &stat) != 0)
    {
        set_error(err_desc, -errno, "Failed to stat the file %s.", path);
        fclose(fp);
        return NULL;
    }

    buffer = os_malloc(stat.st_size + 1);
    if (buffer == NULL)
    {
        set_error(err_desc, -ENOMEM, "Failed to allocate memory when reading file %s.", path);
        fclose(fp);
        return NULL;
    }

    if (fread(buffer, stat.st_size, 1, fp) != 1)
    {
        set_error(err_desc, -errno, "Failed to read the file %s.", path);
        os_free(buffer);
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    buffer[stat.st_size] = '\0';

    return buffer;
}

void adm_file_write_from_str(const char *path, const char *contents, cl_error_desc_t *err_desc)
{
    FILE *fp = NULL;

    EXA_ASSERT(err_desc != NULL);

    set_error(err_desc, EXA_SUCCESS, "Success");

    fp = fopen(path, "wb");
    if (fp == NULL)
    {
        set_error(err_desc, -errno, "Failed to open the file %s for writing.", path);
        return;
    }

    if (fwrite(contents, strlen(contents), 1, fp) != 1)
    {
        set_error(err_desc, -errno, "Failed to write the file %s.", path);
        fclose(fp);
        return;
    }

    if (fclose(fp) != 0)
    {
        set_error(err_desc, -errno, "Failed to close the file %s.", path);
        return;
    }
}
