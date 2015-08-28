/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "admind/src/deviceblocks.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include "os/include/os_inttypes.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_volume.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "log/include/log.h"

/**
 * Check if 'devpath' exists and is a block device.
 *
 * @param[in] devpath Path of the device. Eg: /dev/exa/group/volume
 *
 * @return EXA_SUCCESS if the device 'devpath' is a block device, a
 *         negative error code otherwise
 */
static int admind_check_device(const char *devpath)
{
    struct stat info;

    if (stat(devpath, &info) < 0)
        return -errno;

    if (!S_ISBLK(info.st_mode))
        return -EXA_ERR_NOT_BLOCK_DEV;

    return EXA_SUCCESS;
}

int admind_voldeviceblock_set_readahead(struct adm_volume *volume,
                                        uint32_t readahead)
{
    char path[EXA_MAXSIZE_LINE + 1];
    unsigned long readahead_sectors;
    int fd;
    int ret;

    if (readahead > UINT32_MAX / 2)
        return -ERANGE;

    readahead_sectors = readahead * 2;

    adm_volume_path(path, sizeof(path), volume->group->name,
                    volume->name);

    ret = admind_check_device(path);
    if (ret != EXA_SUCCESS)
        return ret;

    if ((fd = open(path, O_RDONLY)) == -1)
        return -errno;

    if (ioctl(fd, BLKRASET, readahead_sectors) == -1)
    {
        close(fd);
        return -errno;
    }

    close(fd);

    return EXA_SUCCESS;
}
