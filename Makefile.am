# $Id$

## Process this file with automake to produce Makefile.in
RSYNC = rsync --rsh=ssh

# build the following subdirectories

SUBDIRS = po src doc bindings tests

if BUILD_EXAMPLES
SUBDIRS += examples
endif

 # the following files are not mentioned in any other Makefile
EXTRA_DIST = COPYRIGHT CHANGES TODO CONTRIBUTORS THREADS \
             rrdtool.spec favicon.ico bootstrap \
             libtool \
             netware/Makefile m4/snprintf.m4 \
             etc/rrdcached-default-redhat etc/rrdcached-init-redhat \
             win32/README win32/Makefile.msc

CLEANFILES = config.cache

# use relaxed rules when building dists
AUTOMAKE_OPTIONS= foreign 

# where we keep local rules for automake
ACLOCAL_AMFLAGS=-I m4
ACLOCAL_M4= $(top_srcdir)/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config
#AUTOCONF = @AUTOCONF@ --localdir=$(top_srcdir)/config

#	$(RSYNC) CHANGES archive/$(PACKAGE)-$(VERSION).tar.gz tobi@ipn.caida.org:/ipn/web/Tools/RRDtool/pub/

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	etc/rrdcached.socket \
	etc/rrdcached.service
endif

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

##END##
