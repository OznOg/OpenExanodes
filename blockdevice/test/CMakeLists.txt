#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_unit_test(ut_blockdevice
    ../src/blockdevice.c)

target_link_libraries(ut_blockdevice
    blockdevice)

add_unit_test(ut_sys_blockdevice
    ../src/sys_blockdevice.c)

target_link_libraries(ut_sys_blockdevice
    blockdevice)

if (WITH_UT_ROOT)
    add_unit_test(ut_blockdevice_stream
        ../src/blockdevice_stream.c)

    target_link_libraries(ut_blockdevice_stream
        blockdevice
        sys_blockdevice
        vrt_stream
        blockdevice_stream)
endif()

