
## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	$(WARN_CFLAGS) 					\
	-DG_LOG_DOMAIN=\"DriveFUSION\"			\
	-DVERSION=\"$(VERSION)\"			\
	$(SCI_CFLAGS)

bin_PROGRAMS = smd_importe

smd_importe_SOURCES = \
	constantes.h parser.h smd_importe.c parser.c

smd_importe_LDADD = \
	$(SCI_LIBS)

