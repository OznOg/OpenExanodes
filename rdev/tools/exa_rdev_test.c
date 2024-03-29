/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/* TODO List in usage help the statuses available for use with --set-status */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* So that we get under the hood access to exa_rdev */
#define RDEV_TEST

#include "rdev/include/exa_rdev.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_error.h"
#include "common/include/exa_math.h"

#include "os/include/os_disk.h"
#include "os/include/os_file.h"
#include "os/include/os_getopt.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"
#include "os/include/os_time.h"
#include "os/include/os_error.h"
#include "os/include/os_assert.h"
#ifdef WIN32
#include "os/include/os_windows.h"
#endif

#define PAGE_SIZE 4096

/** Name of this program */
static const char *program = NULL;

/* This feature isn't supported on Linux yet */
#ifdef WIN32
static int rdev_status_from_str(rdev_req_status_t *status, const char *str)
{
    rdev_req_status_t s;

    for (s = RDEV_REQ_STATUS__FIRST; s <= RDEV_REQ_STATUS__LAST; s++)
        if (strcmp(str, exa_rdev_status_name(s)) == 0)
        {
            *status = s;
            return 0;
        }

    return -EINVAL;
}

static const char *status_info_str(rdev_req_status_t status)
{
    return status < 0 ? exa_error_msg(status) : exa_rdev_status_name(status);
}

static int list_statuses(void)
{
    int i = 0;
    char path[EXA_MAXSIZE_DEVPATH + 1];
    rdev_req_status_t status;
    int use_count;

    /* XXX Should use format %-*s with EXA_MAXSIZE_DEVPATH but we never (?)
     * have paths that long and keeping the width small makes the output
     * much more pleasing to the eye... */
    printf("Entries with use count > 0:\n");
    printf("%-4s  %-50s  %-3s  %s\n", "Dev#", "Path", "Use", "Status");
    while (exa_rdev_at_index(i, path, sizeof(path), &status, &use_count) != -ENOENT)
    {
        if (use_count > 0)
            printf("%4d  %-50s  %3d  %s (%d)\n", i, path, use_count,
                   status_info_str(status), status);
        i++;
    }

    return 0;
}

static int do_status(const char *path, char op, const char *status_str)
{
    rdev_req_status_t status;
    int use_count = -1;
    int err = 0;

    switch (op)
    {
    case 'g':
        /* Get the disk status */
        err = exa_rdev_under_the_hood_do(path, 'g', &status, &use_count);
        if (err != 0)
        {
            fprintf(stderr, "failed getting status of %s: error %d\n", path, err);
            return err;
        }
        printf("use_count = %d\n", use_count);
        printf("status    = %s (%d)\n", status_info_str(status), status);
        break;

    case 's':
        /* Set the disk status */
        err = rdev_status_from_str(&status, status_str);
        if (err != 0)
        {
            fprintf(stderr, "invalid status: %s\n", status_str);
            return err;
        }
        err = exa_rdev_under_the_hood_do(path, 's', &status, &use_count);
        if (err != 0)
        {
            fprintf(stderr, "failed setting status of %s: error %d\n", path, err);
            return err;
        }
        break;

    default:
        OS_ASSERT_VERBOSE(false, "invalid op: '%c'", op);
    }

    return err;
}

static int deactivate_device(char *path)
{
    int ret;

    ret = exa_rdev_deactivate(NULL, path);
    if (ret != 0)
	fprintf(stderr, "Failed to deactivate device %s: error %d\n",
		path, ret);

    fprintf(stderr, "Deactivated device %s\n", path);

    return ret;
}

#else  /* WIN32 */

static int list_statuses(void)
{
    fprintf(stderr, "not supported on Linux yet\n");
    return -EINVAL;
}

static int do_status(const char *path, char op, const char *status_str)
{
    fprintf(stderr, "not supported on Linux yet\n");
    return -EINVAL;
}

static int deactivate_device(char *path)
{
    exa_rdev_handle_t *dev_req;
    int ret;

    dev_req = exa_rdev_handle_alloc(path);
    if (dev_req == NULL)
    {
	fprintf(stderr, "exa_rdev_request_init() failed, disk path = %s\n",
		path);
	return -1;
    }

    ret = exa_rdev_deactivate(dev_req, NULL);
    if (ret != 0)
	fprintf(stderr, "Failed to deactivate device %s: error %d\n",
		path, ret);

    exa_rdev_handle_free(dev_req);

    fprintf(stderr, "Deactivated device %s\n", path);

    return ret;
}

#endif  /* WIN32 */


static int async_op(rdev_op_t op, uint64_t offset, uint64_t size_in_bytes,
                     exa_rdev_handle_t *dev_req)
{
    struct IO {
        void *buffer;
        size_t size;
    };
    int retval;

    if (size_in_bytes % SECTOR_SIZE)
        return -EINVAL;

    retval = RDEV_REQUEST_NONE_ENDED;
    while (retval != RDEV_REQUEST_ALL_ENDED) {

        if (retval == RDEV_REQUEST_NONE_ENDED && size_in_bytes == 0)
            retval = RDEV_REQUEST_NOT_ENOUGH_FREE_REQ;

        switch (retval)
        {
            case RDEV_REQUEST_ALL_ENDED:
            case RDEV_REQUEST_NONE_ENDED:
                {
                    uint64_t chunk_size = MIN(size_in_bytes, EXA_RDEV_READ_WRITE_FRAGMENT);
                    /* Prepare buffer to do requests, exa_rdev requires it to be aligned */
                    struct IO *io = malloc(sizeof(*io));
                    io->buffer = os_aligned_malloc(EXA_RDEV_READ_WRITE_FRAGMENT, PAGE_SIZE, NULL);
                    io->size = chunk_size;

                    if (op == RDEV_OP_WRITE) {
                        /* about to write data on rdev, thus first get the data to be
                         * written from the standard input */
                        ssize_t bytes_read = 0;
                        do {
                            retval = read(0, io->buffer + bytes_read, chunk_size - bytes_read);
                            if (retval == 0) {
                                memset(io->buffer + bytes_read, 0, chunk_size - bytes_read);
                                retval = chunk_size - bytes_read;
                            }
                            if (retval > 0)
                                bytes_read += retval;

                        } while ((retval == -1 && errno == EINTR) || bytes_read < chunk_size);
                    }

                    /* Now data buffer is full, write data on rdev */

                    /* Be carefull the 'nbd_private' pointer can be modified */
                    retval = exa_rdev_make_request_new(op, (void **)&io, offset / SECTOR_SIZE,
                            chunk_size / SECTOR_SIZE, io->buffer, dev_req);
                    if (retval == RDEV_REQUEST_NOT_ENOUGH_FREE_REQ) {
                        os_free(io->buffer);
                        os_free(io);
                        break; /* retry */
                    }

                    if (retval < 0)
                    {
                        os_free(io->buffer);
                        os_free(io);
                        fprintf(stderr, "Error %d\n", retval);
                        retval = RDEV_REQUEST_END_ERROR;
                        break;
                    }

                    if (retval == RDEV_REQUEST_END_OK) {
                        if (op == RDEV_OP_READ) {
                            /* about to write data on rdev, thus first get the data to be
                             * written from the standard input */
                            ssize_t bytes_written = 0;
                            int ret;
                            do {
                                ret = write(1, io->buffer + bytes_written, io->size - bytes_written);
                                if (ret >= 0)
                                    bytes_written += ret;

                            } while ((ret == -1 && errno == EINTR) || bytes_written < chunk_size);
                        }
                        os_free(io->buffer);
                        os_free(io);
                    }

                    size_in_bytes -= chunk_size;
                    offset += chunk_size;
                }
                break;

            case RDEV_REQUEST_NOT_ENOUGH_FREE_REQ:
                {
                    struct IO *io = NULL;
                    retval = exa_rdev_wait_one_request((void **)&io, dev_req);
                    if (retval == RDEV_REQUEST_ALL_ENDED)
                        break;
                    if (op == RDEV_OP_READ && retval == RDEV_REQUEST_END_OK) {
                        /* about to write data on rdev, thus first get the data to be
                         * written from the standard input */
                        ssize_t bytes_written = 0;
                        do {
                            retval = write(1, io->buffer, io->size);
                            if (retval >= 0)
                                bytes_written += retval;

                        } while ((retval == -1 && errno == EINTR) || bytes_written < io->size);
                    }
                    if (io != NULL)
                        os_free(io->buffer);
                    os_free(io);
                    retval = RDEV_REQUEST_END_OK;
                    break;
                }

            case RDEV_REQUEST_END_OK:
            case RDEV_REQUEST_END_ERROR:
                if (retval == RDEV_REQUEST_END_ERROR)
                    fprintf(stderr, "Request ended with error\n");
                retval = RDEV_REQUEST_NONE_ENDED;
                break;
        }
    }
    return 0;
}

static void usage(void)
{
#ifdef WIN32
    rdev_req_status_t status;
#endif

    fprintf(stderr, "Tool for reading and writing EXA_RDEV\n");
    fprintf(stderr, "Size MUST be aligned on %d\n", SECTOR_SIZE);
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s [options]\n", program);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -b, --break                Set the status of the device to broken\n");
    fprintf(stderr, "  -d, --disk <path>          Use the device specified by the path\n");
    fprintf(stderr, "  -G, --get-status           Get the status of the device\n");
    fprintf(stderr, "  -h, --help                 Print this usage help\n");
    fprintf(stderr, "  -L, --list-statuses        List device statuses\n");
    fprintf(stderr, "  -o  --offset               Offset from which to start read/write\n");
    /* fprintf(stderr, "  -r, --random               Make random access to disk\n"); */
    fprintf(stderr, "  -s, --size <size>          Size of data to read/write\n");
    fprintf(stderr, "  -S, --set-status <status>  Set the status of the device\n");
#ifdef WIN32
    fprintf(stderr, "                             <status> ::= one of");
    for (status = RDEV_REQ_STATUS__FIRST; status <= RDEV_REQ_STATUS__LAST; status++)
        fprintf(stderr, " %s", exa_rdev_status_name(status));
    fprintf(stderr, "\n");
#endif
    fprintf(stderr, "  -w  --write                Write on disk (Read is the default)\n");
    fprintf(stderr, "\n");

    exit(1);
}

static int do_io(const char *disk_path, bool write, uint64_t offset,
                  uint64_t size_in_bytes)
{
    int disk_fd;
    exa_rdev_handle_t *dev_req;
    uint64_t disk_size;
    int ret;

    /* Get disk size */
    disk_fd = os_disk_open_raw(disk_path, OS_DISK_READ);
    if (disk_fd < 0)
    {
	fprintf(stderr, "Can not open disk %s\n", disk_path);
	return -1;
    }

    ret = os_disk_get_size(disk_fd, &disk_size);
    if (ret < 0)
    {
	fprintf(stderr, "Can not get the size of disk %s\n", disk_path);
	return -1;
    }

    fprintf(stderr, "Disk %s has a size of %"PRIu64" bytes\n",
	    disk_path, disk_size);

    dev_req = exa_rdev_handle_alloc(disk_path);
    if (dev_req == NULL)
    {
	fprintf(stderr, "exa_rdev_request_init() failed, disk_path = %s\n",
		disk_path);
	return -1;
    }

    /* Carry on requests to exa_rdev */
    ret = async_op(write ? RDEV_OP_WRITE : RDEV_OP_READ, offset, size_in_bytes, dev_req);

    fprintf(stderr, "Finished %s %"PRIu64 " bytes on disk %s at offset %"PRIu64 "; result (%d)\n",
	    write ? "writing" : "reading", size_in_bytes, disk_path, offset, ret);

    return ret;
}

int main(int argc, char *argv[])
{
    struct option long_opts[] = {
        { "break",         no_argument,       NULL, 'b' },
        { "disk",          required_argument, NULL, 'd' },
        { "get-status",    no_argument,       NULL, 'G' },
        { "help",          no_argument,       NULL, 'h' },
        { "list-statuses", no_argument,       NULL, 'L' },
        { "offset",        required_argument, NULL, 'o' },
/*      { "random",        no_argument,       NULL, 'r' }, */
        { "size",          required_argument, NULL, 's' },
        { "set-status",    required_argument, NULL, 'S' },
        { "write",         no_argument,       NULL, 'w' },
        { NULL,            0,                 NULL, '\0' }
    };
    int long_idx;
    int opt;
    enum {
        OP_IO,
        OP_LIST_STATUSES,
        OP_GET_STATUS,
        OP_SET_STATUS,
        OP_DEACTIVATE_DEVICE
    } op = OP_IO;
    char *disk_path = NULL;
/*     bool random = false; */
    bool write = false;
    uint64_t size_in_bytes = 0;
    uint64_t offset = 0;
    char status_str[256] = "";
    int err;

#ifdef WIN32
    os_windows_disable_crash_popup();
#endif

    program = argv[0];

    if (argc == 1)
    {
	fprintf(stderr, "%s: no option given\nType %s --help for usage help\n",
                program, program);
	return -1;
    }

    while (true)
    {
	opt = os_getopt_long(argc, argv, "d:bGhLo:r:s:wS:", long_opts, &long_idx);
        /* Missing argument, quit right away (os_getopt_long() takes care of
         * printing an error message) */
        if (opt == ':' || opt == '?')
            return 1;

        if (opt == -1)
            break;

	switch (opt)
	{
	case 'b':
	    op = OP_DEACTIVATE_DEVICE;
	    break;

	case 'd':
	    disk_path = optarg;
	    break;

        case 'G':
            op = OP_GET_STATUS;
            break;

	case 'h':
	    usage();
	    break;

        case 'L':
            op = OP_LIST_STATUSES;
            break;

	case 'o':
            if (to_uint64(optarg, &offset) != EXA_SUCCESS)
            {
                fprintf(stderr, "invalid offset\n");
                exit(1);
            }
	    break;

/* 	case 'r': */
/* 	    random = true; */
/* 	    break; */

	case 's':
            if (to_uint64(optarg, &size_in_bytes) != EXA_SUCCESS)
            {
                fprintf(stderr, "invalid size\n");
                exit(1);
            }
	    break;

	case 'w':
	    write = true;
	    break;

        case 'S':
            op = OP_SET_STATUS;
            strlcpy(status_str, optarg, sizeof(status_str));
            break;
	}
    }

    if (optind < argc)
    {
        fprintf(stderr, "%s: too many arguments\nType %s --help for usage help\n",
                program, program);
        return 1;
    }

    err = exa_rdev_static_init(RDEV_STATIC_GET);
    if (err != 0)
    {
        fprintf(stderr, "failed initializating RDEV statics: error %d\n", err);
        fprintf(stderr, "\tExanodes is probably not running on this node.\n");
        return 1;
    }

    if (disk_path == NULL && op != OP_LIST_STATUSES)
    {
        fprintf(stderr, "no disk path specified\n");
        err = -1;
        goto done;
    }

    if (size_in_bytes % SECTOR_SIZE) {
        return -EINVAL;
    }

    switch (op)
    {
    case OP_IO:
        err = do_io(disk_path, write, offset, size_in_bytes);
        break;

    case OP_LIST_STATUSES:
        err = list_statuses();
        break;

    case OP_GET_STATUS:
        err = do_status(disk_path, 'g', NULL);
        break;

    case OP_SET_STATUS:
        err = do_status(disk_path, 's', status_str);
        break;

    case OP_DEACTIVATE_DEVICE:
	err = deactivate_device(disk_path);
	break;
    }

done:
    exa_rdev_static_clean(RDEV_STATIC_RELEASE);
    return err ? 1 : 0;
}
