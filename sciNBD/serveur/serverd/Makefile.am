
## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir) 				\
	-I$(top_builddir)				\
	-I$(KERNELDIR)/include				\
	$(WARN_CFLAGS) 					\
	-DG_LOG_DOMAIN=\"DriveFUSION\"			\
	-DVERSION=\"$(VERSION)\"			\
	-D_XOPEN_SOURCE=500				\
	-D_REENTRANT -D_LARGEFILE64_SOURCE		\
	$(SCI_CFLAGS)

bin_PROGRAMS = smd_serverd

smd_serverd_SOURCES = \
	smd_serverd.c \
	smd_svd2exd.c \
	smdsv_client.c \
	smdsv_divers.c \
	smdsv_sci.c \
	smdsv_traiterio.c \
	smdsv_connex.c \
	constantes.h \
	smd_server.h \
	smdsv_client.h \
	smdsv_divers.h \
	smdsv_sci.h

smd_serverd_LDADD = \
	$(SCI_LIBS) $(PTHREAD_LIBS)

