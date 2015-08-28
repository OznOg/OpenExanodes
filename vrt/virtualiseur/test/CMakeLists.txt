#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_unit_test(ut_storage
    ../src/storage.c)

target_link_libraries(ut_storage
    fake_storage
    fake_rdev
    # FIXME assembly shouldn't be there (pulled due to use
    # of extents in rdev->chunks.free_chunks)
    assembly
    spof_group
    memory_stream
    exa_common_user
    blockdevice)

add_unit_test(ut_spof_group
    ../src/spof_group.c
    ../src/chunk.c)

target_link_libraries(ut_spof_group
    exa_common_user)

add_unit_test(ut_vrt_volume
    ../src/vrt_volume.c
    # FIXME Should use libs instead
    ../src/chunk.c
    ../src/storage.c
    ../src/vrt_stats.c)

target_link_libraries(ut_vrt_volume
    fake_rdev
    fake_storage
    fake_assembly_group
    spof_group
    assembly
    memory_stream
    stat_stream
    exalogclientfake
    exa_common_user
    exa_os
    # FIXME Shouldn't need this
    blockdevice
    volume_blockdevice)

add_unit_test(ut_vrt_group
    ../src/vrt_group.c
    # FIXME Should use libs instead
    ../src/chunk.c
    ../src/storage.c
    ../src/vrt_layout.c
    ../src/vrt_volume.c)

target_link_libraries(ut_vrt_group
    sstriping
    rain1
    fake_rdev
    fake_storage
    fake_assembly_group
    spof_group
    assembly
    memory_stream
    stat_stream
    exalogclientfake
    exa_common_user
    exa_os
    blockdevice
    volume_blockdevice)
