/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_getopt.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_disk.h"
#include "os/include/os_file.h"
#include "os/include/strlcpy.h"
#include "os/include/os_random.h"
#include "os/include/os_error.h"
#include "os/include/os_assert.h"
#include "os/include/os_stdio.h"

#include "admind/services/rdev/src/rdev_sb.h"

#include "common/include/exa_constants.h"
#include "common/include/uuid.h"

#ifdef WIN32
#include "os/include/os_windows.h"
#include <winioctl.h>
#include <io.h>  /* for read() */
#else
#include <unistd.h>  /* for read() */
#endif

#include <string.h>
#include <ctype.h>

/* FIXME Use Exanodes' "disk_patterns" tunable? */
#ifdef WIN32
#define VOLUME_PATTERN "\\\\?\\Volume{*"
#else
#define VOLUME_PATTERN "/dev/sd*"
#endif

/** Name of this program */
static const char *program = NULL;

/**
 * Helper function: open the specified volume, unless it isn't capable of
 * storing a rdev superblock.
 *
 * @param[in] volume  Volume to open
 * @param[in] mode    Access mode
 *
 * @return File descriptor if successful, a negative error code otherwise
 */
static int __open_volume(const char *volume, int mode)
{
    int fd = -1;
    uint64_t size;
    int err = 1;

    /* We need the read access to get the size. */
    OS_ASSERT(mode & OS_DISK_READ);

    fd = os_disk_open_raw(volume, mode | OS_DISK_EXCL);
    if (fd < 0)
    {
        err = fd;
        fprintf(stderr, "failed opening disk %s: %s (%d)\n", volume,
                strerror(-err), err);
        goto done;
    }

    err = os_disk_get_size(fd, &size);
    if (err)
    {
        fprintf(stderr, "failed getting size of disk %s: %s (%d)", volume,
                strerror(-err), err);
        goto done;
    }

    if (size < RDEV_RESERVED_AREA_IN_SECTORS * SECTOR_SIZE)
    {
        err = -EINVAL;
        fprintf(stderr, "disk %s too small: %"PRIu64" bytes, and min is %u bytes\n",
                volume, size, RDEV_RESERVED_AREA_IN_SECTORS * SECTOR_SIZE);
        goto done;
    }

    return fd;

done:
    if (fd >= 0)
        close(fd);

    return err;
}

/**
 * Helper function: write a superblock to an opened volume.
 *
 * @param[in] fd  File descriptor to the volume
 * @param[in] sb  Superblock
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int __write_superblock(int fd, const rdev_superblock_t *sb)
{
    ssize_t bytes_written;

    bytes_written = write(fd, sb, RDEV_SUPERBLOCK_SIZE);
    if (bytes_written != RDEV_SUPERBLOCK_SIZE)
    {
        fprintf(stderr, "failed writing RDEV superblock: %s (%d)\n",
                strerror(errno), errno);
        return -errno;
    }

    return 0;
}

/**
 * Helper function: read the superblock from an opened volume.
 *
 * @param[in]  fd  File descriptor to the volume
 * @param[out] sb  Superblock read
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int __read_superblock(int fd, rdev_superblock_t *sb)
{
    ssize_t bytes_read;

    bytes_read = read(fd, sb, RDEV_SUPERBLOCK_SIZE);
    if (bytes_read != RDEV_SUPERBLOCK_SIZE)
    {
        fprintf(stderr, "failed reading RDEV superblock: %s (%d)\n",
                strerror(errno), errno);
        return -errno;
    }

    return 0;
}

/**
 * Read the superblock of a volume.
 *
 * @param[in] volume  Volume to read the superblock from
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int read_rdev_superblock(const char *volume)
{
    int fd = -1;
    char buffer[RDEV_SUPERBLOCK_SIZE];
    rdev_superblock_t *sb = (rdev_superblock_t *)buffer;
    int err = 0;

    fd = __open_volume(volume, OS_DISK_READ);
    if (fd < 0)
        return fd;
    err = __read_superblock(fd, sb);
    close(fd);

    if (err)
        return err;

    /* Force string termination, in case the superblock is corrupted
     * or just not an actual superblock */
    sb->magic[sizeof(sb->magic) - 1] = '\0';

    printf("Superblock of %s:\n", volume);
    printf("    Magic: '%s'\n", sb->magic);
    printf("     UUID: "UUID_FMT"\n", UUID_VAL(&sb->uuid));

    return 0;
}

/* FIXME More or less a duplicate of rdev.c:rdev_initialize_sb() */
/**
 * Write a superblock on a volume.
 *
 * @param[in] volume  Volume to write the superblock on
 * @param[in] uuid    UUID to use (may be NULL)
 *
 * If the specified UUID is NULL, a random one will be generated.
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int write_rdev_superblock(const char *volume, const exa_uuid_t *uuid)
{
    int fd = -1;
    char buffer[RDEV_SUPERBLOCK_SIZE];
    rdev_superblock_t *sb = (rdev_superblock_t *)buffer;
    int err = 0;

    memset(buffer, 0, sizeof(buffer));
    strlcpy(sb->magic, EXA_RDEV_SB_MAGIC, sizeof(sb->magic));
    if (uuid == NULL)
    {
        os_random_init();
        uuid_generate(&sb->uuid);
        printf("Generated random UUID: "UUID_FMT"\n", UUID_VAL(&sb->uuid));
    }
    else
        uuid_copy(&sb->uuid, uuid);

    fd = __open_volume(volume, OS_DISK_RDWR);
    if (fd < 0)
        return fd;
    err = __write_superblock(fd, sb);
    close(fd);

    return err;
}

/**
 * Delete the superblock of a volume.
 * The superblock is 'deleted' by being overwritten with zeroes.
 *
 * @param[in] volume  Volume whose superblock is to be deleted
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int delete_rdev_superblock(const char *volume)
{
    char buffer[RDEV_SUPERBLOCK_SIZE];
    int fd;

    fd = __open_volume(volume, OS_DISK_RDWR);
    if (fd < 0)
        return fd;

    memset(buffer, 0, sizeof(buffer));
    return __write_superblock(fd, (rdev_superblock_t *)buffer);
}

/**
 * Check whether a volume belongs to Exanodes (ie, is tagged with Exanodes'
 * magic).
 *
 * @param[in] volume  Volume to check
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int __get_volume_info(const char *volume, rdev_superblock_t *sb,
                             size_t sb_size)
{
    int fd;
    int err = 0;

    OS_ASSERT(sb_size >= RDEV_SUPERBLOCK_SIZE);

    fd = __open_volume(volume, OS_DISK_READ);
    if (fd < 0)
        return -errno;
    err = __read_superblock(fd, sb);
    close(fd);

    return err;
}

/**
 * List all volumes.
 *
 * @return 0 if successful, a negative error code otherwise
 */
static int list_volumes(void)
{
#define MARK     "EXA"
#define NO_MARK  "   "
    os_disk_iterator_t *iter;
    const char *disk;

    iter = os_disk_iterator_begin(VOLUME_PATTERN);
    if (iter == NULL)
    {
        fprintf(stderr, "failed creating disk iterator\n");
        return -ENOMEM;
    }

    printf("Disks matching '%s' (Exanodes disks are marked with '"MARK"'):\n",
           VOLUME_PATTERN);
    while ((disk = os_disk_iterator_get(iter)) != NULL)
    {
        char buf[RDEV_SUPERBLOCK_SIZE];
        rdev_superblock_t *sb = (rdev_superblock_t *)buf;
        bool exa;

        if (__get_volume_info(disk, sb, RDEV_SUPERBLOCK_SIZE) != 0)
        {
            fprintf(stderr, "failed reading sb of %s\n", disk);
            continue;
        }

        exa = strcmp(sb->magic, EXA_RDEV_SB_MAGIC) == 0;
        printf("    %s  %s", exa ? MARK : NO_MARK, disk);
        if (exa)
            printf("  "UUID_FMT, UUID_VAL(&sb->uuid));
        printf("\n");
    }

    os_disk_iterator_end(iter);

    return 0;
}

/**
 * Print usage help and exit.
 *
 * The help is printed on stdout if the exit status specified is zero and on
 * stderr otherwise.
 *
 * @param[in] exit_status  Status to exit with
 */
static void usage(int exit_status)
{
    FILE *out = exit_status == 0 ? stdout : stderr;

    fprintf(out, "Tool for reading and writing RDEV superblocks\n");
    fprintf(out, "\n");
    fprintf(out, "Usage: %s [options]\n", program);
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  -d, --delete <disk>  Delete the RDEV superblock on the specified disk.\n");
    fprintf(out, "  -h, --help           Print this usage help.\n");
    fprintf(out, "  -l, --list           List all disks.\n");
    fprintf(out, "  -r, --read <disk>    Read the RDEV superblock of the specified disk.\n");
    fprintf(out, "  -u, --uuid <uuid>    RDEV UUID to use.\n");
    fprintf(out, "  -w, --write <disk>   Write a RDEV superblock on the specified disk.\n");
    fprintf(out, "                       If no UUID is specified with -u, a randomly generated\n");
    fprintf(out, "                       UUID will be used.\n");
    fprintf(out, "\n");
    fprintf(out, "The following disk names are accepted (they may be terminated by a single\n");
    fprintf(out, "backslash '\\'):\n");
#ifdef WIN32
    fprintf(out, "  X:\n");
    fprintf(out, "  \\\\?\\X:\n");
    fprintf(out, "  \\\\?\\Volume{...}\n");
    fprintf(out, "where X is a drive letter.\n");
#else
    fprintf(out, "  /dev/sd*\n");
#endif
    fprintf(out, "\n");
    fprintf(out, "An RDEV UUID is of the form: xxxxxxxx:xxxxxxxx:xxxxxxxx:xxxxxxxx\n");
    fprintf(out, "(four segments of eight hexadecimal digits).\n");

    exit(exit_status);
}

int main(int argc, char *argv[])
{
    static struct option long_opts[] = {
        { "delete", required_argument, NULL, 'd' },
        { "help",   no_argument,       NULL, 'h' },
        { "list",   no_argument,       NULL, 'l' },
        { "read",   required_argument, NULL, 'r' },
        { "uuid",   required_argument, NULL, 'u' },
        { "write",  required_argument, NULL, 'w' },
        { NULL,     0,                 NULL, '\0' }
    };
    int long_idx;
    enum { OP_NONE, OP_LIST_VOLUMES, OP_READ_SB,
           OP_WRITE_SB, OP_DELETE_SB } op = OP_NONE;
    char *disk = NULL;
    char vol[OS_PATH_MAX];
    const char *uuid_str = NULL;
    exa_uuid_t uuid_data;
    exa_uuid_t *uuid = NULL;
    int err;

    program = argv[0];

    if (argc == 1)
    {
        fprintf(stderr, "%s: no option given\nType %s --help for usage help\n",
                program, program);
        exit(1);
    }

    while (true)
    {
        int c = os_getopt_long(argc, argv, "d:hlr:u:w:", long_opts, &long_idx);
        if (c == -1)
            break;

        switch (c)
        {
        case 'd':
            op = OP_DELETE_SB;
            disk = optarg;
            break;

        case 'h':
            usage(0);
            break;

        case 'l':
            op = OP_LIST_VOLUMES;
            break;

        case 'r':
            op = OP_READ_SB;
            disk = optarg;
            break;

        case 'u':
            uuid_str = optarg;
            break;

        case 'w':
            op = OP_WRITE_SB;
            disk = optarg;
            break;

        default:
            usage(1);
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "%s: too many arguments\nType %s --help for usage help\n",
                program, program);
        exit(1);
    }

    if (uuid_str != NULL)
    {
        if (uuid_scan(uuid_str, &uuid_data) < 0)
        {
            fprintf(stderr, "invalid uuid: %s\n", uuid_str);
            exit(1);
        }
        uuid = &uuid_data;
    }

    if (disk != NULL)
    {
        size_t len = strlen(disk);

        if (disk[len - 1] == '\\')
            disk[len - 1] = '\0';
        if (os_disk_normalize_path(disk, vol, sizeof(vol)) != 0
            || !os_disk_path_is_valid(vol))
        {
            fprintf(stderr, "invalid disk name: %s\n", vol);
            exit(1);
        }
    }

    switch (op)
    {
    case OP_LIST_VOLUMES:
        err = list_volumes();
        break;

    case OP_READ_SB:
        err = read_rdev_superblock(vol);
        break;

    case OP_WRITE_SB:
        err = write_rdev_superblock(vol, uuid);
        break;

    case OP_DELETE_SB:
        err = delete_rdev_superblock(vol);
        break;

    default:
        OS_ASSERT_VERBOSE(false, "invalid op: %d", op);
    }

    return err ? 1 : 0;
}
