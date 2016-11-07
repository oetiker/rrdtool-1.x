.PHONY: lua perl-piped perl-shared python ruby

if BUILD_TCL
SUB_tcl = tcl
endif
if BUILD_LUA
SUB_lua = lua
endif

SUBDIRS = $(SUB_tcl) $(SUB_lua)

if ENABLE_RPATH
PYTHON_RPATH=--rpath=$(libdir)
PERL_RPATH=RPATH=$(libdir)
endif

# the following files are not mentioned in any other Makefile
EXTRA_DIST = perl-piped/MANIFEST perl-piped/README perl-piped/Makefile.PL perl-piped/RRDp.pm perl-piped/t/base.t \
	     perl-shared/ntmake-build perl-shared/MANIFEST perl-shared/README perl-shared/Makefile.PL perl-shared/RRDs.pm perl-shared/RRDs.ppd perl-shared/RRDs.xs perl-shared/t/base.t perl-shared/t/callback-long.t perl-shared/t/callback.t \
	     ruby/CHANGES     ruby/README      ruby/extconf.rb  ruby/main.c      ruby/test.rb \
	     dotnet/rrdlib.cs dotnet/rrd_binding_test.cs dotnet/rrdlib.sln dotnet/favicon.ico dotnet/dnrrdlib.csproj dotnet/Properties/AssemblyInfo.cs  dotnet/dnrrd_binding_test.csproj   dotnet/RrdException.cs \
             python/COPYING python/README.md python/rrdtoolmodule.c python/setup.py


# add the following to the all target
all-local:  @COMP_PERL@ @COMP_RUBY@ @COMP_PYTHON@

install-data-local:
	$(AM_V_GEN)test -f ${builddir}/perl-piped/Makefile \
           && ( cd ${builddir}/perl-piped \
	        && $(MAKE) install ) \
	   || true
	$(AM_V_GEN)test -f ${builddir}/perl-shared/Makefile \
           && ( cd ${builddir}/perl-shared \
                && $(MAKE) install ) \
           || true
	$(AM_V_GEN)test -f ${builddir}/ruby/Makefile \
           && ( cd ${builddir}/ruby \
                && $(MAKE) EPREFIX=$(DESTDIR)$(exec_prefix) $(RUBY_MAKE_OPTIONS) install ) \
           || true
	$(AM_V_GEN)test -d ${builddir}/python/build \
           && ( cd ${builddir}/python \
                && env BUILDLIBDIR=${abs_top_builddir}/src/.libs \
                  $(PYTHON) ${abs_srcdir}/python/setup.py install \
                     --skip-build --root=$(DESTDIR) --prefix=$(prefix) \
                     --exec-prefix=$(exec_prefix)) \
           || true

# rules for building the ruby module
# RUBYARCHDIR= is to work around in a makefile quirk not sure 
# it is is the right thing todo, but it makes rrdtool build on freebsd as well
ruby:
	-mkdir -p ${builddir}/ruby
	( cd ${builddir}/ruby \
          && $(RUBY) ${abs_srcdir}/ruby/extconf.rb \
          && $(MAKE) \
             EPREFIX=$(exec_prefix) \
             ABS_TOP_SRCDIR=${abs_top_srcdir} \
             ABS_TOP_BUILDDIR=${abs_top_builddir} \
             $(RUBY_MAKE_OPTIONS) RUBYARCHDIR= )

# rules for building the python module
python:
	-mkdir -p ${builddir}/$@
	cd ${builddir}/$@ \
	  && ( test -e rrdtoolmodule.c || ln -s ${abs_srcdir}/$@/rrdtoolmodule.c ) \
          && env \
               ABS_TOP_SRCDIR=${abs_top_srcdir} \
               ABS_TOP_BUILDDIR=${abs_top_builddir} \
               LD_RUN_PATH=$(libdir) \
               $(PYTHON) ${abs_srcdir}/$@/setup.py build_ext $(PYTHON_RPATH) \
          && env \
               ABS_TOP_SRCDIR=${abs_top_srcdir} \
               ABS_TOP_BUILDDIR=${abs_top_builddir} \
               $(PYTHON) ${abs_srcdir}/$@/setup.py build

# rules for building the perl module
perl-piped:
	-mkdir -p ${builddir}/$@
	( cd ${builddir}/$@ \
	  && ( test -e Makefile.PL || ln -s ${abs_srcdir}/$@/Makefile.PL ) \
	  && ( test -e RRDp.pm || ln -s ${abs_srcdir}/$@/RRDp.pm ) \
	  && ( test -e t || ln -s ${abs_srcdir}/$@/t ) \
          && env \
             ABS_TOP_SRCDIR=${abs_top_srcdir} \
             ABS_TOP_BUILDDIR=${abs_top_builddir} \
             ABS_SRCDIR=${abs_srcdir}/$@ \
             $(PERL) Makefile.PL $(PERL_MAKE_OPTIONS) \
          && $(MAKE) )

perl-shared:
	-mkdir -p ${builddir}/$@
	( cd ${builddir}/$@ \
	  && ( test -e Makefile.PL || ln -s ${abs_srcdir}/$@/Makefile.PL ) \
	  && ( test -e RRDs.pm || ln -s ${abs_srcdir}/$@/RRDs.pm ) \
	  && ( test -e RRDs.ppd || ln -s ${abs_srcdir}/$@/RRDs.ppd ) \
	  && ( test -e RRDs.xs || ln -s ${abs_srcdir}/$@/RRDs.xs ) \
	  && ( test -e t || ln -s ${abs_srcdir}/$@/t ) \
          && env \
             ABS_TOP_SRCDIR=${abs_top_srcdir} \
             ABS_TOP_BUILDDIR=${abs_top_builddir} \
             ABS_SRCDIR=${abs_srcdir}/$@ \
             $(PERL) Makefile.PL $(PERLFLAGS) $(PERL_MAKE_OPTIONS) $(PERL_RPATH) \
          && $(MAKE) )
	-mkdir -p ${builddir}/perl-shared

clean-local:
	-( test -f ${builddir}/perl-piped/Makefile \
          && cd ${builddir}/perl-piped \
          && ( $(MAKE) clean || true ) \
          && rm -f Makefile )
	-( test -f ${builddir}/perl-shared/Makefile \
          && cd ${builddir}/perl-shared \
          && ( $(MAKE) clean || true ) \
          && rm -f Makefile )
	-( test -f ${builddir}/ruby/Makefile \
          && cd ${builddir}/ruby \
          && ( $(MAKE) clean || true ) \
          && rm -f Makefile )
	-rm -rf ${builddir}/python/build
##END##
