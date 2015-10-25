#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_PERF)
    set(PERF_SOURCES nbd_serverd_perf.c)
    set(LIBPERF exa_perf_instance exaperf)
endif (WITH_PERF)

add_executable(exa_serverd
    nbd_serverd.c
    ndevs.c
    nbd_disk_thread.c
    ${PERF_SOURCES})

target_link_libraries(exa_serverd
    daemon_server
    examsg
    ${LIBPERF}
    exa_common_user
    daemon_request_queue
    exa_nbd_list
    nbd_tcp
    exalogclient
    exa_os
    os_daemon_child
    rdev)

install(TARGETS exa_serverd DESTINATION ${SBIN_DIR})
