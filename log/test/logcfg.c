/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <stdio.h>

#include "log/include/log.h"
#include "examsg/include/examsg.h"

int
main(int argc, char *argv[])
{
  exalog_as(EXAMSG_TEST_ID);

  exalog_configure(EXAMSG_TEST_ID, EXALOG_LEVEL_DEBUG);
  exalog_debug("You should see this");
  exalog_error("You should see this too");

  exalog_configure(EXAMSG_TEST_ID, EXALOG_LEVEL_ERROR);
  exalog_debug("You shouldn't see this");
  exalog_error("But you should see this");

  return 0;
}
