## Process this file with automake to produce Makefile.in

#AUTOMAKE_OPTIONS   = foreign
#
#ACLOCAL_M4	   = $(top_srcdir)/config/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config
fontsdir =  $(datadir)/rrdtool/fonts
fonts_DATA = DejaVuSansMono-Roman.ttf

if STATIC_PROGRAMS
AM_LDFLAGS = -all-static
endif

#INCLUDES = $(FREETYPE_INCLUDES) $(ART_INCLUDES) \
#           $(PNG_INCLUDES) $(ZLIB_INCLUDES)
RRD_DEFAULT_FONT=@RRD_DEFAULT_FONT@
AM_CPPFLAGS = -DRRD_DEFAULT_FONT=\"$(RRD_DEFAULT_FONT)\" -DNUMVERS=@NUMVERS@

UPD_C_FILES =		\
	rrd_getopt.c	\
	rrd_getopt1.c	\
	parsetime.c	\
	rrd_hw.c	\
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
        art_rgba_svp.c \
	rrd_gfx.c \
	rrd_afm.c rrd_afm_data.c \
	rrd_dump.c	\
	rrd_fetch.c	\
	rrd_tool.c	\
	rrd_resize.c \
	rrd_tune.c

noinst_HEADERS = \
        art_rgba_svp.h \
	unused.h \
	rrd_gfx.h \
	rrd_getopt.h parsetime.h \
	rrd_format.h rrd_tool.h rrd_xport.h rrd.h rrd_hw.h rrd_rpncalc.h \
	rrd_nan_inf.h fnv.h rrd_graph.h rrd_afm.h rrd_afm_data.h \
	rrd_is_thread_safe.h

noinst_LTLIBRARIES        = librrdupd.la

lib_LTLIBRARIES           = librrd.la 
if BUILD_MULTITHREAD
lib_LTLIBRARIES           += librrd_th.la
endif

librrdupd_la_SOURCES      = $(UPD_C_FILES) rrd_not_thread_safe.c
librrdupd_la_LIBADD       = $(CORE_LIBS)

librrd_la_SOURCES         = $(RRD_C_FILES)
librrd_la_LIBADD          = librrdupd.la $(ALL_LIBS)

# This flag accepts an argument of the form current[:revision[:age]]. So,
# passing -version-info 3:12:1 sets current to 3, revision to 12, and age to 1.
#
# If either revision or age are omitted, they default to 0. Also note that
# age must be less than or equal to the current interface number.
#
# Here are a set of rules to help you update your library version information:
#
#   1. Start with version information of 0:0:0 for each libtool library.
#
#   2. Update the version information only immediately before a public
#   release of your software. More frequent updates are unnecessary, and
#   only guarantee that the current interface number gets larger faster.
#
#   3. If the library source code has changed at all since the last update,
#   then increment revision (c:r:a becomes c:r+1:a).
#
#   4. If any interfaces have been added, removed, or changed since the last
#   update, increment current, and set revision to 0.
#
#   5. If any interfaces have been added since the last public release, then
#   increment age.
#
#   6. If any interfaces have been removed since the last public release,
#   then set age to 0.
#
# Never try to set the interface numbers so that they correspond to the
# release number of your package. This is an abuse that only fosters
# misunderstanding of the purpose of library versions. Instead, use the
# -release flag (see Release numbers), but be warned that every release of
# your package will not be binary compatible with any other release.
#
# see http://www.gnu.org/software/libtool/manual.html#SEC32 for explanation
librrd_la_LDFLAGS         = -version-info 2:11:0

librrd_th_la_SOURCES         = $(UPD_C_FILES) $(RRD_C_FILES) rrd_thread_safe.c
librrd_th_la_CFLAGS          = $(MULTITHREAD_CFLAGS)
librrd_th_la_LDFLAGS         = $(MULTITHREAD_LDFLAGS) -version-info 2:11:0
librrd_th_la_LIBADD          = $(ALL_LIBS)

include_HEADERS	= rrd.h

bin_PROGRAMS	= rrdtool rrdupdate

if BUILD_RRDCGI
bin_PROGRAMS += rrdcgi
endif

rrdcgi_SOURCES	= rrd_cgi.c
rrdcgi_LDADD	= librrd.la

rrdupdate_SOURCES = rrdupdate.c
rrdupdate_LDADD	  = librrdupd.la

rrdtool_SOURCES = 
rrdtool_DEPENDENCIES = rrd_tool.o librrd.la
rrdtool_LDADD	= librrd.la

# strftime is here because we do not usually need it. unices have propper
# iso date support
EXTRA_DIST= strftime.c strftime.h $(fonts_DATA) \
	win32comp.c  rrd_thread_safe_nt.c get_ver.awk

