#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

configure_file("exanodes.in"
           "${CMAKE_CURRENT_BINARY_DIR}/exanodes" @ONLY)
   install(FILES "${CMAKE_CURRENT_BINARY_DIR}/exanodes" DESTINATION "${PKG_ETC_DIR}/logrotate.d")

if (WITH_MONITORING)
    configure_file("exa_agentx.in"
           "${CMAKE_CURRENT_BINARY_DIR}/exa_agentx" @ONLY)
   install(FILES "${CMAKE_CURRENT_BINARY_DIR}/exa_agentx" DESTINATION "${PKG_ETC_DIR}/logrotate.d")
endif (WITH_MONITORING)
