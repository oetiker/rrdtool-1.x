Summary: Round Robin Database Tool to store and display time-series data
Name: rrdtool
Version: 1.2.16
Release: 3%{?dist}
License: GPL
Group: Applications/Databases
URL: http://oss.oetiker.ch/%{name}/
Source: http://oss.oetiker.ch/%{name}/pub/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: gcc-c++, openssl-devel
BuildRequires: libpng-devel, zlib-devel, libart_lgpl-devel >= 2.0
BuildRequires: freetype-devel, python-devel >= 2.3
Requires: perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))

%{!?python_sitearch: %define python_sitearch %(%{__python} -c 'from distutils import sysconfig; print sysconfig.get_python_lib(1)')}
%{!?python_version: %define python_version %(%{__python} -c 'import sys; print sys.version.split(" ")[0]')}

%description
RRD is the Acronym for Round Robin Database. RRD is a system to store and
display time-series data (i.e. network bandwidth, machine-room temperature,
server load average). It stores the data in a very compact way that will not
expand over time, and it presents useful graphs by processing the data to
enforce a certain data density. It can be used either via simple wrapper
scripts (from shell or Perl) or via frontends that poll network devices and
put a friendly user interface on it.

%package devel
Summary: RRDtool libraries and header files
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
RRD is the Acronym for Round Robin Database. RRD is a system to store and
display time-series data (i.e. network bandwidth, machine-room temperature,
server load average). This package allow you to use directly this library.

%package doc
Summary: RRDtool documentation
Group: Documentation

%description doc
RRD is the Acronym for Round Robin Database. RRD is a system to store and
display time-series data (i.e. network bandwidth, machine-room temperature,
server load average). This package contains documentation on using RRD.

%package -n perl-%{name}
Summary: Perl RRDtool bindings
Group: Development/Languages
Requires: %{name} = %{version}-%{release}
Obsoletes: %{name}-perl <= %{version}
Provides: %{name}-perl = %{version}

%description -n perl-%{name}
The Perl RRDtool bindings

%package -n python-%{name}
Summary: Python RRDtool bindings
Group: Development/Languages
BuildRequires: python
Requires: python >= %{python_version}
Requires: %{name} = %{version}-%{release}

%description -n python-%{name}
Python RRDtool bindings.

%prep
%setup

# Fix to find correct python dir on lib64
%{__perl} -pi -e 's|get_python_lib\(0,0,prefix|get_python_lib\(1,0,prefix|g' \
    configure

# Shouldn't be necessary when using --libdir, but
# introduces hardcoded rpaths where it shouldn't,
# if not done...
%{__perl} -pi.orig -e 's|/lib\b|/%{_lib}|g' \
    configure Makefile.in

%build
%configure \
    --program-prefix="%{?_program_prefix}" \
    --libdir=%{_libdir} \
    --disable-static \
    --with-pic \
    --with-perl-options='INSTALLDIRS="vendor"'

# Fix another rpath issue
%{__perl} -pi.orig -e 's|-Wl,--rpath -Wl,\$rp||g' \
    bindings/perl-shared/Makefile.PL

# Force RRDp bits where we want 'em, not sure yet why the
# --with-perl-options and --libdir don't take
pushd bindings/perl-piped/
%{__perl} Makefile.PL INSTALLDIRS=vendor
%{__perl} -pi.orig -e 's|/lib/perl|/%{_lib}/perl|g' Makefile
popd

%{__make} %{?_smp_mflags}

# Fix @perl@ and @PERL@
find examples/ -type f \
    -exec %{__perl} -pi -e 's|^#! \@perl\@|#!%{__perl}|gi' {} \;
find examples/ -name "*.pl" \
    -exec %{__perl} -pi -e 's|\015||gi' {} \;

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR="$RPM_BUILD_ROOT" install

# Pesky RRDp.pm...
%{__mv} $RPM_BUILD_ROOT%{perl_vendorarch}/../RRDp.pm $RPM_BUILD_ROOT%{perl_vendorarch}/

# We only want .txt and .html files for the main documentation
%{__mkdir_p} doc2/html doc2/txt
%{__cp} -a doc/*.txt doc2/txt/
%{__cp} -a doc/*.html doc2/html/

# Put perl docs in perl package
%{__mkdir_p} doc3/html
%{__mv} doc2/html/RRD*.html doc3/html/

# Clean up the examples
%{__rm} -f examples/Makefile* examples/*.in

# This is so rpm doesn't pick up perl module dependencies automatically
find examples/ -type f -exec chmod 0644 {} \;

# Clean up the buildroot
%{__rm} -rf $RPM_BUILD_ROOT%{_datadir}/doc/%{name}-%{version}/{txt,html}/ \
	$RPM_BUILD_ROOT%{perl_vendorarch}/ntmake.pl \
	$RPM_BUILD_ROOT%{perl_archlib}/perllocal.pod \
        $RPM_BUILD_ROOT%{_datadir}/%{name}/examples \
        $RPM_BUILD_ROOT%{perl_vendorarch}/auto/*/{.packlist,*.bs}

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%{_bindir}/*
%{_libdir}/*.so.*
%{_libdir}/rrdtool/*
%{_datadir}/%{name}/fonts/*
%{_mandir}/man1/*

%files devel
%defattr(-,root,root,-)
%{_includedir}/*.h
%exclude %{_libdir}/*.la
%{_libdir}/*.so

%files doc
%defattr(-,root,root,-)
%doc CHANGES CONTRIBUTORS COPYING COPYRIGHT README TODO NEWS THREADS doc2/html doc2/txt
%doc examples

%files -n perl-%{name}
%defattr(-,root,root,-)
%doc doc3/html
%{_mandir}/man3/*
%{perl_vendorarch}/*.pm
%attr(0755,root,root) %{perl_vendorarch}/auto/RRDs/*

%files -n python-%{name}
%defattr(-,root,root,-)
%doc bindings/python/AUTHORS bindings/python/COPYING bindings/python/README
%{python_sitearch}/rrdtoolmodule.so

%changelog
* Mon Jun 05 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-3
- Merge spec fixes from bz 185909

* Sun Jun 04 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-2
- Remove explicit perl dep, version grabbing using rpm during
  rpmbuild not guaranteed to work (fails on ppc in plague),
  and auto-gen perl deps are sufficient

* Sat Jun 03 2006 Jarod Wilson <jwilson@redhat.com> 1.2.13-1
- Update to release 1.2.13
- Merge spec changes from dag, atrpms and mdk builds
- Additional hacktastic contortions for lib64 & rpath messiness
- Add missing post/postun ldconfig
- Fix a bunch of rpmlint errors
- Disable static libs, per FE guidelines
- Split off docs

* Wed Apr 19 2006 Chris Ricker <kaboom@oobleck.net> 1.2.12-1
- Rev to 1.2

* Fri May 20 2005 Matthias Saou <http://freshrpms.net/> 1.0.49-5
- Include patch from Michael to fix perl module compilation on FC4 (#156242).

* Fri May 20 2005 Matthias Saou <http://freshrpms.net/> 1.0.49-4
- Fix for the php module patch (Joe Pruett, Dag Wieers), #156716.
- Update source URL to new location since 1.2 is now the default stable.
- Don't (yet) update to 1.0.50, as it introduces some changes in the perl
  modules install.

* Mon Jan 31 2005 Matthias Saou <http://freshrpms.net/> 1.0.49-3
- Put perl modules in vendor_perl and not site_perl. #146513

* Thu Jan 13 2005 Matthias Saou <http://freshrpms.net/> 1.0.49-2
- Minor cleanups.

* Thu Aug 25 2004 Dag Wieers <dag@wieers.com> - 1.0.49-1
- Updated to release 1.0.49.

* Wed Aug 25 2004 Dag Wieers <dag@wieers.com> - 1.0.48-3
- Fixes for x86_64. (Garrick Staples)

* Fri Jul  2 2004 Matthias Saou <http://freshrpms.net/> 1.0.48-3
- Actually apply the patch for fixing the php module, doh!

* Thu May 27 2004 Matthias Saou <http://freshrpms.net/> 1.0.48-2
- Added php.d config entry to load the module once installed.

* Thu May 13 2004 Dag Wieers <dag@wieers.com> - 1.0.48-1
- Updated to release 1.0.48.

* Tue Apr 06 2004 Dag Wieers <dag@wieers.com> - 1.0.47-1
- Updated to release 1.0.47.

* Thu Mar  4 2004 Matthias Saou <http://freshrpms.net/> 1.0.46-2
- Change the strict dependency on perl to fix problem with the recent
  update.

* Mon Jan  5 2004 Matthias Saou <http://freshrpms.net/> 1.0.46-1
- Update to 1.0.46.
- Use system libpng and zlib instead of bundled ones.
- Added php-rrdtool sub-package for the php4 module.

* Fri Dec  5 2003 Matthias Saou <http://freshrpms.net/> 1.0.45-4
- Added epoch to the perl dependency to work with rpm > 4.2.
- Fixed the %% escaping in the perl dep.

* Mon Nov 17 2003 Matthias Saou <http://freshrpms.net/> 1.0.45-2
- Rebuild for Fedora Core 1.

* Sun Aug  3 2003 Matthias Saou <http://freshrpms.net/>
- Update to 1.0.45.

* Wed Apr 16 2003 Matthias Saou <http://freshrpms.net/>
- Update to 1.0.42.

* Mon Mar 31 2003 Matthias Saou <http://freshrpms.net/>
- Rebuilt for Red Hat Linux 9.

* Wed Mar  5 2003 Matthias Saou <http://freshrpms.net/>
- Added explicit perl version dependency.

* Sun Feb 23 2003 Matthias Saou <http://freshrpms.net/>
- Update to 1.0.41.

* Fri Jan 31 2003 Matthias Saou <http://freshrpms.net/>
- Update to 1.0.40.
- Spec file cleanup.

* Fri Jul 05 2002 Henri Gomez <hgomez@users.sourceforge.net>
- 1.0.39

* Mon Jun 03 2002 Henri Gomez <hgomez@users.sourceforge.net>
- 1.0.38

* Fri Apr 19 2002 Henri Gomez <hgomez@users.sourceforge.net>
- 1.0.37

* Tue Mar 12 2002 Henri Gomez <hgomez@users.sourceforge.net>
- 1.0.34
- rrdtools include zlib 1.1.4 which fix vulnerabilities in 1.1.3

