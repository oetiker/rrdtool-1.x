## Process this file with automake to produce Makefile.in

#AUTOMAKE_OPTIONS   = foreign
#
#ACLOCAL_M4	   = $(top_srcdir)/config/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config
DEFS += -DMAKE_TIMESTAMP=\""$(shell date)"\"
fontsdir =  $(datadir)/rrdtool/fonts
fonts_DATA = VeraMono.ttf

#INCLUDES = $(CGI_INCLUDES) $(FREETYPE_INCLUDES) $(ART_INCLUDES) \
#           $(PNG_INCLUDES) $(ZLIB_INCLUDES)
AM_CPPFLAGS = -DRRD_DEFAULT_FONT=\"$(fontsdir)/$(fonts_DATA)\"

RRD_C_FILES =		\
	getopt.c	\
	getopt1.c	\
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
	rrd_xport.c	\
	rrd_nan_inf.c	\
	rrd_gfx.c rrd_gfx.h \
	rrd_afm.c rrd_afm_data.c \
	getopt.h ntconfig.h parsetime.h \
	rrd_format.h rrd_tool.h rrd_xport.h rrd.h rrd_hw.h rrd_rpncalc.h \
	rrd_tool.c \
	rrd_nan_inf.h fnv.h rrd_graph.h rrd_afm.h rrd_afm_data.h \
	rrd_is_thread_safe.h

lib_LTLIBRARIES           = librrd.la 
if BUILD_MULTITHREAD
lib_LTLIBRARIES           += librrd_th.la
endif

#noinst_LTLIBRARIES        = librrd_private.la

librrd_la_SOURCES         = $(RRD_C_FILES) rrd_not_thread_safe.c
#librrd_private_la_SOURCES = $(RRD_C_FILES) rrd_not_thread_safe.c

# librrd_la_LIBADD          = $(RRD_LIBS)
# see http://www.gnu.org/software/libtool/manual.html#SEC32 for explanation
librrd_la_LDFLAGS         = -version-info 1:0:0

librrd_th_la_SOURCES         = $(RRD_C_FILES) rrd_thread_safe.c
librrd_th_la_CFLAGS          = $(MULTITHREAD_CFLAGS)
librrd_th_la_LDFLAGS         = $(MULTITHREAD_LDFLAGS) -version-info 1:0:0


include_HEADERS	= rrd.h

#librrd_private_la_LIBADD  = $(RRD_LIBS)
#librrd_private_la_LDFLAGS = -static

bin_PROGRAMS	= rrdtool rrdupdate
if BUILD_RRDCGI
bin_PROGRAMS += rrdcgi
endif

rrdcgi_SOURCES	= rrd_cgi.c
rrdcgi_LDADD	= librrd.la

rrdupdate_SOURCES = 
rrdupdate_LDADD	= rrdupdate.o librrd.la

# rrdupdate.c: rrd_update.c
#	-ln -s ${srcdir}/rrdupdate.c rrd_update.c

rrdupdate.o: rrd_update.c
	$(COMPILE) -DSTANDALONE -c -o $@ $^

rrdtool_SOURCES = 
rrdtool_DEPENDENCIES = rrd_tool.o
rrdtool_LDADD	= librrd.la

EXTRA_DIST= rrdtool.dsp rrdtool.dsw $(fonts_DATA)
