#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

# FIXME Make extent a separate library

add_library(assembly STATIC
    assembly_group.c
    assembly_volume.c
    assembly_slot.c
    extent.c)

target_link_libraries(assembly
    assembly_prediction
    vrt_common
    vrt_stream)

add_library(assembly_prediction STATIC
    assembly_prediction.c)
