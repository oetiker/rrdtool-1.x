## Process this file with automake to produce Makefile.in

#AUTOMAKE_OPTIONS   = foreign
#
#ACLOCAL_M4	   = $(top_srcdir)/config/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config

if STATIC_PROGRAMS
AM_LDFLAGS = -all-static
endif

INCLUDES = -DLOCALEDIR="\"$(datadir)/locale\""
RRD_DEFAULT_FONT=@RRD_DEFAULT_FONT@
AM_CPPFLAGS = -DRRD_DEFAULT_FONT=\"$(RRD_DEFAULT_FONT)\" -DNUMVERS=@NUMVERS@

UPD_C_FILES =		\
	rrd_parsetime.c	\
	rrd_hw.c	\
	rrd_hw_math.c	\
	rrd_hw_update.c	\
	rrd_diff.c	\
	rrd_format.c	\
	rrd_info.c	\
	rrd_error.c	\
	rrd_open.c	\
	rrd_nan_inf.c	\
	rrd_rpncalc.c	\
	rrd_update.c

RRD_C_FILES =		\
	hash_32.c	\
	pngsize.c	\
	rrd_create.c	\
	rrd_graph.c	\
	rrd_graph_helper.c	\
	rrd_version.c	\
	rrd_last.c	\
	rrd_lastupdate.c	\
	rrd_first.c	\
	rrd_restore.c	\
	rrd_xport.c	\
	rrd_gfx.c \
	rrd_dump.c	\
	rrd_fetch.c	\
	rrd_resize.c \
	rrd_tune.c

noinst_HEADERS = \
	unused.h \
	rrd_getopt.h rrd_parsetime.h \
	rrd_config_bottom.h rrd_i18n.h \
	rrd_format.h rrd_tool.h rrd_xport.h rrd.h rrd_rpncalc.h \
	rrd_hw.h rrd_hw_math.h rrd_hw_update.h \
	fnv.h rrd_graph.h \
	rrd_is_thread_safe.h

if BUILD_GETOPT
noinst_HEADERS += rrd_getopt.h
UPD_C_FILES += rrd_getopt.c rrd_getopt1.c
endif

noinst_LTLIBRARIES        = librrdupd.la

lib_LTLIBRARIES           = librrd.la 
if BUILD_MULTITHREAD
lib_LTLIBRARIES           += librrd_th.la
endif

librrdupd_la_SOURCES      = $(UPD_C_FILES) rrd_not_thread_safe.c
librrdupd_la_LIBADD       = $(CORE_LIBS) @LIB_LIBINTL@

librrd_la_SOURCES         = $(RRD_C_FILES)
librrd_la_DEPENDENCIES    = librrdupd.la librrd.sym
librrd_la_LIBADD          = librrdupd.la $(ALL_LIBS)
librrd_la_LDFLAGS         = -version-info @LIBVERS@
librrd_la_LDFLAGS         += -export-symbols librrd.sym

librrd_th_la_SOURCES         = $(UPD_C_FILES) $(RRD_C_FILES) rrd_thread_safe.c
librrd_th_la_DEPENDENCIES    = librrd.sym
librrd_th_la_CFLAGS          = $(MULTITHREAD_CFLAGS)
librrd_th_la_LDFLAGS         = $(MULTITHREAD_LDFLAGS) -version-info @LIBVERS@
librrd_th_la_LDFLAGS         += -export-symbols librrd.sym
librrd_th_la_LIBADD          = $(ALL_LIBS)

include_HEADERS	= rrd.h rrd_format.h

bin_PROGRAMS	= rrdtool rrdupdate

if BUILD_RRDCGI
bin_PROGRAMS += rrdcgi
endif

rrdcgi_SOURCES	= rrd_cgi.c
rrdcgi_LDADD	= librrd.la

rrdupdate_SOURCES = rrdupdate.c
rrdupdate_LDADD	  = librrdupd.la

rrdtool_SOURCES = rrd_tool.c
rrdtool_DEPENDENCIES = librrd.la
rrdtool_LDADD	= librrd.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = librrd.pc

# strftime is here because we do not usually need it. unices have propper
# iso date support
EXTRA_DIST= librrd.pc.in strftime.c strftime.h  rrd_getopt.c rrd_getopt1.c rrd_getopt.h \
	win32comp.c  rrd_thread_safe_nt.c get_ver.awk librrd.sym.in.in plbasename.c   plbasename.h   

librrd.sym: librrd.sym.in
	grep -v LIBC_HAS_GETOPT_LONG librrd.sym.in >$@
