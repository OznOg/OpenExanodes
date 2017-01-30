#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(KernelModule)

add_kernel_module(exa_bd
    bd_log.h
    bd_init.c
    bd_list.c
    bd_list.h
    bd_user_kernel.c
    bd_user_kernel.h
    bd_user_bd.c
    bd_user_bd.h
    bd_user_fops.c
    bd_user_fops.h
    ${CMAKE_SOURCE_DIR}/target/linux_bd_target/include/bd_user.h
    ${CMAKE_SOURCE_DIR}/common/include/exa_assert.h
    ${CMAKE_SOURCE_DIR}/common/include/exa_constants.h
    ${CMAKE_SOURCE_DIR}/common/include/exa_error.h
    ${CMAKE_SOURCE_DIR}/common/include/exa_math.h
    ${CMAKE_SOURCE_DIR}/common/include/exa_names.h
    ${CMAKE_SOURCE_DIR}/os/include/os_assert.h
    ${CMAKE_SOURCE_DIR}/os/include/os_inttypes.h)
