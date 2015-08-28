#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_executable(tm_tool
    tm_tool.c)

target_link_libraries(tm_tool
    tm_client
    tm_file
    exa_common_user
    exa_os)

install(TARGETS tm_tool DESTINATION ${SBIN_DIR})
