#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WIN32)
elseif (UNIX)
    configure_file(token_manager.in "${CMAKE_CURRENT_BINARY_DIR}/token_manager" @ONLY)
    configure_file(logrotate.in "${CMAKE_CURRENT_BINARY_DIR}/token_manager.logrotate" @ONLY)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/token_manager" DESTINATION "/etc/init.d")
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/token_manager.logrotate" DESTINATION "/etc/logrotate.d")
endif (WIN32)
