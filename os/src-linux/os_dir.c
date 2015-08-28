/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_assert.h"
#include "os/include/os_dir.h"
#include "os/include/os_file.h"
#include "os/include/os_error.h"
#include "os/include/os_stdio.h"
#include "os/include/strlcpy.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>  /* for mkdir() */
#include <unistd.h>    /* for stat() */
#include <dirent.h>
#include <errno.h>

/** Directory handle */
struct os_dir
{
    const char *path;  /**< Path of directory */
    DIR *cdir;         /**< Linux directory */
    char *cur_entry;   /**< Entry last returned to caller */
};

int os_dir_create(const char *path)
{
    struct stat st;

    if (path == NULL || path[0] == '\0')
        return -EINVAL;

    if (stat(path, &st) == 0)
    {
        /* Succeed if the directory already exists: all we want is
         * to have the given directory to exist when returning */
        if (S_ISDIR(st.st_mode))
            return 0;

        /* The path already exists but is not a dir */
        return -ENOTDIR;
    }

    /* XXX This is the access mode hardcoded in os_file:create_dir()
     * but should be passed as a parameter */
    if (mkdir(path, 0777) != 0)
        return -errno;

    return 0;
}

static int __os_dir_create_rec(const char *path, const char *sep)
{
    char subdir[OS_PATH_MAX];
    const char *next_sep;
    int err;

    if (path == NULL)
        return -EINVAL;

    /* No separator */
    if (sep == NULL)
	return os_dir_create(path);

    strlcpy(subdir, path, sep - path + 1);

    if (subdir[0] != '\0')
    {
	err = os_dir_create(subdir);
	if (err != 0)
	    return err;
    }

    next_sep = strchr(sep + 1, *OS_FILE_SEP);

    return __os_dir_create_rec(path, next_sep);
}

int os_dir_create_recursive(const char *path)
{
    if (path == NULL)
        return -EINVAL;

    return __os_dir_create_rec(path, strchr(path, *OS_FILE_SEP));
}

int os_dir_remove(const char *path)
{
    if (path == NULL)
        return -EINVAL;

    if (rmdir(path) != 0 && errno != ENOENT)
        return -errno;

    return 0;
}

int os_dir_remove_tree(const char *path)
{
    struct dirent **namelist;
    int i, nbelem;
    int retval = 0;
    char filename[PATH_MAX];
    struct stat info;

    if (path == NULL)
        return -EINVAL;

    nbelem = scandir(path, &namelist, NULL, NULL);

    if (nbelem < 0)
    {
        if (errno == ENOENT)
            return 0;
        else
            return -errno;
    }

    for (i = 0; i < nbelem; i++)
    {
        if (strcmp(namelist[i]->d_name, ".") == 0 ||
            strcmp(namelist[i]->d_name, "..") == 0)
            continue;

        retval = os_snprintf(filename, sizeof(filename),
			     "%s/%s", path, namelist[i]->d_name);
        if (retval >= (int)sizeof(filename))
        {
            retval = -E2BIG;
            goto error_dofreemem;
        } else
            retval = 0;

        /* Get info about the file */
        if (stat(filename, &info) <0)
        {
            retval = -errno;
            goto error_dofreemem;
        }

        /* Recurse if this is a dir */
        if (S_ISDIR(info.st_mode))
        {
            retval = os_dir_remove_tree(filename);
            if (retval != 0)
                goto error_dofreemem;
        }
        else
        {   /* not a directory */
            if (remove(filename) != 0)
            {
                retval = -errno;
                goto error_dofreemem;
            }
        }

    }

    /* The directory is now empty, we can remove it */
    if(remove(path) != 0)
    {
        if (errno == ENOENT)
            retval = 0;
        else
            retval = -errno;
    }

error_dofreemem:
    for (i = 0; i < nbelem; i++)
        free(namelist[i]);

    free(namelist);

    return retval;
}
