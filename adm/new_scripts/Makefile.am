
## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	$(WARN_CFLAGS) 					\
	-DG_LOG_DOMAIN=\"DriveFUSION\"			\
	-DVERSION=\"$(VERSION)\"			\
	-Wall -D_LARGEFILE64_SOURCE -g -O

bin_PROGRAMS = devinfo

devinfo_SOURCES = \
	devinfo.c

devinfo_LDADD = \
	-lm

EXTRA_DIST = \
	commun.pm	\
	constantes.pm	\
	dgcreate.pl	\
	dgkill.pl	\
	dgstart.pl	\
	startFUSION.pl	\
	stopFUSION.pl	\
	test.pl		\
	vcreate.pl
