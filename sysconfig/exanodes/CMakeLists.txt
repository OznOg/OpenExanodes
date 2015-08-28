#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_FS AND NOT WIN32)
  add_subdirectory(preumount.d)
  add_subdirectory(postmount.d)
endif (WITH_FS AND NOT WIN32)

if (WITH_PERF)
  install(FILES exaperf.conf
          DESTINATION "${SYSCONF_DIR}/exanodes")
endif (WITH_PERF)
