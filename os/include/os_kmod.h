/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_KMOD_H

#ifdef WIN32

#define os_kmod_load(module_name) 0
#define os_kmod_unload(module_name) 0

#else /* WIN32 */

/**
 * Load a kernel module and create the associated char device.
 *
 * @param[in] module_name  Name of the module to load
 *
 * @return 0 if success, positive (exit) error code otherwise.
 */
int os_kmod_load(const char *module_name);

/**
 * Unload a kernel module
 *
 * @param[in] module_name  Name of the module to unload.
 *
 * @return 0 if success, positive (exit) error code otherwise.
 */
int os_kmod_unload(const char *module_name);

#endif /* WIN32 */

#endif /* _OS_KMOD_H */
