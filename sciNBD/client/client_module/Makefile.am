## Process this file with automake to produce Makefile.in

libexec_PROGRAMS = scimapdev.o

libexecdir = ${prefix}/../$(MODULE_INSTALLDIR)

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(KERNEL_CFLAGS) \
	-D_KERNEL-DEXPORT_SYMTAB -DLinux \
	$(SCIIRM_CFLAGS)

scimapdev_o_SOURCES = \
	constantes.h \
	parser.c \
	parser.h \
	scimapdev.h \
	smd_clean.c \
	smd_divers.c \
	smd_divers.h \
	smd_init.c \
	smd_ops.c \
	smd_ops.h \
	smd_proc.c \
	smd_proc.h \
	smd_request.c \
	smd_request.h \
	smd_sci.c \
	smd_snode.c \
	smd_snode.h


scimapdev.o$(EXEEXT): $(scimapdev_o_OBJECTS)
	ld -r -o $@ $(scimapdev_o_OBJECTS)

CLEANFILES = core *.core *~ scimapdev



