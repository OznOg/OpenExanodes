#
# Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_PERF)
    set(NBD_CLIENTD_PERF nbd_clientd_perf_private.c)
    set(LIBPERF exaperf)
endif (WITH_PERF)

add_executable(exa_clientd
    nbd_clientd.c
    nbd_blockdevice.c
    nbd_stats.c
    bd_user_user.c
    ${NBD_CLIENTD_PERF})

if (WITH_MONITORING)
    set(MONITORING_LIBRARIES md_client)
endif (WITH_MONITORING)

target_link_libraries(exa_clientd
    executive_export
    vrt
    ${MONITORING_LIBRARIES}
    daemon_server
    exa_lum
    exalogclient
    examsg
    ${LIBPERF}
    exa_common_user
    daemon_request_queue
    blockdevice
    nbd_tcp
    exa_nbd_list
    exa_os
    os_daemon_child)

install(TARGETS exa_clientd DESTINATION ${SBIN_DIR})
