## Process this file with automake to produce Makefile.in

libexec_PROGRAMS = dfvirtual.o

libexecdir = ${prefix}/../$(MODULE_INSTALLDIR)

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(KERNEL_CFLAGS)

dfvirtual_o_SOURCES = \
	constantes.h \
	parser.c \
	parser.h \
	virtualiseur.h \
	vrt_clean.c \
	vrt_divers.c \
	vrt_divers.h \
	vrt_group.c \
	vrt_group.h \
	vrt_init.c  \
	vrt_layout.c \
	vrt_layout.h \
	vrt_ops.c \
	vrt_ops.h \
	vrt_proc.c \
	vrt_proc.h \
	vrt_rdev.c \
	vrt_rdev.h \
	vrt_request.c \
	vrt_request.h \
	vrt_superblock.c \
	vrt_superblock.h \
	vrt_zone.c \
	vrt_zone.h


dfvirtual.o$(EXEEXT): $(dfvirtual_o_OBJECTS)
	ld -r -o $@ $(dfvirtual_o_OBJECTS)

CLEANFILES = core *.core *~ dfvirtual 

