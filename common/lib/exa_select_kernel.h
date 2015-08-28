/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef _EXA_SELECT_KERNEL_H
#define _EXA_SELECT_KERNEL_H

int exa_select_alloc(struct file *filp);
void exa_select_free(struct file *filp);
int exa_select_kernel(struct file *filp, unsigned int cmd, unsigned long arg);

#endif /* _EXA_SELECT_KERNEL_H */
