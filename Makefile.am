## Process this file with automake to produce Makefile.in

SUBDIRS = po adm vrt sciNBD tools

EXTRA_DIST = \
	drivefusion.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in



distcore = $(PACKAGE)-core-$(VERSION)


CLEANFILES = drivefusion.spec


ACLOCAL_AMFLAGS = 

DIST_COMMON = ABOUT

.PHONY: doxygen
doxygen: Doxyfile
	rm -rf doxygen
	mkdir -p doxygen
	doxygen
