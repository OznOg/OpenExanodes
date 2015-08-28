/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADM_SERIALIZE_H
#define __ADM_SERIALIZE_H

int adm_serialize_to_memory(char *buffer, int size, int create);
int adm_serialize_to_null(int create);

#endif /* __ADM_SERIALIZE_H */
