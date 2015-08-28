#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

# Always go inside the ISCSI directory, since some of its libs
# are required by Admind no matter what.
add_subdirectory(iscsi)

if (WITH_BDEV)
    add_subdirectory(linux_bd_target)
endif (WITH_BDEV)
