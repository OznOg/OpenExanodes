/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** @file os_memtrace.c
 * @brief User-space memory allocation routines.
 *
 * Memory allocation statistics can be switched on or off with the
 * WITH_MEMTRACE define.
 */

#include "os/include/os_mem.h"

#include <stdlib.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#include "os/include/os_assert.h"
#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_syslog.h"
#include "os/include/os_thread.h"
#include "os/include/os_stdio.h"


/* Definition of memory blocks. */
struct os_memblock
{
    struct os_memblock *prev;   /**< Previous allocated block. */
    struct os_memblock *next;   /**< Next allocated block. */

    char file[32];              /**< File name in which block was
                                 * allocated. */
    unsigned int line;          /**< Line number at which block was
                                 * allocated. */
    unsigned int magic;         /**< Magic number for integrity
                                 * checks. Value: OS_MEMTRACE_MAGIC. */
#define OS_MEMTRACE_MAGIC         (0xdeadbeaf)
#define OS_MEMTRACE_TAIL_BUF_SIZE (256*4) /* size of the trailing buffer here 256 magic */
#define OS_MEMTRACE_DEL_PATTERN   (0xEE)  /* Patern to erase memory */
    size_t size;                /**< User data block size. */
    char ptr[4];                /**< Start of user data. This has to be
                                 * the last member of
                                 * ::os_memblock. Actual size of this
                                 * array is os_memblock::size. */
};


/** Offset of user data inside memory @link ::os_memblock block
 * @endlink. */
#define MEMBLOCK_OFFSET \
    ((char *)(&(((struct os_memblock *)NULL)->ptr)) - ((char *)NULL))

/** Internal list of allocated memory blocks. */
static struct os_memblock *blocks = NULL;

/** Mutex for protecting access to #blocks. */
static os_thread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


/** Initialize a memory block and return a pointer on user data.
 * @param[in] b: pointer on memory block to be initialized.
 * @param[in] size: number of bytes to be allocated.
 * @param[in] file: File name in which block is allocated.
 * @param[in] line: Line number at which block is allocated.
 * @return a pointer to the allocated memory.
 * @sa os_release_memblock.
 */
static void *os_init_memblock(struct os_memblock *b, size_t size,
                              const char *file, unsigned int line)
{
    const char *lfile;
    int l;

    /* initialize memblock structure */
    if ((l = strlen(file)) < sizeof(b->file))
        lfile = file;
    else
        lfile = file + (strlen(file) - (sizeof(b->file)-1));
    strlcpy(b->file, lfile, sizeof(b->file));
    b->file[sizeof(b->file)-1]='\0';

    b->line = line;
    b->magic = OS_MEMTRACE_MAGIC;
    b->size = size;
    for(l=0; l < OS_MEMTRACE_TAIL_BUF_SIZE; l += 4)
        *(int *)(&b->ptr[size] + l) = OS_MEMTRACE_MAGIC;

    /* register new block in the global list */
    os_thread_mutex_lock(&lock);
    b->prev = NULL;
    b->next = blocks;
    if (blocks) blocks->prev = b;
    blocks = b;
    os_thread_mutex_unlock(&lock);
    memset(&b->ptr, OS_MEMTRACE_DEL_PATTERN, size);

    /* return user data */
    return &b->ptr;
}


/** Release a memory block.
 * @param[in] b: block of memory to be released.
 * @sa os_init_memblock.
 */
static void os_release_memblock(struct os_memblock *b)
{
    os_thread_mutex_lock(&lock);

    if (b->prev)
        b->prev->next = b->next;

    if (b->next)
        b->next->prev = b->prev;

    if (b == blocks)
        blocks = b->next;

    os_thread_mutex_unlock(&lock);
}


/** Intended to be a replacement for malloc(). This function should not
 * be called directly, use os_malloc() instead.
 * @param[in] size: number of bytes to be allocated.
 * @param[in] file: File name in which block is allocated.
 * @param[in] line: Line number at which block is allocated.
 * @return a pointer to the allocated memory.
 * @sa os_free_trace().
 */
void *os_malloc_trace(size_t size, const char *file, unsigned int line)
{
    struct os_memblock *b;

    /* allocate memory, including our overhead */
    b = malloc(size + MEMBLOCK_OFFSET + OS_MEMTRACE_TAIL_BUF_SIZE);
    if (!b)
        return NULL;

    /* register block and return user data */
    return os_init_memblock(b, size, file, line);
}


/** Intended to be a replacement for free(). This function should not
 * be called directly, use os_free() instead.
 * \param[in] ptr: block of memory to be released. It must be a pointer
 * to memory allocated with os_malloc().
 * \param[in] file: File name in which block is freed.
 * \param[in] line: Line number at which block is freed.
 * \sa os_malloc_trace
 */
void os_free_trace(void *ptr, const char *file, unsigned int line)
{
    int l;
    struct os_memblock *b;

    if (ptr == NULL)
        return;

    /* get our memblock back and check magic number */
    b = (struct os_memblock *)((char *)ptr - MEMBLOCK_OFFSET);

    OS_ASSERT_VERBOSE(b->magic == OS_MEMTRACE_MAGIC,
                      "trying to free wrong or corrupted memory block "
                      "at file `%s', line %d\n", file, line);
    for(l=0; l < OS_MEMTRACE_TAIL_BUF_SIZE; l += 4)
    {
        OS_ASSERT_VERBOSE(*(int *)(&b->ptr[b->size] + l) == OS_MEMTRACE_MAGIC,
                          "corrupted memory block at file `%s', line %d\n",
                          file, line);
    }

    /* invalidate memory with OS_MEMTRACE_DEL_PATTERN */
    memset(ptr, OS_MEMTRACE_DEL_PATTERN, b->size);
    os_snprintf(ptr, b->size, "%d%s", line, file);

    /* deregister block */
    os_release_memblock(b);

    /* bye bye */
    free(b);
}


/** Intended to be a replacement for realloc(). This function should not
 * be called directly, use os_realloc() instead.
 * @param[in] ptr       Pointer to the block to reallocate.
 * @param[in] size	Number of bytes to be allocated.
 * @param[in] file	File name in which block is allocated.
 * @param[in] line	Line number at which block is allocated.
 * @return a pointer to the allocated memory.
 * @sa os_free_trace().
 */
void *os_realloc_trace(void *ptr, size_t size, const char *file, unsigned int line)
{
    struct os_memblock *o = (struct os_memblock *)((char *)ptr - MEMBLOCK_OFFSET);
    struct os_memblock *b;

    OS_ASSERT_VERBOSE(! ptr || o->magic == OS_MEMTRACE_MAGIC,
                      "trying to realloc wrong or corrupted memory block "
                      "at file `%s', line %d\n", file, line);

    /* allocate new memory */
    b = os_malloc_trace(size, file, line);
    if (!b)
        return NULL;

    /* copy and free old data */
    if (ptr)
    {
        memcpy(b, ptr, size<o->size?size:o->size);
        os_free_trace(ptr, file, line);
    }

    /* return user data */
    return b;
}


/** Duplicate a string.
*
* The new string must be released with os_free.
*
* @param[in] s     String.
* @param[in] File  name in which block is allocated.
* @param[in] line  Line number at which block is allocated.
*
* @return pointer on the new string
*/
char *os_strdup_trace(const char *s, const char *file, unsigned int line)
{
    int size = strlen(s) + 1;
    char *n;

    if (s == NULL)
        return NULL;

    n = os_malloc_trace(size, file, line);
    if (!n)
        return NULL;

    strlcpy(n, s, size);
    return n;
}

char *os_strndup_trace(const char *s, size_t size, const char *file,
                       unsigned int line)
{
    const char *end_null;
    size_t len;
    char *buf;

    end_null = memchr(s, '\0', size);
    if (end_null != NULL)
        len = end_null - s;
    else
        len = size;

    buf = os_malloc_trace(len + 1, file, line);
    if (buf == NULL)
        return NULL;

    memcpy(buf, s, len);
    buf[len] = '\0';

    return buf;
}

/** @brief perform a check of coherence of memory and display statistics
 *         on memory allocations if sumary asked.
 *
 * No-op if WITH_MEMTRACE is not defined.
 * @param[in] descr     Short string describing what statistics are
 *                      displayed.
 * @param[in] level     What to display
 */
void os_meminfo(const char *descr, os_meminfo_level_t level)
{
    int i;
    uint64_t t;
    struct os_memblock *b;

    if (level == OS_MEMINFO_DETAILED)
    {
        os_syslog(OS_SYSLOG_ERROR, "%s memory allocation statistics\n", descr);
        os_syslog(OS_SYSLOG_ERROR, "---------------------------------\n");
    }

    /* iterate over all blocks, most recent first */
    os_thread_mutex_lock(&lock);

    for (b = blocks, i = 0, t = 0; b; b = b->next, i++)
    {
        int l;
        OS_ASSERT_VERBOSE(b->magic == OS_MEMTRACE_MAGIC,
                          "wrong or corrupted memory block somewhere...\n");
        for (l = 0; l < OS_MEMTRACE_TAIL_BUF_SIZE; l += 4)
            OS_ASSERT_VERBOSE(*(int *)(&b->ptr[b->size] + l) == OS_MEMTRACE_MAGIC,
                              "corrupted memory block at file `%s', line %d\n",
                              b->file, b->line);

        if (level == OS_MEMINFO_DETAILED)
            os_syslog(OS_SYSLOG_ERROR, "%s:%d: %" PRIzu " bytes, at %p\n",
                      b->file, b->line, b->size, &b->ptr);

        t += b->size;
    }

    os_thread_mutex_unlock(&lock);

    if (level >= OS_MEMINFO_SUMMARY)
    {
        os_syslog(OS_SYSLOG_ERROR, "----------------------------\n");
        os_syslog(OS_SYSLOG_ERROR, "Summary:\n");
        os_syslog(OS_SYSLOG_ERROR, "  %d blocks allocated.\n", i);
        os_syslog(OS_SYSLOG_ERROR, "  %"PRIu64" bytes total.\n", t);
        os_syslog(OS_SYSLOG_ERROR, "----------------------------\n");
    }
}
