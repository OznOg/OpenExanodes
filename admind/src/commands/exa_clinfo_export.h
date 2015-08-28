/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_CLINFO_EXPORT_H
#define __EXA_CLINFO_EXPORT_H

#include <libxml/tree.h>

#include "common/include/uuid.h"

void local_clinfo_export(int thr_nb, void *msg);
void local_clinfo_get_nth_iqn(int thr_nb, void *msg);

int cluster_clinfo_export_by_volume(int thr_nb, xmlNodePtr father_node,
				    exa_uuid_t *volume_uuid);


#endif
