## Process this file with automake to produce Makefile.in

#AUTOMAKE_OPTIONS   = foreign
#
#ACLOCAL_M4	   = $(top_srcdir)/config/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config

CGI_LIB_DIR	   = $(top_srcdir)/@CGI_LIB_DIR@
GD_LIB_DIR	   = $(top_srcdir)/@GD_LIB_DIR@
PNG_LIB_DIR	   = $(top_srcdir)/@PNG_LIB_DIR@
ZLIB_LIB_DIR	   = $(top_srcdir)/@ZLIB_LIB_DIR@

INCLUDES 	   = -I$(CGI_LIB_DIR) -I$(GD_LIB_DIR) -I$(PNG_LIB_DIR) -I$(ZLIB_LIB_DIR)

#COMPILE   = $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) $(CFLAGS_EXTRA)
#LTCOMPILE = $(LIBTOOL) --mode=compile $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) $(CFLAGS_EXTRA)
#LINK      = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(CFLAGS_EXTRA) $(LDFLAGS) -o $@

RRD_C_FILES =		\
	gdpng.c		\
	getopt.c	\
	getopt1.c	\
	gifsize.c	\
	parsetime.c	\
	hash_32.c	\
	rrd_hw.c	\
	pngsize.c	\
	rrd_create.c	\
	rrd_diff.c	\
	rrd_dump.c	\
	rrd_info.c	\
	rrd_error.c	\
	rrd_fetch.c	\
	rrd_format.c	\
	rrd_graph.c	\
	rrd_graph_helper.c	\
	rrd_last.c	\
	rrd_open.c	\
	rrd_resize.c	\
	rrd_restore.c	\
	rrd_rpncalc.c \
	rrd_tune.c	\
	rrd_update.c	\
	getopt.h ntconfig.h parsetime.h rrd_format.h rrd_tool.h rrd.h rrd_hw.h rrd_rpncalc.h

# Build two libraries.  One is a public one that gets installed in
# $prefix/lib.  Libtool does not create an archive of the PIC compiled
# objects for this library type.  The second library is a private one
# meant to build the RRDs.so for perl-shared.  In this case libtool
# creates a ./.lib/*.al file that contains the PIC compiled object
# files.

RRD_LIBS =				\
	$(CGI_LIB_DIR)/librrd_cgi.la	\
	$(GD_LIB_DIR)/librrd_gd.la	\
	$(PNG_LIB_DIR)/librrd_png.la	\
	$(ZLIB_LIB_DIR)/librrd_z.la

lib_LTLIBRARIES           = librrd.la
noinst_LTLIBRARIES        = librrd_private.la

librrd_la_SOURCES         = $(RRD_C_FILES)
librrd_private_la_SOURCES = $(RRD_C_FILES)

librrd_la_LIBADD          = $(RRD_LIBS)
librrd_la_LDFLAGS         = -version-info 0:0:0

include_HEADERS	= rrd.h

librrd_private_la_LIBADD  = $(RRD_LIBS)
librrd_private_la_LDFLAGS = -static

bin_PROGRAMS	= rrdcgi rrdtool rrdupdate

rrdcgi_SOURCES	= rrd_cgi.c
rrdcgi_LDADD	= librrd.la

rrdupdate_SOURCES = 
rrdupdate_LDADD	= rrdupdate.o librrd.la


rrdupdate.c: rrd_update.c
	-ln -s rrd_update.c rrdupdate.c

rrdupdate.o: rrdupdate.c
	$(COMPILE) -DSTANDALONE -c rrdupdate.c

rrdtool_SOURCES	= rrd_tool.c
rrdtool_LDADD	= librrd.la

EXTRA_DIST= rrdtool.dsp rrdtool.dsw
