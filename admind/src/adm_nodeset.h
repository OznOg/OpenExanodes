/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADM_NODESET_H
#define __ADM_NODESET_H

#include <sys/types.h>
#include <libxml/tree.h>

#include "common/include/exa_nodeset.h"


extern const exa_nodeset_t adm_nodeset_all;


int adm_nodeset_contains_me(const exa_nodeset_t *set);
const char *adm_nodeid_to_name(exa_nodeid_t id);
exa_nodeid_t adm_nodeid_from_name(const char *name);
void adm_nodeset_set_all(exa_nodeset_t *set);
int adm_nodeset_from_names(exa_nodeset_t *set, const char *nodelist);
void adm_nodeset_to_names(const exa_nodeset_t *set, char *list, size_t n);

int xml_set_prop_nodeset(xmlNodePtr xml_node, const char *name,
			 const exa_nodeset_t *set);


#endif /* __ADM_NODESET_H */
