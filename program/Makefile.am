## Process this file with automake to produce Makefile.in
RSYNC = rsync --rsh=ssh

# build the following subdirectories
SUBDIRS = libraries bindings src doc examples

  # the following files are not mentioned in any other Makefile
EXTRA_DIST = COPYRIGHT CHANGES NT-BUILD-TIPS.txt TODO CONTRIBUTORS rrdtool.spec

CLEANFILES = config.cache

# use relaxed rules when building dists
AUTOMAKE_OPTIONS= foreign no-dependencies

# where we keep local rules for automake
ACLOCAL_M4= 	$(top_srcdir)/config/aclocal.m4
AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config
AUTOCONF = @AUTOCONF@ --localdir=$(top_srcdir)/config

to-docs: to-versync
	(cd doc && $(MAKE) clean && $(MAKE) && $(MAKE) pdf)
	(cd website && wmk-1.7.4 -f manual tutorial contributors.wml && ./site-sync )

to-versync: 
	perl -i -p -e '"$(VERSION)" =~ /(\d+)\.(\d+)\.(\d+)/; $$v=sprintf("%1d.%02d0%02d1" ,$${1},$${2},$${3}); s|VERSION\s*=\s*[\d.]+|VERSION = $$v|' perl-*/RRD?.pm
	perl -i -p -e 's|RRDtool\s+\d+\.\d+\.\d+ |RRDtool $(VERSION) |' src/*.[ch]

to-dist: to-docs dist
	mv $(PACKAGE)-$(VERSION).tar.gz archive

to-scp: to-dist
	cp CHANGES  archive/$(PACKAGE)-$(VERSION).tar.gz /home/oetiker/public_html/webtools/rrdtool/pub/
	(cd /home/oetiker/public_html/webtools/rrdtool/pub; rm $(PACKAGE).tar.gz; ln -s $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE).tar.gz)

#	$(RSYNC) CHANGES archive/$(PACKAGE)-$(VERSION).tar.gz tobi@ipn.caida.org:/ipn/web/Tools/RRDtool/pub/

site-perl-inst: site-perl-install

site-perl-install: bindings/perl-piped/Makefile bindings/perl-shared/Makefile
	cd bindings/perl-piped && $(MAKE) install
	cd bindings/perl-shared && $(MAKE) install

site-tcl-install:
	cd bindings/tcl && $(MAKE) tcl-install

##END##
