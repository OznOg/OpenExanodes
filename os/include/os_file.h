/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_FILE_H
#define _OS_FILE_H

#include <fcntl.h>

#ifdef WIN32
#include <direct.h>  /* for MAX_PATH */
#else
#include <unistd.h>
#include <limits.h>  /* for PATH_MAX */
#endif

#include "os/include/os_inttypes.h"
#include "os/include/os_stdio.h"

#ifdef WIN32
#define OS_PATH_MAX  _MAX_PATH
#define OS_FILE_SEP "\\"
#else
#define OS_PATH_MAX  PATH_MAX
#define OS_FILE_SEP "/"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Atomically rename a file.
 *
 * @param[in]  src   Old filename
 * @param[in]  dest  New filename
 *
 * Warnings:
 *   - Do not use it with directories.
 *   - Asserts if the platform doesn't support atomic renaming.
 *
 * In case of failure, errno is set to the error code.
 *
 * @return 0 if successful, a negative error code otherwise
 *
 * @os_replace{Linux, rename}
 * @os_replace{Windows, move_file_transacted}
 */
int os_file_rename(const char *src, const char *dest);

/**
 * Get the base name of a path.
 *
 * Warning: the input path is modified.
 *
 * @param[in,out] path  Path to extract the basename of
 *
 * @return Basename if successful, NULL otherwise (note that if the path has
 *         no base name, the function returns "")
 *
 * @os_replace{Linux, basename}
 * @os_replace{Windows, _splitpath, _splitpath_s}
 */
char *os_basename(char *path);

/**
 * Get the program name without the extension from a given path
 *
 * Warning: the input path is modified.
 *
 * @param path  Path to extract program name from
 *
 * @return Program name if successfull, NULL otherwise
 */
char *os_program_name(char *path);

/**
 * Tell whether a path is absolute.
 *
 * @param[in] path  Path to check
 *
 * @return true if absolute, false otherwise (a NULL path is considered to be
 *         non-absolute)
 */
bool os_path_is_absolute(const char *path);


/**
 * Return the directory part of a path
 *
 * @param[in] path  Path to parse.
 *
 * NOTE: The source string path may be modified like when using
 * dirname.
 *
 * @return a pointer to path
 *
 */
char *os_dirname(char *path);

/* FIXME WIN32  probably os_ functions are needed here to be coherent */
#ifdef WIN32
#include <io.h>

#define unlink _unlink
#define lseek _lseek
#define read _read
#ifndef __cplusplus
#define open _open
#define close _close
#define write _write
#endif /*__cplusplus*/
#define fileno _fileno

#endif

#ifdef __cplusplus
}
#endif

#endif /* _OS_FILE_H */
