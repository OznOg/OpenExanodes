#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_subdirectory(src)

configure_file(exa_fsscript.in "${CMAKE_CURRENT_BINARY_DIR}/exa_fsscript" @ONLY)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/exa_fsscript" DESTINATION ${SBIN_DIR})
