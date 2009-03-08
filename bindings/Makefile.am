.PHONY: python ruby

if BUILD_TCL
SUB_tcl = tcl
endif

SUBDIRS = $(SUB_tcl)

# the following files are not mentioned in any other Makefile
EXTRA_DIST = perl-piped/MANIFEST perl-piped/README perl-piped/Makefile.PL perl-piped/RRDp.pm perl-piped/t/base.t \
	     perl-shared/ntmake-build perl-shared/MANIFEST perl-shared/README perl-shared/Makefile.PL perl-shared/RRDs.pm  perl-shared/RRDs.xs perl-shared/t/base.t \
	     ruby/CHANGES     ruby/README      ruby/extconf.rb  ruby/main.c      ruby/test.rb \
             python/ACKNOWLEDGEMENT python/AUTHORS python/COPYING python/README python/rrdtoolmodule.c python/setup.py


# add the following to the all target
all-local:  @COMP_PERL@ @COMP_RUBY@ @COMP_PYTHON@

install-data-local:
	test -f perl-piped/Makefile && cd perl-piped && $(MAKE) install || true
	test -f perl-shared/Makefile && cd perl-shared && $(MAKE) install || true
	test -f ruby/Makefile && cd ruby && $(MAKE) EPREFIX=$(DESTDIR)$(exec_prefix) $(RUBY_MAKE_OPTIONS) install || true
	test -d python/build && cd python && env BUILDLIBDIR=../../src/.libs $(PYTHON) setup.py install --skip-build --prefix=$(DESTDIR)$(prefix) --exec-prefix=$(DESTDIR)$(exec_prefix) || true

# rules for buildung the ruby module
# RUBYARCHDIR= is to work around in a makefile quirk not sure 
# it is is the right thing todo, but it makes rrdtool build on freebsd as well
ruby: 
	cd ruby && $(RUBY) extconf.rb && $(MAKE) EPREFIX=$(exec_prefix) $(RUBY_MAKE_OPTIONS) RUBYARCHDIR=

# rules for buildung the pyton module
python:
	cd python && env BUILDLIBDIR=../../src/.libs $(PYTHON) setup.py build_ext --rpath=$(libdir) && env LIBDIR=../../src/.libs $(PYTHON) setup.py build

# rules for building the perl module
perl_piped: perl-piped/Makefile
	cd perl-piped && $(MAKE)

perl-piped/Makefile: perl-piped/Makefile.PL
	cd perl-piped && $(PERL) Makefile.PL $(PERL_MAKE_OPTIONS)

perl_shared: perl-shared/Makefile
	cd perl-shared && $(MAKE)

perl-shared/Makefile: perl-shared/Makefile.PL
	cd perl-shared && $(PERL) Makefile.PL $(PERLFLAGS) $(PERL_MAKE_OPTIONS) RPATH=$(libdir)
# LIBS="$(LDFLAGS) $(LIBS)" $(PERLFLAGS) $(PERL_MAKE_OPTIONS)

clean-local:
	test -f perl-piped/Makefile && cd perl-piped && $(MAKE) clean || true
	test -f perl-piped/Makefile && rm perl-piped/Makefile || true
	test -f perl-shared/Makefile && cd perl-shared && $(MAKE) clean || true
	test -f perl-shared/Makefile && rm -f perl-shared/Makefile || true 
	test -f ruby/Makefile && cd ruby && $(MAKE) clean && rm Makefile || true 
	test -d python/build && cd python &&  rm -rf build || true
##END##
