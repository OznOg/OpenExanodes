/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_conversion.h"
#include "common/include/exa_error.h"

#include <features.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include "md5.h"
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

static const char *self;
static bool err = false;

static void error(const char *fmt, ...)
{
    va_list al;

    fprintf(stderr, "error: ");

    va_start(al, fmt);
    vfprintf(stderr, fmt, al);
    va_end(al);

    fprintf(stderr, "\n");

    err = true;
}

int main(int argc, char **argv)
{
    int di;
    char *cp;
    int inFd = -1;
    int outFd = -1;
    int inCc = 0;
    int outCc;
    char *inFile;
    char *outFile;
    uint64_t blockSize, readSize;
    uint64_t size;
    uint64_t outTotal;
    char *buf = NULL;
    char hex_output[16 * 2 + 1];

    self = strrchr(argv[0], '/');
    if (self == NULL)
        self = argv[0];
    else
        self++;

    /* Parse any options */
    if (argc != 5)
    {
        fprintf(stderr, "dd with md5 checksum.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "usage: %s <if> <of> <bufsize> <count>\n", self);
        fprintf(stderr, "\n");
        fprintf(stderr, "    <if>       Input file\n");
        fprintf(stderr, "    <of>       Output file\n");
        fprintf(stderr, "    <bufsize>  Size of buffer\n");
        fprintf(stderr, "    <count>    Number of bytes to copy\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "NOTES:\n");
        fprintf(stderr, "  - The actual number of bytes copied may be greater"
                             " than the specified count\n"
                        "    because of the buffer size.\n");
        fprintf(stderr, "  - The computed md5 is appended to the end of the"
                             " output file (no idea why).\n");
        exit(1);
    }

    inFile  = argv[1];
    outFile = argv[2];

    if (to_uint64(argv[3], &blockSize) != EXA_SUCCESS)
    {
        error("invalid buffer size: '%s'", argv[3]);
        goto done;
    }

    if (to_uint64(argv[4], &size) != EXA_SUCCESS)
    {
        error("invalid byte count: '%s'", argv[4]);
        goto done;
    }

    if ((buf = malloc(blockSize)) == NULL)
    {
        error("alloc buffer: %s", exa_error_msg(-errno));
        goto done;
    }

    outTotal = 0;

    /* Open the source file*/
    inFd = open(inFile, 0);
    if (inFd < 0)
    {
        error("open input file %s: %s",inFile, exa_error_msg(-errno));
        goto done;
    }

    /* Open the dest file*/
    outFd = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (outFd < 0)
    {
        error("open output file %s: %s",outFile, exa_error_msg(-errno));
        goto done;
    }

    lseek(inFd, 0, SEEK_SET);
    lseek(outFd, 0, SEEK_SET);

    readSize = blockSize;

    /* Write the file with block  */
    while (outTotal < size)
    {

        /* Read the source file */
        inCc = read(inFd, buf, readSize);
        if (inCc <= 0)
        {
            error("read: %s", exa_error_msg(-errno));
            goto done;
        }

        cp = buf;
        outCc = 0;

        /* Write on the dest file */
        while (outCc < inCc)
        {
            md5_state_t state;
            md5_byte_t digest[16];

            outCc = write(outFd, cp, inCc);
            if (outCc <= 0)
            {
                error("write: %s", exa_error_msg(-errno));
                goto done;
            }

            inCc -= outCc;
            outTotal += outCc;

            /* Manage the check sum */
            md5_init(&state);
            md5_append(&state, (const md5_byte_t *)cp,outCc);
            md5_finish(&state, digest);
            for (di = 0; di < 16; ++di)
                sprintf(hex_output + di * 2, "%02x", digest[di]);

            cp += outCc;
        }

        /* End of the file */
        if (size - outTotal < blockSize)
            readSize = size - outTotal;
    }

    lseek(outFd, 0,SEEK_END);
    if (write(outFd, hex_output, strlen(hex_output)) <= 0)
    {
        error("write md5: %s", exa_error_msg(-errno));
        goto done;
    }

done:
    if (inFd != -1 && close(inFd) != 0)
        error("close %s: %s", inFile, exa_error_msg(-errno));

    if (outFd != -1 && close(outFd) != 0)
        error("close %s: %s", outFile, exa_error_msg(-errno));

    sync();
    free(buf);

    return err ? 1 : 0;
}
