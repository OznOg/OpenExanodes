/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef VRT_STREAM_H
#define VRT_STREAM_H

/* FIXME Should return -EPERM when the access mode doesn't allow an operation
   and return -EOPNOTSUPP only when the operation is not implemented (NULL) */

#include "os/include/os_inttypes.h"

#include <stdarg.h>

/** Stream access mode */
typedef enum
{
    STREAM_ACCESS_READ,   /**< Read only */
    STREAM_ACCESS_WRITE,  /**< Write only */
    STREAM_ACCESS_RW      /**< Read and write */
} stream_access_t;

#define STREAM_ACCESS__FIRST  STREAM_ACCESS_READ
#define STREAM_ACCESS__LAST   STREAM_ACCESS_RW

/** Tell whether an access mode is valid */
#define STREAM_ACCESS_IS_VALID(a) \
    ((a) >= STREAM_ACCESS__FIRST && (a) <= STREAM_ACCESS__LAST)

/** Stream seek mode */
typedef enum
{
    STREAM_SEEK_FROM_BEGINNING,  /**< From the beginning */
    STREAM_SEEK_FROM_END,        /**< From the end */
    STREAM_SEEK_FROM_POS         /**< Relatively to current position */
} stream_seek_t;

/** Tell whether a seek mode is valid */
#define STREAM_SEEK_IS_VALID(s) \
    ((s) >= STREAM_SEEK_FROM_BEGINNING && (s) <= STREAM_SEEK_FROM_POS)

/** Stream operations */
typedef struct
{
    /* All operations are synchronous.
       If an operation is not supported, the operation shall be NULL. */
    /* XXX Add expected contract for each of these */
    int (*read_op)(void *context, void *buf, size_t size);
    int (*write_op)(void *context, const void *buf, size_t size);
    int (*flush_op)(void *context);
    int (*seek_op)(void *context, int64_t offset, stream_seek_t seek);
    uint64_t (*tell_op)(void *context);
    void (*close_op)(void *context);
} stream_ops_t;

/** Generic stream */
typedef struct stream stream_t;

/**
 * Create a stream.
 *
 * @param[out] stream   Stream created
 * @param[in]  context  Stream context
 * @param[in]  ops      Stream operations on the context (copied, the caller
 *                      need not keep a copy)
 * @param[in]  access   Access mode
 *
 * @return 0 if successful, a negative error code otherwise
 */
int stream_open(stream_t **stream, void *context, const stream_ops_t *ops,
                stream_access_t access);

/**
 * Close a stream.
 *
 * NOTES:
 *   - Automatically calls flush_op, if defined.
 *   - The context is *not* freed. It is left to close_op to do so, if needed.
 *
 * @param[in,out] stream  Stream to free
 */
void __stream_close(stream_t *stream);

/** Free a stream and set it to NULL */
#define stream_close(stream)  (__stream_close(stream), (stream) = NULL)

/**
 * Read from a stream.
 *
 * @param[in,out] stream  Stream to read from
 * @param[out]    buf     Buffer to hold what's read
 * @param[in]     size    Size of buffer, in bytes
 *
 * Note that if size is 0, the actual implementation (for the stream's
 * context) is not even called.
 *
 * @return number of bytes read if successful, -EINVAL if a parameter is invalid,
 *         -EOPNOTSUPP if the operation is not supported by the stream, or any
 *         negative error code the underlying context-specific operation may issue
 */
int stream_read(stream_t *stream, void *buf, size_t size);

/**
 * Write to a stream.
 *
 * @param[in,out] stream  Stream to write to
 * @param[in]     buf     Buffer to write
 * @param[size]   size    Size of buffer, in bytes
 *
 * Note that if size is 0, the actual implementation (for the stream's
 * context) is not even called.
 *
 * @return number of bytes written if successful,
 *         -EINVAL if a parameter is invalid,
 *         -EOPNOTSUPP if the operation is not supported by the stream,
 *         -ENOSPC when attempting to write beyond the end of the stream
 *                 (if relevant for the underlying context),
 *         or any negative error code the underlying context-specific
 *         operation may issue
 */
int stream_write(stream_t *stream, const void *buf, size_t size);

/**
 * Formatted printing of a variable argument list to a stream.
 *
 * WARNING - This function allocates memory.
 *
 * @param[in,out] stream  Stream to print to
 * @param[in]     fmt     Format string
 * @param[in]     al      Variable argument list
 *
 * @return same as stream_write()
 */
int stream_vprintf(stream_t *stream, const char *fmt, va_list al);

/**
 * Formatted printing to a stream.
 *
 * WARNING - This function allocates memory.
 *
 * @param[in,out] stream  Stream to print to
 * @param[in]     fmt     Format string
 * @param[in]     ...     Optional values to print
 *
 * @return same as stream_vprintf()
 */
int stream_printf(stream_t *stream, const char *fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));

/**
 * Flush the writes on a stream.
 *
 * If the stream is readonly, this function does not call flush_op.
 *
 * @param[in,out] stream  Stream to flush
 *
 * @return 0 if successful, -EINVAL if the stream is invalid, -EOPNOTSUPP if
 *         the operation is not supported by the stream, or any a negative
 *         error code the underlying context-specific operation may issue
 */
int stream_flush(stream_t *stream);

/**
 * Seek on a stream.
 *
 * @param[in,out] stream  Stream to seek on
 * @param[in]     offset  Offset to seek to
 * @param[in]     seek    Seek mode
 *
 * @return 0 if successful, -EINVAL if a parameter is invalid or if the offset
 *         is outside the valid range (taking the seek mode into account),
 *         -EOPNOTSUPP if the operation is not supported by the stream,
 *         or any negative error code the underlying context-specific
 *         operation may issue
 */
int stream_seek(stream_t *stream, int64_t offset, stream_seek_t seek);

/**
 * Rewind a stream.
 *
 * Equivalent to stream_seek(..., 0, STREAM_SEEK_FROM_BEGINNING).
 *
 * @param[in,out] stream  Stream to rewind
 *
 * @return same values as stream_seek()
 */
int stream_rewind(stream_t *stream);

/** Special offset value returned by stream_tell() on error */
#define STREAM_TELL_ERROR  ((uint64_t)-1)

/**
 * Tell the position in a stream.
 *
 * @param[in] stream  Stream to get the position of
 *
 * @return current absolute position or STREAM_TELL_ERROR
 */
uint64_t stream_tell(const stream_t *stream);

/**
 * Get the access mode of a stream.
 *
 * @param[in] stream  Stream to get the access mode of
 *
 * @return access mode if the stream is valid, or an arbitrary invalid value
 *         otherwise (check with STREAM_ACCESS_IS_VALID()).
 */
stream_access_t stream_access(const stream_t *stream);

/**
 * Get the context of a stream.
 *
 * NOTES:
 *
 *   - This function may only be used by stream implementations.
 *     It is *not* meant to be used by stream users.
 *
 *   - A stream implementation using this function should have a magic
 *     as part of its context, so that it's able to check that the
 *     context it gets is indeed what it expects (poor man's typing).
 *
 * @param[in] stream  Stream to get the context of
 *
 * @return context if successful, NULL otherwise
 */
void *__stream_context(const stream_t *stream);

#endif /* VRT_STREAM_H */
