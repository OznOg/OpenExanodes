/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_CLINFO_FILESYSTEM_H
#define __EXA_CLINFO_FILESYSTEM_H

#ifdef WITH_FS

#include <libxml/tree.h>

#include "fs/include/exa_fsd.h"

int cluster_clinfo_filesystem(int thr_nb, xmlNodePtr fs_node,
			      fs_data_t* fs, bool get_fs_size);
void local_clinfo_fs(int thr_nb, void *msg);

#endif
#endif
