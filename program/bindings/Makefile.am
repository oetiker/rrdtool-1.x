SUBDIRS = tcl

# the following files are not mentioned in any other Makefile
EXTRA_DIST = `cat perl-*/MANIFEST`

# lets schedule the perl stuff for installation
# the special call to install-sh is because the -d switch is not portable
install-data-local:
	./config/mkinstalldirs $(DESTDIR)$(prefix)/lib/perl/auto/RRDs
	$(INSTALL) -m 644 perl-piped/RRDp.pm $(DESTDIR)$(prefix)/lib/perl
	$(INSTALL) -m 644 perl-shared/RRDs.pm $(DESTDIR)$(prefix)/lib/perl
	$(INSTALL) -m 644 perl-shared/blib/arch/auto/RRDs/RRDs.bs $(DESTDIR)$(prefix)/lib/perl/auto/RRDs
	$(INSTALL) -m 755 perl-shared/blib/arch/auto/RRDs/RRDs.@SO_EXT@ $(DESTDIR)$(prefix)/lib/perl/auto/RRDs

# add the following to the all target
all-local:      @COMP_PERL@

# rules for building the perl module
perl_piped: perl-piped/Makefile
	cd perl-piped && $(MAKE)

perl-piped/Makefile: perl-piped/Makefile.PL
	cd perl-piped && $(PERL) Makefile.PL $(PERL_MAKE_OPTIONS)

perl_shared: perl-shared/Makefile
	cd perl-shared && $(MAKE)

perl-shared/Makefile: perl-shared/Makefile.PL
	cd perl-shared && $(PERL) Makefile.PL $(PERLFLAGS) $(PERL_MAKE_OPTIONS)

clean-local:
	cd perl-piped && test -f Makefile && $(MAKE) clean || true
	cd perl-shared && test -f Makefile && $(MAKE) clean || true

##END##
