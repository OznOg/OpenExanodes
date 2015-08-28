#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_subdirectory(md5)

# FIXME WIN32: `exatest_odirect` is not cross-platform.
if (NOT WIN32)
    add_executable(exatest_odirect
        exatest_odirect.c)
    target_link_libraries(exatest_odirect
        exa_common_user
        exa_os)
    install(TARGETS exatest_odirect DESTINATION ${SBIN_DIR})
endif (NOT WIN32)

install(PROGRAMS exatest_gentree DESTINATION ${SBIN_DIR})
