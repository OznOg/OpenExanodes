#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

# FIXME WIN32: The following programs are not cross-platform.
if (NOT WIN32)
    add_library(md5 STATIC
        md5.c
        md5.h)

    add_executable(exatest_get_md5 get_md5.c)
    target_link_libraries(exatest_get_md5 md5)

    add_executable(exatest_mydd mydd.c)
    target_link_libraries(exatest_mydd
                          exa_common_user
                          md5)

    install(TARGETS exatest_get_md5 DESTINATION ${SBIN_DIR})
    install(TARGETS exatest_mydd DESTINATION ${SBIN_DIR})
endif (NOT WIN32)