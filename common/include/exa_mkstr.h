/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _EXA_MKSTR_H
#define _EXA_MKSTR_H

/*
 * The macro exa_mkstr(a) makes a string from an integer value. For
 * example, the exa_mkstr(25) will be replaced by "25".
 */

#define exa_mkstrinter(a) #a
#define exa_mkstr(a) exa_mkstrinter(a)

#endif /* _EXA_MKSTR_H */
