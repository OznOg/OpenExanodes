#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(blockdevice STATIC
    blockdevice.c)

target_link_libraries(blockdevice
    exa_os)

add_library(sys_blockdevice STATIC
    sys_blockdevice.c)

target_link_libraries(sys_blockdevice
    blockdevice)

add_library(blockdevice_stream
    blockdevice_stream.c)

target_link_libraries(blockdevice_stream
    vrt_stream)

