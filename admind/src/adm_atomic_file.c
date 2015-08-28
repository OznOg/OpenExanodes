/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "adm_atomic_file.h"

#include "common/include/exa_error.h"
#include "common/include/exa_constants.h"

#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_string.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"


/** Suffix for temporary file */
#define ATOMIC_FILE_TEMP_SUFFIX  ".new"

/* FIXME: This function should go to libos and:
 * - the Windows implementation should use TxF, the transactional FS API
 *   because rename() is not atomic,
 * - the Linux implementation should do a fsync() on the file and on its
 *   instead of a fflush() because fflush() flushes only user space buffers,
 *   not kernel space ones.
 */
int adm_atomic_file_save(const char *filename, const void *data,
                         size_t data_size)
{
    char path[OS_PATH_MAX];
    size_t path_len;
    const char *sep;
    char tmp_filename[OS_PATH_MAX + strlen(ATOMIC_FILE_TEMP_SUFFIX)];
    FILE *file = NULL;
    int fd = -1;
    int err;

    /* FIXME Should be os-specific path separator */
    sep = strrchr(filename, *OS_FILE_SEP);
    if (sep == NULL)
    {
        /* Must have a path */
        err = -EINVAL;
        goto done;
    }

    path_len = sep - filename + 1;
    if (path_len > sizeof(path) - 1)
    {
        err = -ENAMETOOLONG;
        goto done;
    }
    strlcpy(path, filename, path_len);

    /* Create a new file */
    if (os_snprintf(tmp_filename, sizeof(tmp_filename), "%s%s", filename,
		    ATOMIC_FILE_TEMP_SUFFIX) >= sizeof(tmp_filename))
    {
        err = -ENAMETOOLONG;
        goto done;
    }

    file = fopen(tmp_filename, "wb");
    if (file == NULL)
    {
        err = -errno;
        goto done;
    }

    /* Retrieve the file descriptor */
    fd = fileno(file);
    if (fd == -1)
    {
        err = -errno;
        goto done;
    }

    /* Disable buffering */
    if (setvbuf(file, NULL, _IONBF, 0) != 0)
    {
        err = -errno;
        goto done;
    }

    /* Write the data */
    if (fwrite(data, data_size, 1, file) != 1)
    {
        err = -errno;
        goto done;
    }

    /* Sync file data and metadata on disk */
    if (fflush(file) != 0)
    {
        err = -errno;
        goto done;
    }

    err = fclose(file);
    file = NULL;
    if (err != 0)
    {
        err = -errno;
        goto done;
    }

    /* Atomically switch to the new file */
    err = os_file_rename(tmp_filename, filename);
    if (err != 0)
        goto done;

    return EXA_SUCCESS;

done:

    if (file != NULL)
        fclose(file);

    return err;
}
