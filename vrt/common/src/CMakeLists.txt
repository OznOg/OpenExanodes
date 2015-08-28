#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(vrt_common STATIC
    spof.c)

target_link_libraries(vrt_common
    exa_common_user)

add_library(vrt_stream
    vrt_stream.c)

target_link_libraries(vrt_stream
    exa_os)

add_library(memory_stream
    memory_stream.c)

target_link_libraries(memory_stream
    vrt_stream)

add_library(checksum_stream
    checksum_stream.c)

target_link_libraries(checksum_stream
    exa_common_user
    vrt_stream)

add_library(file_stream
    file_stream.c)

target_link_libraries(file_stream
    vrt_stream
    exa_os)

add_library(tee_stream
    tee_stream.c)

target_link_libraries(tee_stream
    vrt_stream)

add_library(stat_stream
    stat_stream.c)

target_link_libraries(stat_stream
    vrt_stream)

add_library(null_stream
    null_stream.c)

add_library(narrowed_stream
    narrowed_stream.c)

