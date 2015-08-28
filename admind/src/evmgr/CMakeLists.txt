#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

# --- libexa_evmgr --------------------------------------------------
# libexa_evmgr contains all functions related to event handling of
# admind. It also contain functions used to call different services.

add_library(exa_evmgr STATIC
    evmgr.c
    evmgr_mship.c)

target_link_libraries(exa_evmgr
    tm_client)

if (WITH_MONITORING)
    target_link_libraries(exa_evmgr md_client)
endif (WITH_MONITORING)

