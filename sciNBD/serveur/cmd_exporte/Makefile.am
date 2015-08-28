
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

bin_PROGRAMS = smd_exporte

smd_exporte_SOURCES = \
	smd_exporte.c

smd_exporte_LDADD = \
	$(SCI_LIBS)

