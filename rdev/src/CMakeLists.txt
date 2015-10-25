#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (NOT WIN32)
    if (WITH_PERF)
        set(RDEV_PERF_SOURCES rdev_perf.c)
        set(LIBPERF exa_perf_instance exaperf)
    endif (WITH_PERF)

    if (WITH_LIBAIO)
        add_library(rdev STATIC
            rdev_common.c
            rdev_libaio_linux.c
            ${RDEV_PERF_SOURCES})

        target_link_libraries(rdev exa_os ${LIBPERF} aio)
    else (WITH_LIBAIO)
        add_library(rdev STATIC
            rdev_common.c
            rdev_linux.c
            ${RDEV_PERF_SOURCES})

        target_link_libraries(rdev exa_os ${LIBPERF})

        include(KernelModule)

        add_kernel_module(exa_rdev
            rdev_kmodule.c
            rdev_kmodule.h
            ${CMAKE_SOURCE_DIR}/common/include/exa_assert.h
            ${CMAKE_SOURCE_DIR}/common/include/exa_constants.h
            ${CMAKE_SOURCE_DIR}/common/include/exa_error.h
            ${CMAKE_SOURCE_DIR}/common/include/exa_math.h
            ${CMAKE_SOURCE_DIR}/common/include/exa_names.h
            ${CMAKE_SOURCE_DIR}/rdev/include/exa_rdev.h
            ${CMAKE_SOURCE_DIR}/os/include/os_assert.h
            ${CMAKE_SOURCE_DIR}/os/include/os_inttypes.h)
    endif (WITH_LIBAIO)

else (NOT WIN32)

    add_library(rdev STATIC
                rdev_common.c
                rdev_windows.c)
    target_link_libraries(rdev exa_os)

endif (NOT WIN32)

