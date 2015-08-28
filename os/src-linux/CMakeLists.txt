#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_MEMTRACE)
    set(MEMTRACE_SOURCES os_memtrace.c)
endif (WITH_MEMTRACE)

add_library(exa_os STATIC
    os_atomic.c
    os_dir.c
    os_disk.c
    os_error.c
    os_file.c
    os_kmod.c
    os_ipc_sem.c
    os_mem.c
    os_filemap.c
    ${MEMTRACE_SOURCES}
    os_network.c
    os_process.c
    os_random.c
    os_semaphore.c
    os_shm.c
    os_string.c
    os_syslog.c
    os_system.c
    os_thread.c
    os_time.c
    os_stdio.c
    os_user.c
    os_daemon_parent.c
    os_daemon_lock.c)

set_target_properties(exa_os PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(exa_os exa_os_common ${LIBPTHREAD} ${LIBRT} ${LIBDL})

add_library(os_daemon_child STATIC
    os_daemon_child.c)
target_link_libraries(os_daemon_child exa_os)

add_library(os_daemon_parent STATIC
    os_daemon_parent.c)

