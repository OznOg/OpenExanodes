/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_SYSTEM_H__
#define __EXA_SYSTEM_H__


/** \brief execute an external programm
 * This function is a wrapper around a fork + exec facility.
 *
 * \return -1 if error (command not executed) or the return exit value
 */
int exa_system(char *const *command);



#endif /* __EXA_SYSTEM_H__ */
