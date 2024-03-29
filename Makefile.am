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

 # the following files are not mentioned in any other Makefile
EXTRA_DIST = COPYRIGHT CHANGES TODO CONTRIBUTORS THREADS VERSION LICENSE \
             rrdtool.spec favicon.ico bootstrap \
             libtool .indent.pro \
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
	cd bindings/tcl && $(MAKE) tcl-install

site-python-install: all
	cd bindings/python && $(PYTHON) setup.py install $(if $(DESTDIR),--root=$(DESTDIR))

# Skip this part of "make distcheck": the perl, ruby, and tcl files
# are not uninstalled by "make uninstall".
distuninstallcheck:
	@:

# find . -name "*.c" -or -name "*.h" | xargs perl -0777 -n -e 'while (s/typedef\s+(?:unsigned\s+|signed\s+|unival\s+)?\S+\s+\*?([^{}\s;(]+)//){print "-T$1\n"}'
indent:
	find ./ -name "*.[ch]" | xargs indent

clean-local:
	-rm -f config.h bindings/perl-piped/Makefile.old bindings/perl-shared/Makefile.old



dist-hook: VERSION
	$(AM_V_GEN)cd $(distdir) && $(PERL) -i -p -e 's/^\$$VERSION.+/\$$VERSION='$(NUMVERS)';/' bindings/perl-*/*.pm
	$(AM_V_GEN)cd $(distdir) && $(PERL) -i -p -e 's/RRDtool 1.GIT, Copyright by Tobi Oetiker/RRDtool '$(PACKAGE_VERSION)', Copyright 1997-'`date +%Y`' by Tobi Oetiker/' src/*.h src/*.c
	$(AM_V_GEN)cd $(distdir) && $(PERL) -i -p -e 's/^Version:.+/Version: '$(PACKAGE_VERSION)'/' rrdtool.spec
	$(AM_V_GEN)$(PERL) -i -p -e 's/rrdtool-[\.\d]+\d(-[a-z0-9]+)?/rrdtool-'$(PACKAGE_VERSION)'/g' doc/rrdbuild.pod
	$(AM_V_GEN)(cd doc && $(MAKE)) && cp -p doc/rrdbuild.* $(distdir)/doc

##END##
