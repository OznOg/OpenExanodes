#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_TOOLS)
  add_executable(exa_rdev_test
    exa_rdev_test.c)

  add_executable(exa_rdev_broken
    exa_rdev_broken.c)

  target_link_libraries(exa_rdev_test
    exa_os
    exa_common_user
    rdev)

  target_link_libraries(exa_rdev_broken
    service_rdev
    exa_os
    exa_common_user)

  install(TARGETS exa_rdev_test DESTINATION ${SBIN_DIR})
  install(TARGETS exa_rdev_broken DESTINATION ${SBIN_DIR})
endif (WITH_TOOLS)
