
## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	$(WARN_CFLAGS) 					\
	-DG_LOG_DOMAIN=\"DriveFUSION\"			\
	-DVERSION=\"$(VERSION)\"			


bin_PROGRAMS = checkdev

checkdev_SOURCES = \
	checkdev.c

