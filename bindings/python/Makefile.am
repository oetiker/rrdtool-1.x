AM_CPPFLAGS = @CFLAGS@ -I../../src @PYTHON_INCLUDES@

#pythondir = $(pyexecdir)/python@PYTHON_VERSION@/site-packages

python_PROGRAMS = rrdtoolmodule.so

rrdtoolmodule_so_LDFLAGS = -module -shared -L../../src
rrdtoolmodule_so_LDADD = -lrrd
rrdtoolmodule_so_SOURCES =     rrdtoolmodule.c

noinst_HEADERS =                rrd_extra.h

clean:
	rm -rf build
