
## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	$(WARN_CFLAGS) 					\
	-DG_LOG_DOMAIN=\"DriveFUSION\"			\
	-DVERSION=\"$(VERSION)\"			\
	-Wall -D_LARGEFILE64_SOURCE

EXTRA_DIST = test.sh

bin_PROGRAMS = dgcreate zcreate dgstart zstart zstop zdelete dgstop dgdelete zresize vstatus

general_SOURCES = \
	dataaccess.c dataaccess.h uuid.c validation.c parser.c conffile.c \
	conffile.h constantes.h i18n.h parser.h uuid.h validation.h \
	vrt_superblock.h vrtcommands.h 

vstatus_SOURCES = \
	vstatus.c \
	$(general_SOURCES)

zcreate_SOURCES = \
	zcreate.c \
	$(general_SOURCES)

zresize_SOURCES = \
	zresize.c \
	$(general_SOURCES)

zstart_SOURCES = \
	zstart.c \
	$(general_SOURCES)

zstop_SOURCES = \
	zstop.c \
	$(general_SOURCES)

zdelete_SOURCES = \
	zdelete.c \
	$(general_SOURCES)

dgcreate_SOURCES = \
	dgcreate.c \
	$(general_SOURCES)

dgdelete_SOURCES = \
	dgdelete.c \
	$(general_SOURCES)

dgstart_SOURCES = \
	dgstart.c \
	$(general_SOURCES)

dgstop_SOURCES = \
	dgstop.c \
	$(general_SOURCES)

