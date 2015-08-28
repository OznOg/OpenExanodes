#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#


if (NOT WITH_BDEV)
    set(LIBTARGET iscsi_target)
else (NOT WITH_BDEV)
    set(LIBTARGET iscsi_target linux_bd_target)
endif (NOT WITH_BDEV)


add_library(executive_export STATIC
    executive_export.c
    export_io.c
    ${BLOCKDEVICE_READAHEAD})

target_link_libraries(executive_export
    ${LIBTARGET}
    exa_export)

add_library(exa_export STATIC
    export.c
    ../include/export.h)

target_link_libraries(exa_export
    iqn
    iqn_filter)


