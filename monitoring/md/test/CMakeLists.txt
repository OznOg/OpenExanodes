#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_unit_test(ut_exa_md
    ../src/md_event_list.c
    ../src/md_trap_sender.c
    ../src/md_srv_com.c
    ../../agentx/src/md_handler.c
    ../../agentx/src/md_heartbeat.c)

target_link_libraries(ut_exa_md
    md_com
    md_com_socket
    exalogclientfake
    exa_os
    ${LIBPTHREAD})

add_unit_test(ut_md_srv_com
    ../src/md_srv_com.c)

target_link_libraries(ut_md_srv_com
    md_com
    md_com_socket
    exalogclientfake
    exa_os
    ${LIBPTHREAD})

add_unit_test(ut_md_trap_sender
    ../src/md_trap_sender)

target_link_libraries(ut_md_trap_sender
    md_com
    exalogclientfake
    exa_os
    ${LIBPTHREAD})

add_unit_test(ut_md_event_list
    ../src/md_event_list.c
    ../src/md_trap_sender.c)

target_link_libraries(ut_md_event_list
    md_com
    exalogclientfake
    exa_os
    ${LIBPTHREAD})
