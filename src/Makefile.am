## Process this file with automake to produce Makefile.in

#AUTOMAKE_OPTIONS   = foreign
#
#ACLOCAL_M4	   = $(top_srcdir)/config/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config

if USE_INSTALLED_libcgi
CGI_LIB 	   = -lcgi
else
CGI_LIB	           = $(top_srcdir)/libraries/@CGI_LIB_DIR@/librrd_cgi.la
CGI_INCLUDES       = -I$(top_srcdir)/libraries/@CGI_LIB_DIR@
endif
if USE_INSTALLED_libfreetype
FREETYPE_LIB       = -lfreetype
else
FREETYPE_LIB       = $(top_srcdir)/libraries/@FREETYPE_LIB_DIR@/librrd_freetype.la
FREETYPE_INCLUDES  = -I$(top_srcdir)/libraries/@FREETYPE_LIB_DIR@/include
endif
if USE_INSTALLED_libart_lgpl
ART_LIB            = -lart_lgpl
else
ART_LIB 	   = $(top_srcdir)/libraries/@ART_LIB_DIR@/librrd_art.la
ART_INCLUDES       = -I$(top_srcdir)/libraries/@ART_LIB_DIR@
endif
if USE_INSTALLED_libpng
PNG_LIB            = -lpng
else
PNG_LIB		   = $(top_srcdir)/libraries/@PNG_LIB_DIR@/librrd_png.la
PNG_INCLUDES       = -I$(top_srcdir)/libraries/@PNG_LIB_DIR@
endif
if USE_INSTALLED_libz
ZLIB_LIB           = -lz
else
ZLIB_LIB           = $(top_srcdir)/libraries/@ZLIB_LIB_DIR@/librrd_z.la
ZLIB_INCLUDES      = -I$(top_srcdir)/libraries/@ZLIB_LIB_DIR@
endif

INCLUDES = $(CGI_INCLUDES) $(FREETYPE_INCLUDES) $(ART_INCLUDES) \
           $(PNG_INCLUDES) $(ZLIB_INCLUDES)

RRD_C_FILES =		\
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
	rrd_gfx.c rrd_gfx.h \
	getopt.h ntconfig.h parsetime.h rrd_format.h rrd_tool.h rrd.h rrd_hw.h rrd_rpncalc.h

# Build two libraries.  One is a public one that gets installed in
# $prefix/lib.  Libtool does not create an archive of the PIC compiled
# objects for this library type.  The second library is a private one
# meant to build the RRDs.so for perl-shared.  In this case libtool
# creates a ./.lib/*.al file that contains the PIC compiled object
# files.

RRD_LIBS =				\
	$(CGI_LIB)	\
	$(FREETYPE_LIB)	\
	$(ART_LIB)	\
	$(PNG_LIB)	\
	$(ZLIB_LIB)

lib_LTLIBRARIES           = librrd.la
noinst_LTLIBRARIES        = librrd_private.la

librrd_la_SOURCES         = $(RRD_C_FILES)
librrd_private_la_SOURCES = $(RRD_C_FILES)

librrd_la_LIBADD          = $(RRD_LIBS)
librrd_la_LDFLAGS         = -version-info 1:2:0

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
