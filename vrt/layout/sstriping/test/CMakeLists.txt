#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_unit_test(ut_lay_sstriping_superblock
    ../src/lay_sstriping_superblock.c
    # FIXME Should use libraries instead
    ../src/lay_sstriping_group.c
    ../../../virtualiseur/src/storage.c
    ../../../virtualiseur/src/chunk.c)

target_link_libraries(ut_lay_sstriping_superblock
    fake_rdev
    fake_storage
    fake_assembly_group
    memory_stream
    stat_stream
    assembly
    spof_group
    exalogclientfake
    exa_common_user
    exa_os
    # FIXME - THIS IS CRAP
    blockdevice)
