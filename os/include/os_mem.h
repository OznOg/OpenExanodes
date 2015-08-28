/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**
 *  API for memory allocation.
 */

#ifndef _OS_MEM_H
#define _OS_MEM_H

#include <stdlib.h>

#ifdef WIN32
#include <crtdbg.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Memory trace verbosity */
typedef enum os_meminfo_level
{
    OS_MEMINFO_SILENT,
    OS_MEMINFO_SUMMARY,
    OS_MEMINFO_DETAILED
} os_meminfo_level_t;

/**
 * Allocated aligned memory. Allocated memory must be freed with
 * os_aligned_free(). WARNING! Parameters order is inverted compared to
 * the POSIX function memalign().
 *
 * @param size        amount of memory requested
 * @param alignment   the address of the allocated memory will be a multiple
 *                    of alignment, which must be a power of two and a
 *                    multiple of sizeof(void *)
 * @param error_code  The error code in case of failure (NULL if no error
 *                    code wanted)
 *
 * @return the allocated buffer ; NULL is returned in case of error.
 *
 * @os_replace{Linux, posix_memalign}
 * @os_replace{Windows, _aligned_malloc}
 */
void *os_aligned_malloc(size_t size, size_t alignment, int *error_code);

/**
 * Free memory allocated with os_aligned_malloc().
 *
 * @param buffer  buffer allocated with os_aligned_malloc
 *                that is expected to be freed
 *
 * @os_replace{Linux, free}
 * @os_replace{Windows, _aligned_free}
 */
void __os_aligned_free(void *buffer);
#define os_aligned_free(buffer) (__os_aligned_free(buffer), buffer = NULL)

#ifdef WIN32
#define alloca _alloca
#endif

/** @def os_malloc
 * Intended to be a replacement for malloc. If compiled with
 * WITH_MEMTRACE defined, this function will be able to generate
 * statistics on memory allocation.
 * @param[in] size: number of bytes to be allocated.
 * @return a pointer to the allocated memory.
 * @sa os_free
 */

/** @def os_free
 * Intended to be a replacement for free.
 * @param[in] ptr: block of memory to be released. It must be a pointer
 * to memory allocated with os_malloc().
 * @sa os_malloc
 */

/** @def os_meminfo
 * Display statistics on memory allocations. No-op if WITH_MEMTRACE is not
 * defined.
 * @param[in] descr	Short string describing what statistics are
 *			displayed.
 * @param[in] summary	If true, display only a summary.
 */

#if defined(WITH_MEMTRACE) && !defined(WIN32)

void *os_malloc_trace(size_t size, const char *file, unsigned int line);
void *os_realloc_trace(void *ptr, size_t size, const char *file,
                       unsigned int line);
char *os_strdup_trace(const char *s, const char *file, unsigned int line)
    __attribute__((nonnull(1)));
char *os_strndup_trace(const char *s, size_t size, const char *file,
                       unsigned int line)
    __attribute__((nonnull(1)));
void os_free_trace(void *ptr, const char *file, unsigned int line);

#ifdef __cplusplus

#include <new>
#include <exception>

inline void* operator new(size_t size, const char *file, int line)
{
    void *p = os_malloc_trace(size, file, line);
    if (p == 0)
        throw std::bad_alloc(); /* seems to be standard behaviour */
    return p;
}

inline void* operator new[] (size_t size, const char *file, int line)
{
    void *p = os_malloc_trace(size, file, line);
    if (p == 0)
        throw std::bad_alloc(); /* seems to be standard behaviour */
    return p;
}
#define os_new new(__FILE__, __LINE__)

inline void operator delete(void *p, const char *file, int line)
{
    os_free_trace(p, file, line );
}

inline void operator delete[](void *p, const char *file, int line)
{
    os_free_trace(p, file, line );
}
#define os_delete delete(__FILE__, __LINE__)

#endif /* __cplusplus */

#elif defined(WITH_MEMTRACE) && defined(WIN32)

#define os_malloc_trace(size, file, line) \
    _malloc_dbg(size, _NORMAL_BLOCK, file, line)
#define os_realloc_trace(ptr, size, file, line) \
    _realloc_dbg(ptr, size, _NORMAL_BLOCK, file, line)
#define os_strdup_trace(s, file, line) \
    _strdup_dbg(s, _NORMAL_BLOCK, file, line)
#define os_free_trace(ptr, file, line) \
    _free_dbg(ptr, _NORMAL_BLOCK)

#else /* WITH_MEMTRACE */

#define os_malloc_trace(size, file, line)        malloc(size)
#define os_realloc_trace(ptr, size, file, line)  realloc(ptr, size)
#define os_free_trace(ptr, file, line)           free(ptr)
#include <string.h> /* for strdup */
#ifdef WIN32
#define os_strdup_trace(s, file, line)           _strdup(s)
 /* strndup was reimplemented in os_string on windows */
#define os_strndup_trace(s, size, file, line)    strndup(s, size)
#else
#define os_strdup_trace(s, file, line)           strdup(s)
#define os_strndup_trace(s, size, file, line)    strndup(s, size)
#endif

#endif /* WITH_MEMTRACE */

#ifdef WITH_MEMTRACE
void os_meminfo(const char *descr, os_meminfo_level_t level);
#else
#define os_meminfo(descr, log_level) /* empty */
#endif /* WITH_MEMTRACE */

#ifdef WIN32
char *os_strndup_trace(const char *s, size_t size, const char *file,
                       unsigned int line);
#endif

#define os_malloc(size)        os_malloc_trace(size, __FILE__, __LINE__)
#define os_strdup(string)      os_strdup_trace(string, __FILE__, __LINE__)
#define os_strndup(str, size)  os_strndup_trace(str, size, __FILE__, __LINE__)
#define os_realloc(ptr, size)  os_realloc_trace(ptr, size, __FILE__, __LINE__)
#define os_free(ptr)           (os_free_trace(ptr, __FILE__, __LINE__), ptr = NULL)

#ifdef __cplusplus
}
#endif

#endif /* _OS_MEM_H */
