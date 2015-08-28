#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_PERF)
    set(TARGET_PERF target_perf.c)
    set(LIBPERF exaperf)
endif (WITH_PERF)

add_library(iscsi_target STATIC
    algopr_network.c
    scsi_command.c
    iscsi.c
    parameters.c
    pr_lock_algo.c
    scsi_persistent_reservations.c
    iscsi_negociation.c
    scsi_transport_iscsi.c
    ${TARGET_PERF})

target_link_libraries(iscsi_target
    exa_export
    lun
    iqn_filter
    iqn
    exa_common_user
    ${LIBPERF})

# The following libraries are *always* built since some of the code in Admind
# requires it. Eventually, we'll have simultaneous support for both iSCSI and
# bdev anyway.

add_library(lun STATIC
    lun.c)

target_link_libraries(lun exa_common_user)


add_library(iqn STATIC
    iqn.c)

target_link_libraries(iqn exa_common_user)

add_library(iqn_filter STATIC
    iqn_filter.c)

target_link_libraries(iqn_filter iqn exa_common_user)
