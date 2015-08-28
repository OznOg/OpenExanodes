#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

link_directories(${NETSNMP_LIBRARY_DIRS})

add_executable(exa_agentx
    mib_notify.c
    md_handler.c
    md_heartbeat.c
    exa_agentx.c)

target_link_libraries(exa_agentx
    md_com
    md_com_socket
    exa_common_user
    exa_os
    ${NETSNMP_LIBRARIES}
    ${LIBPTHREAD}
    ${LIBRT})

set_target_properties(exa_agentx
    PROPERTIES LINK_FLAGS "${NETSNMP_LINK_FLAGS}"
               INSTALL_RPATH "${NETSNMP_RPATH}"
               BUILD_WITH_INSTALL_RPATH TRUE)

install(TARGETS exa_agentx DESTINATION ${SBIN_DIR})
install(FILES SEANODES.MIB DESTINATION ${DATA_DIR}/snmp/mibs)
