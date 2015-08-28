#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

add_unit_test(ut_spof
	../src/spof.c)

target_link_libraries(ut_spof
    vrt_common)

add_unit_test(ut_vrt_stream
    ../src/vrt_stream.c)

target_link_libraries(ut_vrt_stream
    exa_os
    memory_stream)

add_unit_test(ut_memory_stream
    ../src/memory_stream.c)

target_link_libraries(ut_memory_stream
    vrt_stream)

add_unit_test(ut_file_stream
    ../src/file_stream.c)

target_link_libraries(ut_file_stream
    vrt_stream
    exa_os)

add_unit_test(ut_tee_stream
    ../src/tee_stream.c)

target_link_libraries(ut_tee_stream
    memory_stream
    vrt_stream)

add_unit_test(ut_stat_stream
    ../src/stat_stream.c)

target_link_libraries(ut_stat_stream
    memory_stream
    vrt_stream)

add_unit_test(ut_checksum_stream
    ../src/checksum_stream.c)

target_link_libraries(ut_checksum_stream
    checksum_stream
    memory_stream)

add_unit_test(ut_narrowed_stream
    ../src/narrowed_stream.c)

target_link_libraries(ut_narrowed_stream
    memory_stream
    vrt_stream)

