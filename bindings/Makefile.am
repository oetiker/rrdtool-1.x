.PHONY: lua perl-piped perl-shared python ruby

if BUILD_TCL
SUB_tcl = tcl
endif
if BUILD_LUA
SUB_lua = lua
endif

SUBDIRS = $(SUB_tcl) $(SUB_lua)

if ENABLE_RPATH
PYTHON_RPATH_LIBDIR=$(libdir)
PERL_RPATH=RPATH=$(libdir)
endif

# the following files are not mentioned in any other Makefile
EXTRA_DIST = perl-piped/MANIFEST perl-piped/README perl-piped/Makefile.PL perl-piped/RRDp.pm perl-piped/t/base.t \
	     perl-shared/ntmake-build perl-shared/MANIFEST perl-shared/README perl-shared/Makefile.PL perl-shared/RRDs.pm perl-shared/RRDs.ppd perl-shared/RRDs.xs perl-shared/t/base.t perl-shared/t/callback-long.t perl-shared/t/callback.t \
	     ruby/CHANGES     ruby/README      ruby/extconf.rb  ruby/main.c      ruby/test.rb \
	     dotnet/rrdlib.cs dotnet/rrd_binding_test.cs dotnet/rrdlib.sln dotnet/favicon.ico dotnet/dnrrdlib.csproj dotnet/Properties/AssemblyInfo.cs  dotnet/dnrrd_binding_test.csproj   dotnet/RrdException.cs \
             python/COPYING python/README.md python/rrdtoolmodule.c python/setup.py \
             python/tests/__init__.py python/tests/test_connect_error.py \
             python/tests/test_fetch_cb.py python/tests/test_lastupdate.py \
             python/tests/test_uninstall.py \
             python/uninstall.py perl-uninstall.mk perl-uninstall.pl


# add the following to the all target
all-local:  @COMP_PERL@ @COMP_RUBY@ @COMP_PYTHON@

# A binding that was not built is skipped; one that was built and fails
# to install fails "make install".
install-data-local:
	$(AM_V_GEN)for binding in $(COMP_PERL); do \
	  ( cd ${builddir}/$$binding && $(MAKE) pure_install ) || exit 1; \
	done
	$(AM_V_GEN)if test -n "$(COMP_RUBY)" && test -f ${builddir}/ruby/Makefile; then \
	  test -z "$(RUBY_INSTALL_DIR)" || $(MKDIR_P) "$(DESTDIR)$(RUBY_INSTALL_DIR)"; \
	  ( cd ${builddir}/ruby \
	    && $(MAKE) EPREFIX=$(DESTDIR)$(exec_prefix) $(RUBY_MAKE_OPTIONS) install ) || exit 1; \
	fi
	$(AM_V_GEN)if test -n "$(COMP_PYTHON)" && test -f ${builddir}/python/wheel.stamp; then \
	  $(PYTHON) -m pip install --upgrade --no-index --no-deps \
	    --no-cache-dir --disable-pip-version-check \
	    --target=$(DESTDIR)$(PYTHON_INSTALL_DIR) \
	    ${builddir}/python/dist/*.whl || exit 1; \
	fi

# Perl's packlists and Python's RECORD name the files this build installed.
# pure_install above leaves the shared perllocal.pod record untouched.
uninstall-local:
	@for binding in $(COMP_PERL); do \
	  if test -f ${builddir}/$$binding/Makefile; then \
	    $(MAKE) -C ${builddir}/$$binding -f Makefile \
	      -f $(abs_srcdir)/perl-uninstall.mk \
	      RRDTOOL_UNINSTALL_SCRIPT=$(abs_srcdir)/perl-uninstall.pl \
	      rrdtool-uninstall || exit 1; \
	  fi; \
	done
	@if test -n "$(COMP_PYTHON)" && test -f ${builddir}/python/wheel.stamp; then \
	  $(PYTHON) $(srcdir)/python/uninstall.py "$(DESTDIR)$(PYTHON_INSTALL_DIR)" \
	    ${builddir}/python/dist/*.whl; \
	fi
	@test -z "$(COMP_RUBY)" || test -z "$(RUBY_INSTALL_DIR)" \
	  || rm -f "$(DESTDIR)$(RUBY_INSTALL_DIR)/RRD.$(RUBY_DLEXT)"

# rules for building the ruby module
# RUBYARCHDIR= is to work around in a makefile quirk not sure 
# it is is the right thing todo, but it makes rrdtool build on freebsd as well
ruby:
	-mkdir -p ${builddir}/ruby
	( cd ${builddir}/ruby \
          && env \
             ABS_TOP_SRCDIR=${abs_top_srcdir} \
             ABS_TOP_BUILDDIR=${abs_top_builddir} \
             $(RUBY) ${abs_srcdir}/ruby/extconf.rb \
          && $(MAKE) \
             EPREFIX=$(exec_prefix) \
             ABS_TOP_SRCDIR=${abs_top_srcdir} \
             ABS_TOP_BUILDDIR=${abs_top_builddir} \
             $(RUBY_MAKE_OPTIONS) RUBYARCHDIR= )

# Build the python module as a wheel in ${builddir}/python/dist/; it is
# installed from there (via pip) by install-data-local. The wheel.stamp
# target gives the build real dependency tracking, so re-running `all`
# -- which `make install` and `make check` both do -- rebuilds the wheel
# only when a source actually changed. That also stops a `sudo make
# install` from leaving root-owned build artifacts that a later non-root
# `make` cannot remove. RRDTOOL_RPATH tells setup.py to embed the
# install libdir as the extension's RPATH.
python: ${builddir}/python/wheel.stamp

${builddir}/python/wheel.stamp: ${abs_srcdir}/python/rrdtoolmodule.c ${abs_srcdir}/python/setup.py
	-mkdir -p ${builddir}/python
	cd ${builddir}/python \
	  && rm -rf dist \
	  && ( test -e rrdtoolmodule.c || ln -s ${abs_srcdir}/python/rrdtoolmodule.c ) \
          && env \
               ABS_TOP_SRCDIR=${abs_top_srcdir} \
               ABS_TOP_BUILDDIR=${abs_top_builddir} \
               RRDTOOL_RPATH=$(PYTHON_RPATH_LIBDIR) \
               $(PYTHON) ${abs_srcdir}/python/setup.py bdist_wheel
	touch $@

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
	-rm -rf ${builddir}/python/build ${builddir}/python/dist ${builddir}/python/rrdtool.egg-info ${builddir}/python/wheel.stamp
##END##
