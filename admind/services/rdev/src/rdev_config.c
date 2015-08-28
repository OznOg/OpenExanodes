/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_constants.h"
#include "common/include/exa_env.h"

#include "os/include/os_assert.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_mem.h"
#include "os/include/os_string.h"

#include "admind/services/rdev/include/rdev_config.h"

#include <string.h>

#define RDEV_FILE "disks.conf"

typedef struct rdev_path {
    char path[EXA_MAXSIZE_DEVPATH + 1];
} rdev_path_t;

/**
 * Return a new allocated array of list of paths allowed to be used on the
 * node or NULL if file is not found or an error occured while reading.
 *
 * WARNING this function allocates memory, and the returned buffer
 * MUST be freed by caller with os_free.
 *
 * @param[in:out] err_desc   an error descriptor for output.
 *
 * @return a rdev_path_t array of NBMAX_DISKS_PER_NODE elements.
 */
static rdev_path_t *rdev_read_disk_conf_file(cl_error_desc_t *err_desc)
{
    /* file is opened read only because the code in here is not supposed to
     * modify it. The only way to modify the RDEV_FILE is via an administative
     * action (edit the file) */
    FILE *disk_list;
    rdev_path_t *disk_array = NULL, *it = NULL;
    char disk_conf_file[OS_PATH_MAX];
    char line[EXA_MAXSIZE_DEVPATH + 2]; /* likely ends '\n\0' */
    const char *conf_dir = NULL;

    OS_ASSERT(err_desc != NULL);

    /* note that conf_dir is afforded to end with FILE_SEP */
    conf_dir = exa_env_nodeconfdir();
    if (conf_dir == NULL)
    {
	set_error(err_desc, -EXA_ERR_BAD_INSTALL,
		  "Environment key 'EXANODES_NODE_CONF_DIR' not set.");
	return NULL;
    }

    if (os_snprintf(disk_conf_file, sizeof(disk_conf_file), "%s" RDEV_FILE, conf_dir)
        >= sizeof(disk_conf_file))
    {
	set_error(err_desc, -EXA_ERR_BAD_INSTALL,
		  "Environment key 'EXANODES_NODE_CONF_DIR' too long.");
	return NULL;
    }

    disk_list = fopen(disk_conf_file, "r");
    if (disk_list == NULL)
    {
	/* A non-existent file is not an error */
	if (errno == ENOENT)
	    set_success(err_desc);
        else
            set_error(err_desc, -errno, "File " RDEV_FILE " cannot be read: %s (%d)",
                      os_strerror(errno), -errno);
        return NULL;
    }

    /* prepare array */
    disk_array = os_malloc(sizeof(rdev_path_t) * NBMAX_DISKS_PER_NODE);

    for (it = disk_array; it < disk_array + NBMAX_DISKS_PER_NODE; it++)
	it->path[0] = '\0';

    for (it = disk_array; it < disk_array + NBMAX_DISKS_PER_NODE; it++)
    {
	char *ptr = fgets(line, sizeof(line), disk_list);
        size_t len;

	if (ptr == NULL)
	    break;

	/* remove final '\n'*/
        len = strlen(line);
	if (line[len - 1] == '\n')
	    line[len - 1] = '\0';

        os_str_trim_right(line);

	/* FIXME no check if disk name is truncated here */
	os_strlcpy(it->path, line, EXA_MAXSIZE_DEVPATH + 1);
    }

    /* too many lines in the file -> error */
    if (it == disk_array + NBMAX_DISKS_PER_NODE
	    && fgets(line, sizeof(line), disk_list) != NULL)
    {
	os_free(disk_array);
	fclose(disk_list);

	set_error(err_desc, -EINVAL, "Too many entries in " RDEV_FILE ".");
	return NULL;
    }

    fclose(disk_list);

    set_success(err_desc);

    return disk_array;
}

char *rdev_get_path_list(cl_error_desc_t *err_desc)
{
#define PATH_LIST_SIZE (NBMAX_DISKS_PER_NODE * (EXA_MAXSIZE_DEVPATH + 1))
    char *path_list;
    rdev_path_t *disk_array = NULL, *it = NULL;
    char *pos = NULL;
    size_t len;
    int n = 0;

    disk_array = rdev_read_disk_conf_file(err_desc);

    if (disk_array == NULL)
    {
#define ANY_DISK_STR "any"
	if (err_desc->code == EXA_SUCCESS)
	    return os_strdup(ANY_DISK_STR);
	else
	    return NULL;
    }

    pos = path_list = os_malloc(PATH_LIST_SIZE);

    if (path_list == NULL)
    {
	set_error(err_desc, -ENOMEM, "Cannot allocate buffer for devices list.");
	return NULL;
    }

    /* Make sure path_list is already a valid string. */
    path_list[0] = '\0';

    for (it = disk_array; it < disk_array + NBMAX_DISKS_PER_NODE; it++)
    {
	/* empty disk path -> skip... */
	if (it->path[0] == '\0')
	    continue;

	n += os_snprintf(pos, PATH_LIST_SIZE - n - sizeof(char) /* for \0 */,
		         "%s ", it->path);

	pos = path_list + n;
    }

    /* remove trailing space */
    len = strlen(path_list);
    if (path_list[len - 1] == ' ')
	path_list[len - 1] = '\0';

    os_free(disk_array);
    return path_list;
}


bool rdev_is_path_available(const char *path, cl_error_desc_t *err_desc)
{
    rdev_path_t *disk_array = NULL, *it = NULL;

    disk_array = rdev_read_disk_conf_file(err_desc);

    /* NULL means there is no config file (->"any") or an error */
    if (disk_array == NULL)
	return err_desc->code == EXA_SUCCESS;

    /* compare the path with the array of available pathes */
    for (it = disk_array; it < disk_array + NBMAX_DISKS_PER_NODE; it++)
    {
	if (strcmp(path, it->path) == 0)
	{
	    os_free(disk_array);
	    return true;
	}
    }

    os_free(disk_array);
    return false;
}
