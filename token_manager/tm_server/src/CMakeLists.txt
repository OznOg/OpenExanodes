#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WIN32)
    add_subdirectory(src-windows)
elseif (UNIX)
    add_subdirectory(src-linux)
endif (WIN32)

add_executable(token_manager
    token_manager.c
    tm_tokens.c
    tm_file.c
    tm_err.c)

add_library(tm_file
    tm_file)

target_link_libraries(token_manager
    exa_common_user
    exa_os
    tm_service)

install(TARGETS token_manager DESTINATION ${SBIN_DIR})
