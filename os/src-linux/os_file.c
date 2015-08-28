/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <stdarg.h>
#include "os/include/os_file.h"

int os_file_rename(const char *src, const char *dest)
{
    struct stat src_stat, dest_stat;

    if (src == NULL || dest == NULL)
        return -EINVAL;

    /* Check src exists and is not a directory */
    if (stat(src, &src_stat) != 0)
        return -errno;

    if (S_ISDIR(src_stat.st_mode))
        return -EISDIR;

    /* check that destination does not exist, or if it exists
     * that it is not a directory (cannot overwrite a directory
     * with a file). */
    if (stat(dest, &dest_stat) != 0 && errno != ENOENT)
        return -errno;

    if (errno != ENOENT && S_ISDIR(dest_stat.st_mode))
        return -EISDIR;

    if (rename(src, dest) != 0)
        return -errno;

    return 0;
}

char *os_basename(char *path)
{
    return path == NULL ? NULL : basename(path);
}

char *os_program_name(char *path)
{
    return path == NULL ? NULL : basename(path);
}

bool os_is_user_an_admin(void)
{
    return geteuid() == 0;
}

bool os_path_is_absolute(const char *path)
{
    return path != NULL && path[0] == '/';
}
