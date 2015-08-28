/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __MD_TRAP_SENDER_H__
#define __MD_TRAP_SENDER_H__

#include "monitoring/common/include/md_types.h"


#define MD_TRAP_SENDER_QUEUE_SIZE 512

typedef enum {
    MD_TRAP_SENDER_SUCCESS = 0,
    MD_TRAP_SENDER_OVERFLOW,
    MD_TRAP_SENDER_UNDERFLOW
} md_trap_sender_error_code_t;

md_trap_sender_error_code_t md_trap_sender_enqueue(const md_msg_event_trap_t *trap);

bool md_trap_sender_queue_empty(void);

void md_trap_sender_loop(void);

void md_trap_sender_thread(void *pstop);

void md_trap_sender_static_init(void);
void md_trap_sender_static_clean(void);

#endif /* __MD_TRAP_SENDER_H__ */
