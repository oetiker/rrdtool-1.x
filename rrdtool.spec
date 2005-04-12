%define cvsdate cvs-snap
%define cvsver %(echo %{cvsdate} | tr -d -)
%define sover 1.0.0

Summary: Round Robin Database Tools
Name: rrdtool
Version: 1.2rc8
Release: %{cvsver}
License: GPL
Group: Applications/Databases
Source: http://people.ee.ethz.ch/~oetiker/webtools/rrdtool/pub/beta/rrdtool-1.2rc1.tar.gz
URL: http://people.ee.ethz.ch/~oetiker/webtools/rrdtool/
Buildroot: /tmp/%{name}-root

BuildRequires: perl
BuildRequires: cgilib-devel
BuildRequires: freetype-devel libart_lgpl-devel libpng-devel zlib-devel

%description
It is pretty easy to gather status information from all sorts of things,
ranging from the temperature in your office to the number of octets which
have passed through the FDDI interface of your router. But it is not so
trivial to store this data in a efficient and systematic manner. This is
where RRDtool kicks in. It lets you log and analyze the data you gather from
all kinds of data-sources (DS). The data analysis part of RRDtool is based
on the ability to quickly generate graphical representations of the data
values collected over a definable time period.

%package devel
Summary: RRD Tool development libraries and header files
Group: Development/Libraries
Requires: %{name} = %{version}

%description devel
The RRD Tools development library.

%package perl
Summary: RRD Tool Perl interface
Group: Applications/Databases
Requires: %{name} = %{version}

%description perl
The RRD Tools Perl modules.

%prep
%setup -q -n rrdtool-%{cvsdate}

%define deffont %{_datadir}/%{name}/fonts/VeraMono.ttf

%build
CPPFLAGS="-I/usr/include/libart-2.0 -I/usr/include/freetype2"
export CPPFLAGS
%configure
make

%install
rm -rf %{buildroot}
%makeinstall

# Install the font
mkdir -p %{buildroot}%{_datadir}/fonts
install -m 644 src/VeraMono.ttf %{buildroot}%{deffont}

# Fix up the documentation
[ -d docs ] && mv docs docs.src
mv %{buildroot}/usr/doc docs
rm -f docs/*.pod
[ -d examples ] && mv examples examples.src
mv %{buildroot}/usr/examples examples
[ -d html ] && mv html html.src
mv %{buildroot}/usr/html html

# Fix up the perl
%define perlsite %(perl -MConfig -e 'print $Config{"installsitearch"}')
mkdir -p %{buildroot}%{perlsite}
mv %{buildroot}%{_libdir}/perl/* %{buildroot}%{perlsite}
rmdir %{buildroot}%{_libdir}/perl
rm -f %{buildroot}/%{perlsite}/auto/RRDs/RRDs.bs

# Fix up the man pages
if [ "%{_mandir}" != "/usr/share/man" ]; then
	mkdir -p %{buildroot}%{_mandir}
	mv %{buildroot}/usr/man/* %{buildroot}%{_mandir}/
fi

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%clean
rm -rf %{buildroot}

%files
%defattr (-, root, root)
%doc 00README CONTRIBUTORS COPYING COPYRIGHT NEWS PROJECTS
%doc README THREADS TODO
%doc docs/[a-z]* html/[a-z]*
%doc examples/*.cgi
%{_bindir}/rrdcgi
%{_bindir}/rrdtool
%{_bindir}/rrdupdate
%{_libdir}/librrd.so.%{sover}
%{_libdir}/librrd_th.so.%{sover}
%{_mandir}/man1/[a-z]*
%{deffont}

%files devel
%defattr (-, root, root)
%{_includedir}/rrd.h
%{_libdir}/librrd.a
%{_libdir}/librrd.la
%{_libdir}/librrd.so
%{_libdir}/librrd_th.a
%{_libdir}/librrd_th.la
%{_libdir}/librrd_th.so

%files perl
%doc examples/*.pl
%doc docs/RRD* html/RRD*
%defattr (-, root, root)
%{perlsite}/RRDp.pm
%{perlsite}/RRDs.pm
%dir %{perlsite}/auto/RRDs
%{perlsite}/auto/RRDs/RRDs.so
%{_mandir}/man1/RRDp.1*
%{_mandir}/man1/RRDs.1*

%changelog
* Wed May 26 2004 Mike Slifcak <slif@bellsouth.net> 1.1.0-0.1.20040526
- package examples with rrdtool-perl (decouple Perl from main package)
* Thu Apr 29 2004 Chris Adams <cmadams@hiwaay.net> 1.1.0-0.1.20040430
- initial build
