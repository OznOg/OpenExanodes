
## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	-I$(KERNELDIR)/include				\
	$(WARN_CFLAGS) 					\
	-DG_LOG_DOMAIN=\"DriveFUSION\"			\
	-DVERSION=\"$(VERSION)\"			\
	$(SCI_CFLAGS)

bin_PROGRAMS = smd_exportd

smd_exportd_SOURCES = \
	cotypes.h \
	parser.c \
	parser.h \
	smd_exportd.c \
	table.c \
	table.h

smd_exportd_LDADD = \
	$(SCI_LIBS)

