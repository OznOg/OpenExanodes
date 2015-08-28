/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/admindstate.h"
#include "os/include/os_thread.h"
#include "common/include/exa_assert.h"

static os_thread_mutex_t mutex;

/* This must be initialized to bear a suitable value. */
static AdmindStatus status = (AdmindStatus)-1;

void adm_state_static_init(AdmindStatus _status)
{
    status = _status;
    os_thread_mutex_init(&mutex);
}

/** \brief Set the admind state
 *
 * \param[in] status: the admind state
 */
void
adm_set_state(AdmindStatus _status)
{
  EXA_ASSERT(ADMIND_STATUS_VALID(_status));

  os_thread_mutex_lock(&mutex);

  status = _status;

  os_thread_mutex_unlock(&mutex);
}

/** \brief Get the admind state
 *
 * return the admind state
 */
AdmindStatus
adm_get_state(void)
{
  AdmindStatus _status;

  os_thread_mutex_lock(&mutex);

  _status = status;

  os_thread_mutex_unlock(&mutex);

  EXA_ASSERT(ADMIND_STATUS_VALID(_status));

  return _status;
}
