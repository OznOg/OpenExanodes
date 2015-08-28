#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_definitions(-DCURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_unit_test(ut_group_assembly_regression
    test_group_assembly_funcs.c
    ../../../vrt/virtualiseur/src/chunk.c
    ../../../vrt/virtualiseur/src/storage.c)

target_link_libraries(ut_group_assembly_regression
    fake_rdev
    assembly
    exa_common_user
    exa_os
    exalogclientfake
    spof_group
    blockdevice)

add_executable(test_group_assembly
    test_group_assembly.c
    test_group_assembly_funcs.c
    ../../../vrt/virtualiseur/src/chunk.c
    ../../../vrt/virtualiseur/src/storage.c)

target_link_libraries(test_group_assembly
    fake_rdev
    assembly
    assembly_prediction
    exa_common_user
    exa_os
    exalogclientfake
    spof_group
    blockdevice)

add_unit_test(ut_extent
	../src/extent.c)

target_link_libraries(ut_extent
    vrt_common
    memory_stream)

add_unit_test(ut_assembly_prediction
        ../src/assembly_prediction.c)

target_link_libraries(ut_assembly_prediction
    exa_common_user)

add_unit_test(ut_assembly_slot
    ../src/assembly_slot.c
    ../../../vrt/virtualiseur/src/chunk.c
    ../../../vrt/virtualiseur/src/storage.c
    ../../../vrt/virtualiseur/src/spof_group.c)

target_link_libraries(ut_assembly_slot
    fake_rdev
    fake_storage
    assembly
    memory_stream
    exa_common_user
    # FIXME - THIS IS CRAP
    blockdevice)

add_unit_test(ut_assembly_volume
    ../src/assembly_volume.c
    ../../../vrt/virtualiseur/src/chunk.c
    ../../../vrt/virtualiseur/src/storage.c
    ../../../vrt/virtualiseur/src/spof_group.c)

target_link_libraries(ut_assembly_volume
    fake_rdev
    fake_storage
    assembly
    memory_stream
    blockdevice
    fake_assembly_group
    exalogclientfake)

add_unit_test(ut_assembly_group
    ../src/assembly_group.c
    ../../../vrt/virtualiseur/src/chunk.c
    ../../../vrt/virtualiseur/src/storage.c
    ../../../vrt/virtualiseur/src/spof_group.c)

target_link_libraries(ut_assembly_group
    fake_rdev
    fake_storage
    assembly
    fake_assembly_group
    memory_stream
    exalogclientfake
    exa_os
    # FIXME - THIS IS CRAP
    blockdevice)
