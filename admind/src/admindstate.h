/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef H_ADMIND_STATE
#define H_ADMIND_STATE

typedef enum AdmindStatus {
  ADMIND_NOCONFIG = 0x1, /* No config file */
  ADMIND_STOPPED  = 0x2, /* We have a config file but we are not started */
  ADMIND_STARTING = 0x4, /* We are ready to start, the quorum will start us */
  ADMIND_STARTED  = 0x8, /* We are started */
} AdmindStatus;

#define ADMIND_ANY ((AdmindStatus)(ADMIND_NOCONFIG \
                                 | ADMIND_STOPPED \
                                 | ADMIND_STARTING \
				 | ADMIND_STARTED))

/** Check the validity of an Admind status. ADMIND_ANY is a mask, not a status
    and thus is not considered valid */
#define ADMIND_STATUS_VALID(status) \
    ((status) == ADMIND_NOCONFIG \
     || (status) == ADMIND_STOPPED \
     || (status) == ADMIND_STARTING \
     || (status) == ADMIND_STARTED)

void adm_state_static_init(AdmindStatus status);

void adm_set_state(AdmindStatus status);

AdmindStatus adm_get_state(void);

#endif /* H_ADMIND_STATE */
