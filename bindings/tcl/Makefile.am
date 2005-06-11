
EXTRA_DIST = README tclrrd.c

VERSION = @VERSION@

AM_CFLAGS = @CFLAGS@

TCL_PREFIX = @TCL_PREFIX@
TCL_SHLIB_LD = @TCL_SHLIB_LD@
TCL_SHLIB_CFLAGS = @TCL_SHLIB_CFLAGS@
TCL_SHLIB_SUFFIX = @TCL_SHLIB_SUFFIX@
TCL_PACKAGE_PATH = @TCL_PACKAGE_PATH@
TCL_LD_SEARCH_FLAGS = @TCL_LD_SEARCH_FLAGS@

CLEANFILES = tclrrd.o tclrrd.so

SRC_DIR            = $(top_srcdir)/src
AM_CPPFLAGS        = -I$(TCL_PREFIX)/include -I$(SRC_DIR)
LIBDIRS            = -L$(top_builddir)/src/.libs -L$(top_builddir)/src -L$(libdir)
LIB_RUNTIME_DIR    = $(libdir)

pkglibdir = @TCL_PACKAGE_DIR@

if BUILD_TCL

# Automake doen't like `tclrrd$(VERSION)$(TCL_SHLIB_SUFFIX)' as
# library name. So we build and install this library `by hand'.
#
# We do, however, specify a lib_LIBRARIES target such that
# automake creates the directory (if neecessary).
#
TCL_RRD_LIB = tclrrd$(VERSION)$(TCL_SHLIB_SUFFIX)

lib_LIBRARIES =

all-local: $(TCL_RRD_LIB)

pkglib_DATA = pkgIndex.tcl

pkglib_SCRIPTS = ifOctets.tcl

$(TCL_RRD_LIB): tclrrd.o
	$(TCL_SHLIB_LD) $(TCL_LD_SEARCH_FLAGS) $(LIBDIRS) $< -o $@ -lrrd -lm

tclrrd.o: tclrrd.c
	$(CC) $(AM_CFLAGS) $(CFLAGS) $(TCL_SHLIB_CFLAGS) $(AM_CPPFLAGS) -c $< -DVERSION=\"$(VERSION)\"

pkgIndex.tcl:
	echo "package ifneeded Rrd $(VERSION) \"load $(libdir)/tclrrd$(VERSION)[info sharedlibextension]\"" > $@

install-exec-local: $(TCL_RRD_LIB)
	@$(NORMAL_INSTALL)
	$(INSTALL_PROGRAM) $(TCL_RRD_LIB) $(DESTDIR)$(libdir)/$(TCL_RRD_LIB)

endif

diff:
	cd .. ; diff -c -u -r -N --exclude Makefile --exclude html --exclude doc --exclude Makefile.in --exclude Makefile.old --exclude perl --exclude aclocal.m4 --exclude configure rrdtool-1.0.13 rrdtool-1.0.13-ibr > rrdtool-1.0.13-ibr.patch
	

