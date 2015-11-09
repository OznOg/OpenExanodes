#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#


# --- function to declare a kernel module ----------------------------

# usage :
# include(${CMAKE_SOURCE_DIR}/KernelModule.cmake)
# add_kernel_module(name source1 source2...)
# FIXME WIN32: handle Module.symvers (see Makefile.modules.am)

function(add_kernel_module MODULE_NAME)

# Get all parameters except the first one
foreach (SRC ${ARGN})
    if ("${SRC}" MATCHES "\\.c$" OR "${SRC}" MATCHES "\\.cpp$")
        set(MODULE_SRC ${MODULE_SRC} ${SRC})
    elseif ("${SRC}" MATCHES "\\.h$" OR "${SRC}" MATCHES "\\.hpp$")
        set(MODULE_HEAD ${MODULE_HEAD} ${SRC})
    else ("${SRC}" MATCHES "\\.c$" OR "${SRC}" MATCHES "\\.cpp$")
        message(send_error "Unknown source type: ${SRC}")
    endif ("${SRC}" MATCHES "\\.c$" OR "${SRC}" MATCHES "\\.cpp$")
endforeach (SRC ${ARGN})

# Get source file names relative to current build directory
foreach (SRC ${MODULE_SRC})
    set(ABS_MODULE_SRC ${ABS_MODULE_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/${SRC})
endforeach (SRC ${MODULE_SRC})

if (WITH_DKMS)

    file(RELATIVE_PATH DKMS_SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    set(DKMS_CURRENT_DIR ${DKMS_DIR}/${DKMS_SUBDIR})

    add_custom_target(Makefile.${MODULE_NAME} ALL
        COMMAND /bin/bash ${CMAKE_SOURCE_DIR}/tools/generate_kbuild_26.sh
                ${DKMS_DIR}
                ${DKMS_CURRENT_DIR}
                ${DKMS_DIR}
		'${KERNEL_CFLAGS}'
                ${MODULE_NAME}
                ${MODULE_SRC}
                > Makefile.${MODULE_NAME}
        COMMENT "Generate kernel Makefile for ${MODULE_NAME}")

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Makefile.${MODULE_NAME}
        DESTINATION ${DKMS_CURRENT_DIR}
        RENAME Makefile)

    install(FILES ${MODULE_SRC}
        DESTINATION ${DKMS_CURRENT_DIR})

    foreach(HEADER ${MODULE_HEAD})
        if (NOT ${HEADER} MATCHES "^/")
            set(HEADER ${CMAKE_CURRENT_SOURCE_DIR}/${HEADER})
        endif (NOT ${HEADER} MATCHES "^/")
        if (${HEADER} MATCHES "^${CMAKE_BINARY_DIR}")
            file(RELATIVE_PATH HEADER_SUBDIR ${CMAKE_BINARY_DIR} ${HEADER})
        else (${HEADER} MATCHES "^${CMAKE_BINARY_DIR}")
            file(RELATIVE_PATH HEADER_SUBDIR ${CMAKE_SOURCE_DIR} ${HEADER})
        endif (${HEADER} MATCHES "^${CMAKE_BINARY_DIR}")
        string(REGEX REPLACE "\(.*\)/.*$" "\\1" HEADER_SUBDIR ${HEADER_SUBDIR})
        install(FILES ${HEADER} DESTINATION ${DKMS_DIR}/${HEADER_SUBDIR})
    endforeach(HEADER ${MODULE_HEAD})

else (WITH_DKMS)

    add_custom_target(${MODULE_NAME}.ko ALL
        COMMAND mkdir -p kmod/
        COMMAND /bin/bash ${CMAKE_SOURCE_DIR}/tools/generate_kbuild_26.sh
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_BINARY_DIR}
            '${KERNEL_CFLAGS}'
            ${MODULE_NAME}
            ${MODULE_SRC}
            > kmod/Makefile
        COMMAND ln -sf ${ABS_MODULE_SRC} kmod/
        COMMAND make -C ${LINUX_DIR} modules
                CC="${CMAKE_C_COMPILER}"
                M=${CMAKE_CURRENT_BINARY_DIR}/kmod/
                V=${CMAKE_VERBOSE_MAKEFILE}
        COMMENT "Building Linux module ${MODULE_NAME}.ko")

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kmod/${MODULE_NAME}.ko
        DESTINATION /lib/modules/${LINUX_VERSION}/kernel/exanodes)

endif (WITH_DKMS)

endfunction(add_kernel_module)
