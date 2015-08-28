/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXPORT_IO_H
#define EXPORT_IO_H

/**
 * Initialization of static data necessary to perform IO on exports.
 * *Must* be called prior to using any function of this API.
 *
 * @return EXA_SUCCESS if successful, a negative error code otherwise
 */
int export_io_static_init(void);

/**
 * Clean up the static data used to perform IO on exports.
 * *Must* be called once done with exports.
 */
void export_io_static_cleanup(void);

#endif /* EXPORT_IO_H */
