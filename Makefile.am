# $Id$

## Process this file with automake to produce Makefile.in
RSYNC = rsync --rsh=ssh

# build the following subdirectories

SUBDIRS = po src bindings tests etc

if BUILD_DOCS
SUBDIRS += doc
endif

if BUILD_EXAMPLES
SUBDIRS += examples
endif

if BUILD_FUZZERS
SUBDIRS += fuzz
endif

# "make distcheck" installs into a temporary prefix, but the systemd unit
# directory from pkg-config is absolute, so move it into that prefix.
if HAVE_SYSTEMD
AM_DISTCHECK_CONFIGURE_FLAGS = --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)
endif

 # the following files are not mentioned in any other Makefile
EXTRA_DIST = COPYRIGHT CHANGES TODO CONTRIBUTORS THREADS VERSION LICENSE \
             rrdtool.spec favicon.ico bootstrap \
             .indent.pro \
             m4/snprintf.m4 \
             win32/build-rrdtool.dot win32/build-rrdtool.pdf win32/build-rrdtool.svg \
             win32/librrd-8.def win32/librrd-8.rc win32/librrd-8.vcxproj \
             win32/Makefile.msc win32/Makefile_vcpkg.msc win32/README win32/README-MinGW-w64 \
             win32/rrdcgi.rc win32/rrd_config.h \
             win32/rrd.sln win32/rrdtool.rc win32/rrdtool.vcxproj win32/rrdupdate.rc \
             win32/rrdcgi.vcxproj win32/rrdupdate.vcxproj win32/uac.manifest \
             win32/dirent.h

CLEANFILES = config.cache

# use relaxed rules when building dists
AUTOMAKE_OPTIONS= foreign

# where we keep local rules for automake
ACLOCAL_AMFLAGS=-I m4
ACLOCAL_M4= $(top_srcdir)/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config
#AUTOCONF = @AUTOCONF@ --localdir=$(top_srcdir)/config

#	$(RSYNC) CHANGES archive/$(PACKAGE)-$(VERSION).tar.gz tobi@ipn.caida.org:/ipn/web/Tools/RRDtool/pub/

etc-install:
	cd etc && $(MAKE) install

site-perl-inst: site-perl-install

site-perl-install: all bindings/perl-piped/Makefile bindings/perl-shared/Makefile
	cd bindings/perl-piped && $(MAKE) install
	cd bindings/perl-shared && $(MAKE) install

site-tcl-install: all
	cd bindings/tcl && $(MAKE) install

# into the interpreter's own site-packages, from the wheel "make" built
site-python-install: all
	$(PYTHON) -m pip install --no-index --no-deps --disable-pip-version-check \
	  $${DESTDIR:+--root="$$DESTDIR"} bindings/python/dist/*.whl

# find . -name "*.c" -or -name "*.h" | xargs perl -0777 -n -e 'while (s/typedef\s+(?:unsigned\s+|signed\s+|unival\s+)?\S+\s+\*?([^{}\s;(]+)//){print "-T$1\n"}'
indent:
	find ./ -name "*.[ch]" | xargs indent

clean-local:
	-rm -f config.h bindings/perl-piped/Makefile.old bindings/perl-shared/Makefile.old



# The version in doc/rrdbuild.pod is set by conftools/bump-version.sh,
# which both release paths run before make dist; the tarball ships that
# file and the pages made from it unchanged, and the source is not touched.
dist-hook: VERSION
	$(AM_V_GEN)cd $(distdir) && $(PERL) -i -p -e 's/^\$$VERSION.+/\$$VERSION='$(NUMVERS)';/' bindings/perl-*/*.pm
	$(AM_V_GEN)cd $(distdir) && $(PERL) -i -p -e 's/^Version:.+/Version: '$(PACKAGE_VERSION)'/' rrdtool.spec

##END##
