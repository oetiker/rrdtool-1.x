SUBDIRS = tcl 
# the following files are not mentioned in any other Makefile
EXTRA_DIST = $(shell for A in perl-*; do cat $$A/MANIFEST | sed -e "s,^,$$A/," ; done)

# add the following to the all target
all-local:  @COMP_PERL@

install-data-local:
	cd perl-piped && test -f Makefile && $(MAKE) install || true
	cd perl-shared && test -f Makefile && $(MAKE) install || true

# rules for building the perl module
perl_piped: perl-piped/Makefile
	cd perl-piped && $(MAKE)

perl-piped/Makefile: perl-piped/Makefile.PL
	cd perl-piped && $(PERL) Makefile.PL $(PERL_MAKE_OPTIONS)

perl_shared: perl-shared/Makefile
	cd perl-shared && $(MAKE) RPATH=$(exec_prefix)/lib

perl-shared/Makefile: perl-shared/Makefile.PL
	cd perl-shared && $(PERL) Makefile.PL $(PERLFLAGS) $(PERL_MAKE_OPTIONS)
# LIBS="$(LDFLAGS) $(LIBS)" $(PERLFLAGS) $(PERL_MAKE_OPTIONS)

clean-local:
	cd perl-piped && test -f Makefile && $(MAKE) clean || true
	cd perl-shared && test -f Makefile && $(MAKE) clean || true

##END##
