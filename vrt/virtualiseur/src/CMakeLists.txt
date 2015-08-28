#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_PERF)
    set(VRT_PERF vrt_perf.c)
    set(LIBPERF exaperf)
endif (WITH_PERF)

add_library(spof_group
    chunk.c
    spof_group.c)

target_link_libraries(spof_group
    exa_common_user)

add_library(vrt STATIC
    storage.c
    vrt_cmd.c
    vrt_cmd_threads.c
    vrt_info.c
    vrt_group.c
    vrt_layout.c
    vrt_mempool.c
    vrt_metadata.c
    vrt_module.c
    vrt_msg.c
    vrt_nodes.c
    ${VRT_PERF}
    vrt_realdev.c
    vrt_rebuild.c
    vrt_request.c
    vrt_stats.c
    vrt_volume.c)

add_library(realdev_superblock STATIC
      realdev_superblock.c)

target_link_libraries(vrt
    blockdevice_stream
    narrowed_stream
    checksum_stream
    rdev
    realdev_superblock
    sstriping
    rain1
    assembly
    spof_group
    ${LIBPERF}
    vrt_common
    volume_blockdevice)

add_library(volume_blockdevice STATIC
      volume_blockdevice.c)

target_link_libraries(volume_blockdevice
    exa_nbd_list)
