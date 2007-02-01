AM_CPPFLAGS = @CFLAGS@ -I$(top_srcdir)/src @PYTHON_INCLUDES@

pyexec_PROGRAMS = rrdtoolmodule.so

rrdtoolmodule_so_LDADD = $(top_builddir)/src/librrd.la
rrdtoolmodule_so_SOURCES = rrdtoolmodule.c

noinst_HEADERS =           rrd_extra.h

clean-local:
	rm -rf build

