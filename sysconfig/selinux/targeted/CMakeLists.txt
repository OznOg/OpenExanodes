#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (${SELINUX_POLICY_VERSION} GREATER 24)
    add_custom_target(exa_fsd.pp ALL
        COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/exa_fsd.fc .
        COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/exa_fsd.if .
        COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/exa_fsd-centos7.te exa_fsd.te
        COMMAND make -f ${SELINUX_MAKEFILE}
        DEPENDS exa_fsd-centos7.te exa_fsd.fc exa_fsd.if)
else()
    add_custom_target(exa_fsd.pp ALL
        COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/exa_fsd.* .
        COMMAND make -f ${SELINUX_MAKEFILE}
        DEPENDS exa_fsd.te exa_fsd.fc exa_fsd.if)
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/exa_fsd.pp
    DESTINATION ${DATA_DIR}/selinux/targeted)
