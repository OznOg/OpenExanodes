/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __ADM_DESERIALIZE_H
#define __ADM_DESERIALIZE_H


int adm_deserialize_from_file(const char *path, char *error_msg, int create);
int adm_deserialize_from_memory(const char *buffer, int size,
				char *error_msg, int create);


#endif /* __ADM_DESERIALIZE_H */
