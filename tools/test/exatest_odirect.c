/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include "common/include/exa_conversion.h"
#include "common/include/exa_error.h"

#define BLOCK_SIZE 4096

static const char *self;

static void error_exit(const char *fmt, ...)
{
    va_list al;

    fprintf(stderr, "error: ");

    va_start(al, fmt);
    vfprintf(stderr, fmt, al);
    va_end(al);

    fprintf(stderr, "\n");

    exit(1);
}

static void usage(void)
{
    fprintf(stderr, "Access a device in O_DIRECT mode.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s write|read <device> <block> <byte>\n", self);
    fprintf(stderr, "\n");
    fprintf(stderr, "    <device>  Device on which to perform the operation\n");
    fprintf(stderr, "    <block>   Block number to read or write (>= 0)\n");
    fprintf(stderr, "    <byte>    When writing: byte to write repeatedly\n");
    fprintf(stderr, "              When reading: expected value of all bytes\n");

    exit(1);
}

int main(int argc, char *argv[])
{
  bool do_read, do_write;
  char *dev;
  unsigned int block;
  char byte;
  char *buffer;
  int fd;
  int flags;
  int ret;

  self = strrchr(argv[0], '/');
  if (self == NULL)
      self = argv[0];
  else
      self++;

  if (argc != 5)
      usage();

  do_read  = (strcmp(argv[1], "read")  == 0);
  do_write = (strcmp(argv[1], "write") == 0);
  if (!do_read && !do_write)
      error_exit("invalid operation: '%s'", argv[1]);

  dev = argv[2];
  if (to_uint(argv[3], &block) != EXA_SUCCESS)
      error_exit("invalid block number: '%s'", argv[3]);

  if (argv[4][1] != '\0')
      error_exit("invalid byte: '%s'", argv[4]);
  byte = argv[4][0];

  ret = posix_memalign((void **)&buffer, BLOCK_SIZE, BLOCK_SIZE);
  if (ret != 0)
      error_exit("posix_memalign: %s", strerror(ret));

  flags = O_DIRECT | O_LARGEFILE;
  if (do_read)
      flags |= O_RDONLY;
  if (do_write)
      flags |= O_WRONLY;

  fd = open(dev, flags);
  if (fd == -1)
      error_exit("open: %s", exa_error_msg(-errno));

  ret = lseek(fd, BLOCK_SIZE * block, SEEK_SET);
  if (ret != BLOCK_SIZE * block)
      error_exit("lseek: %s", exa_error_msg(-errno));

  if (do_write)
  {
    int i;

    for (i = 0; i < BLOCK_SIZE; i++)
        buffer[i] = byte;

    ret = write(fd, buffer, BLOCK_SIZE);
    if (ret != BLOCK_SIZE)
        error_exit("write: %s", exa_error_msg(-errno));
  }

  if (do_read)
  {
    int i;

    ret = read(fd, buffer, BLOCK_SIZE);
    if (ret != BLOCK_SIZE)
        error_exit("read: %s", exa_error_msg(-errno));

    for (i = 0; i < BLOCK_SIZE; i++)
        if (buffer[i] != byte)
            error_exit("read: byte %d: expected '%c' (0x%02x), read '%c' (0x%02x)",
                       i, byte, byte, buffer[i], buffer[i]);
  }

  ret = close(fd);
  if (ret == -1)
      error_exit("close: %s", exa_error_msg(-errno));

  return 0;
}

