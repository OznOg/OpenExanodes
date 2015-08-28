#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

all:
	$(MAKE) -C $(LINUX_DIR) M="`pwd`/common/lib" V=1 modules
	./copy_symvers common/lib nbd/exa_rdev/src/
	$(MAKE) -C $(LINUX_DIR) M="`pwd`/nbd/exa_rdev/src/" V=1 modules
	./copy_symvers nbd/exa_rdev target/linux_bd_target/module
	$(MAKE) -C $(LINUX_DIR) M="`pwd`/target/linux_bd_target/module" V=1 modules

clean:
	$(MAKE) -C $(LINUX_DIR) M="`pwd`/common/lib" clean
	$(MAKE) -C $(LINUX_DIR) M="`pwd`/nbd/exa_rdev/src/" clean
	$(MAKE) -C $(LINUX_DIR) M="`pwd`/target/linux_bd_target/module" clean
