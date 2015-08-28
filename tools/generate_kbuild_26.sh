#!/bin/bash

#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

abs_top_srcdir=$1
abs_srcdir=$2
abs_top_builddir=$3
CFLAGS=$4
MODULE_NAME=$5
MODULE_SRC=`basename $6`
shift 6
for file in $*; do
  MODULE_SRC="$MODULE_SRC `basename $file`"
done

echo "# automagically generated Kbuild makefile for linux 2.6.x"
echo "EXTRA_CFLAGS := -I$abs_top_srcdir -I$abs_srcdir -I$abs_top_builddir $CFLAGS"
echo "obj-m := $MODULE_NAME.o"
if [ "$MODULE_NAME.c" != "$MODULE_SRC" ]; then
  echo "$MODULE_NAME-y := $MODULE_SRC" | sed 's/\.c/.o/g'
fi
