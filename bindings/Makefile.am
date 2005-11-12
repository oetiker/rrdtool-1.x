if BUILD_TCL
SUB_tcl = tcl
endif

if BUILD_PYTHON
SUB_python = python
endif

SUBDIRS = $(SUB_tcl) $(SUB_python)

# the following files are not mentioned in any other Makefile
EXTRA_DIST = perl-piped/MANIFEST perl-piped/README perl-piped/Makefile.PL perl-piped/RRDp.pm perl-piped/t/base.t \
	perl-shared/ntmake.pl perl-shared/MANIFEST perl-shared/README perl-shared/Makefile.PL perl-shared/RRDs.pm  perl-shared/RRDs.xs perl-shared/t/base.t


# add the following to the all target
all-local:  @COMP_PERL@

install-data-local:
	test -f perl-piped/Makefile && cd perl-piped && $(MAKE) install || true
	test -f perl-shared/Makefile && cd perl-shared && $(MAKE) install || true

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
	test -f perl-piped/Makefile && cd perl-piped && $(MAKE) clean || rm perl-piped/Makefile || true
	test -f perl-shared/Makefile && cd perl-shared && $(MAKE) clean || rm perl-shared/Makefile || true

##END##
