ACLOCAL_AMFLAGS = -I m4
## Process this file with automake to produce Makefile.in
RSYNC = rsync --rsh=ssh

# build the following subdirectories
if BUILD_LIBINTL
PO=po
else
PO=
endif

SUBDIRS = $(PO) src examples doc bindings

 # the following files are not mentioned in any other Makefile
EXTRA_DIST = COPYRIGHT CHANGES WIN32-BUILD-TIPS.txt TODO CONTRIBUTORS THREADS \
             intltool-extract.in  intltool-merge.in    intltool-update.in \
	     rrdtool.spec favicon.ico win32/Makefile win32/config.h \
	     win32/rrd.dsp win32/rrd.sln win32/rrd.vcproj \
	     win32/rrd_config.h.msvc win32/rrdlib.vcproj win32/rrdtool.dsp \
	     win32/rrdtool.dsw win32/rrdtool.vcproj netware/Makefile

	     

CLEANFILES = config.cache

# use relaxed rules when building dists
AUTOMAKE_OPTIONS= foreign 

# where we keep local rules for automake

ACLOCAL_M4= $(top_srcdir)/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config
#AUTOCONF = @AUTOCONF@ --localdir=$(top_srcdir)/config

to-docs: to-versync
	(cd doc && $(MAKE) clean && $(MAKE) && $(MAKE) pdf)

to-dist: to-docs dist
	mv $(PACKAGE)-$(VERSION).tar.gz archive

to-scp: to-dist
	cp CHANGES  archive/$(PACKAGE)-$(VERSION).tar.gz /home/oetiker/public_html/webtools/rrdtool/pub/
	(cd /home/oetiker/public_html/webtools/rrdtool/pub; rm $(PACKAGE).tar.gz; ln -s $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE).tar.gz)

#	$(RSYNC) CHANGES archive/$(PACKAGE)-$(VERSION).tar.gz tobi@ipn.caida.org:/ipn/web/Tools/RRDtool/pub/

site-perl-inst: site-perl-install

site-perl-install: all bindings/perl-piped/Makefile bindings/perl-shared/Makefile
	cd bindings/perl-piped && $(MAKE) install
	cd bindings/perl-shared && $(MAKE) install

site-tcl-install: all
	cd bindings/tcl && $(MAKE) tcl-install

site-python-install: all
	cd bindings/python && $(PYTHON) setup.py install

# find . -name "*.c" -or -name "*.h" | xargs perl -0777 -n -e 'while (s/typedef\s+(?:unsigned\s+|signed\s+|unival\s+)?\S+\s+\*?([^{}\s;(]+)//){print "-T$1\n"}'
indent:
	find ./ -name "*.[ch]" | xargs indent

##END##
