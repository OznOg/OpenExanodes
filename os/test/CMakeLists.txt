#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

link_directories(${CMAKE_SOURCE_DIR}/os/${OS_SUBDIR})


add_unit_test(ut_os_daemon_parent)
target_link_libraries(ut_os_daemon_parent exa_os os_daemon_parent)
add_dependencies(ut_os_daemon_parent daemon_test_child)

add_executable(daemon_test_child ut_os_daemon_parent.c)
target_link_libraries(daemon_test_child os_daemon_child exa_os)
set_target_properties(daemon_test_child PROPERTIES COMPILE_FLAGS "-DCHILD")

add_unit_test(ut_os_inttypes)
target_link_libraries(ut_os_inttypes exa_os)

add_unit_test(ut_os_stdio)
target_link_libraries(ut_os_stdio exa_os)

add_unit_test(ut_os_atomic)
target_link_libraries(ut_os_atomic exa_os)

add_unit_test(ut_os_dir)
target_link_libraries(ut_os_dir exa_os)

add_unit_test(ut_os_error)
target_link_libraries(ut_os_error exa_os)

add_unit_test(ut_os_ipc_sem)
target_link_libraries(ut_os_ipc_sem exa_os ${LIBPTHREAD} ${LIBRT})

add_unit_test(ut_os_getopt)
target_link_libraries(ut_os_getopt exa_os)
if (WIN32)
    # Suppress warning "statement is unreachable"
    set_target_properties(ut_os_getopt PROPERTIES COMPILE_FLAGS "/Qwd111")
endif (WIN32)

add_unit_test(ut_os_time)
target_link_libraries(ut_os_time exa_os ${LIBRT})

add_unit_test(ut_os_network)
target_link_libraries(ut_os_network exa_os)

add_unit_test(ut_os_process)
target_link_libraries(ut_os_process exa_os)

# This test fails when /dev/shm is not mounted.
if (WITH_UT_ROOT)
    add_unit_test(ut_os_shm)
    target_link_libraries(ut_os_shm exa_os ${LIBRT})
endif (WITH_UT_ROOT)

add_unit_test(ut_os_mem)
target_link_libraries(ut_os_mem exa_os)

add_unit_test(ut_os_filemap)
target_link_libraries(ut_os_filemap exa_os)

add_unit_test(ut_os_file)
target_link_libraries(ut_os_file exa_os)

# When not root this test fails to open a disk.
if (WITH_UT_ROOT)
    add_unit_test(ut_os_disk)
    target_link_libraries(ut_os_disk exa_os)
endif (WITH_UT_ROOT)

add_unit_test(ut_os_random)
target_link_libraries(ut_os_random exa_os)

add_unit_test(ut_os_semaphore)
target_link_libraries(ut_os_semaphore exa_os ${LIBPTHREAD} ${LIBRT})

add_unit_test(ut_os_string)
target_link_libraries(ut_os_string exa_os)

# When not root this test may fail to open the log file.
if (WITH_UT_ROOT)
    add_unit_test(ut_os_syslog)
    target_link_libraries(ut_os_syslog exa_os ${LIBRT})
endif (WITH_UT_ROOT)

# FIXME Missing dependency on os_system_helper!
add_unit_test(ut_os_system)
target_link_libraries(ut_os_system exa_os)

add_executable(os_system_helper os_system_helper.c)
target_link_libraries(os_system_helper exa_os)

add_unit_test(ut_os_thread)
target_link_libraries(ut_os_thread exa_os ${LIBPTHREAD} ${LIBRT})

add_unit_test(ut_os_daemon_lock)
target_link_libraries(ut_os_daemon_lock exa_os)

