## Process this file with automake to produce Makefile.in

#AUTOMAKE_OPTIONS   = foreign
#
#ACLOCAL_M4	   = $(top_srcdir)/config/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config

if STATIC_PROGRAMS
AM_LDFLAGS = -all-static
endif

RRD_DEFAULT_FONT=@RRD_DEFAULT_FONT@
AM_CPPFLAGS = -DLOCALEDIR="\"$(datadir)/locale\"" \
	-DRRD_DEFAULT_FONT=\"$(RRD_DEFAULT_FONT)\" \
	-DNUMVERS=@NUMVERS@
AM_CFLAGS = @CFLAGS@ -I$(top_srcdir)
## no including this by default @WERROR@

UPD_C_FILES =		\
	mutex.c		\
	optparse.c	\
	rrd_strtod.c	\
	rrd_create.c	\
	hash_32.c	\
	rrd_parsetime.c	\
	rrd_hw.c	\
	rrd_hw_math.c	\
	rrd_hw_update.c	\
	rrd_diff.c	\
	rrd_format.c	\
	rrd_info.c	\
	rrd_error.c	\
	rrd_open.c	\
	rrd_client.c	\
	rrd_nan_inf.c	\
	rrd_rpncalc.c	\
	rrd_utils.c	\
	rrd_snprintf.c  \
	rrd_update.c	\
	rrd_modify.c	\
	quicksort.c     \
	rrd_thread_safe.c

RRD_C_FILES =		\
	rrd_version.c	\
	rrd_last.c	\
	rrd_lastupdate.c	\
	rrd_first.c	\
	rrd_dump.c	\
	rrd_flushcached.c \
	rrd_fetch.c	\
	rrd_fetch_cb.c  \
	rrd_resize.c \
	rrd_tune.c	\
	rrd_list.c


if BUILD_RRDGRAPH
RRD_C_FILES += rrd_graph.c	\
	rrd_graph_helper.c	\
	rrd_xport.c	\
	rrd_gfx.c \
	pngsize.c
endif

if BUILD_RRDRESTORE
RRD_C_FILES += rrd_restore.c
endif

noinst_HEADERS = \
	unused.h \
        gettext.h \
	mutex.h \
        rrd_strtod.h \
	rrd_snprintf.h \
	rrd_parsetime.h \
	rrd_config_bottom.h rrd_i18n.h \
	rrd_format.h rrd_tool.h rrd_xport.h optparse.h rrd.h rrd_rpncalc.h \
	rrd_hw.h rrd_hw_math.h rrd_hw_update.h \
	rrd_restore.h rrd_create.h \
	fnv.h rrd_graph.h \
	rrd_is_thread_safe.h \
	rrd_modify.h rrd_update.h rrd_config.h \
	quicksort.h

if BUILD_LIBDBI
RRD_C_FILES += rrd_fetch_libdbi.c
endif

if BUILD_LIBRADOS
UPD_C_FILES += rrd_rados.c
noinst_HEADERS += rrd_rados.h
endif

noinst_LTLIBRARIES        = librrdupd.la

lib_LTLIBRARIES           = librrd.la 

librrdupd_la_SOURCES      = $(UPD_C_FILES)
librrdupd_la_LIBADD       = $(CORE_LIBS)
librrdupd_la_CFLAGS       = $(AM_CFLAGS) $(MULTITHREAD_CFLAGS)
librrdupd_la_LDFLAGS 	  = $(MULTITHREAD_LDFLAGS) -version-info @LIBVERS@

librrd_la_SOURCES         = $(RRD_C_FILES)
librrd_la_DEPENDENCIES    = librrdupd.la
librrd_la_LIBADD          = librrdupd.la $(ALL_LIBS) $(LIBINTL)
librrd_la_CFLAGS          = $(AM_CFLAGS) $(MULTITHREAD_CFLAGS)
librrd_la_LDFLAGS         = $(MULTITHREAD_LDFLAGS) -version-info @LIBVERS@
librrd_la_LDFLAGS         += -export-symbols librrd.sym

include_HEADERS	= rrd.h rrd_format.h rrd_client.h

bin_PROGRAMS	= rrdtool rrdupdate

if BUILD_RRDCGI
bin_PROGRAMS += rrdcgi
endif

if BUILD_RRDCACHED
bin_PROGRAMS += rrdcached
endif

rrdcgi_SOURCES	= rrd_cgi.c
rrdcgi_LDADD	= librrd.la librrdupd.la

rrdupdate_SOURCES = rrdupdate.c
rrdupdate_LDADD	  = librrdupd.la

rrdtool_SOURCES = rrd_tool.c
rrdtool_DEPENDENCIES = librrd.la
rrdtool_LDADD	= librrd.la

rrdcached_SOURCES = rrd_daemon.c
rrdcached_DEPENDENCIES = librrd.la librrdupd.la
rrdcached_CPPFLAGS = -DVERSION='"$(VERSION)"' -DLOCALSTATEDIR='"$(localstatedir)"'
rrdcached_LDADD = librrd.la librrdupd.la $(ALL_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = librrd.pc

# strftime is here because we do not usually need it. unices have propper
# iso date support
EXTRA_DIST= librrd.pc.in strftime.c strftime.h \
	win32comp.c  rrd_thread_safe_nt.c get_ver.awk librrd.sym plbasename.c plbasename.h


install-exec-hook:
	(cd $(DESTDIR)$(bindir) && $(LN_S) rrdupdate rrdcreate && $(LN_S) rrdupdate rrdinfo) || true

distclean-local:
	-rm -rf rrd_config.h
