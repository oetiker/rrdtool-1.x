# $Id$

## Process this file with automake to produce Makefile.in
RSYNC = rsync --rsh=ssh

# build the following subdirectories

SUBDIRS = po src examples doc bindings

 # the following files are not mentioned in any other Makefile
EXTRA_DIST = COPYRIGHT CHANGES TODO CONTRIBUTORS THREADS \
             rrdtool.spec favicon.ico bootstrap \
             libtool \
             netware/Makefile  \
             etc/rrdcached-default etc/rrdcached-init \
             win32/README win32/Makefile.msc

CLEANFILES = config.cache

# use relaxed rules when building dists
AUTOMAKE_OPTIONS= foreign 

# where we keep local rules for automake
ACLOCAL_AMFLAGS=-I m4
ACLOCAL_M4= $(top_srcdir)/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config
#AUTOCONF = @AUTOCONF@ --localdir=$(top_srcdir)/config

TESTS	=	tests/modify1 \
		tests/modify2 \
		tests/modify3 \
		tests/modify4 \
		tests/modify5 \
		tests/tune1 \
		tests/tune2

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

# find . -name "*.c" -or -name "*.h" | xargs perl -0777 -n -e 'while (s/typedef\s+(?:unsigned\s+|signed\s+|unival\s+)?\S+\s+\*?([^{}\s;(]+)//){print "-T$1\n"}'
indent:
	find ./ -name "*.[ch]" | xargs indent

clean-local:
	-rm -f config.h bindings/perl-piped/Makefile.old bindings/perl-shared/Makefile.old

##END##
