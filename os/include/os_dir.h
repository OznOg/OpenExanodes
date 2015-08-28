/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_DIR_H

/*
 * FIXME Should set errno explicitely? Or just on Windows side?
 */

#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
    #ifndef S_ISDIR
    	#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
    #endif

    #ifndef S_ISREG
    	#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #endif

    #ifndef S_ISLNK
    	#define S_ISLNK(m) 0
    #endif

    #ifndef S_ISBLK
        #define S_IFBLK    0x3000
        #define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
    #endif
#endif

#include "os/include/os_inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Directory handle */
typedef struct os_dir os_dir_t;

/**
 * Create a directory.
 *
 * @param[in]  path  Path of directory to create.
 *
 * - All intermediate directory components must exist.
 * - Creation of an already existing directory will succeed.
 *
 *
 * @return 0 if successful, error code otherwise
 *
 * @os_replace{Linux, mkdir}
 * @os_replace{Windows, CreateDirectory, _mkdir}
 */
int os_dir_create(const char *path);

/**
 * Create a directory recursively.
 *
 * @param[in]  path  Path of directory to create.
 *
 * - All intermediate directories are created if they don't exist.
 * - Creation of an already existing directory will succeed.
 *
 * @return 0 if successful, error code otherwise
 */
int os_dir_create_recursive(const char *path);

/**
 * Remove a directory.
 *
 * @param[in] path  Path of directory to remove.
 *
 * - The directory must be empty.
 * - Removal of an inexistent directory will succeed.
 *
 * @return 0 if successful, error code otherwise
 *
 * @os_replace{Linux, rmdir, remove}
 * @os_replace{Windows, RemoveDirectory, _rmdir}
 */
int os_dir_remove(const char *path);

/**
 * Remove a directory and recursively all files and subdirs
 *
 * @param[in] path  Path of directory to remove.
 *
 * - The whole tree rooted at the given directory is deleted.
 * - Removal of an inexistent directory will succeed.
 *
 * @return 0 if successful, error number otherwise
 *
 * @os_replace{Linux, rmdir, remove}
 * @os_replace{Windows, _rmdir, SHFileOperation}
 */
int os_dir_remove_tree(const char *path);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _OS_DIR_H */
