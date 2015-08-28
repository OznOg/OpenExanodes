#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_PERF)
    set(TARGET_PERF bd_user_target_perf.c)
    set(LIBPERF exaperf)
endif (WITH_PERF)

add_library(linux_bd_target STATIC
    ${TARGET_PERF}
    bd_user_target.c)
    
target_link_libraries(linux_bd_target
    ${LIBPERF})
