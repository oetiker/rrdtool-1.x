%define cvsdate 2004-04-30
%define cvsver %(echo %{cvsdate} | tr -d -)
%define sover 1.0.2

Summary: Round Robin Database Tools
Name: rrdtool
Version: 1.1.0
Release: 0.1.%{cvsver}
License: GPL
Group: Applications/Networking
Source: http://people.ee.ethz.ch/~oetiker/webtools/rrdtool/pub/beta/rrdtool-cvs-snap.tar.gz
URL: http://people.ee.ethz.ch/~oetiker/webtools/rrdtool/
Buildroot: /tmp/%{name}-root

BuildRequires: perl
BuildRequires: cgilib
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
Requires: rrdtool = %{version}-%{release}

%description devel
The RRD Tools development library.

%package perl
Summary: RRD Tool Perl interface
Group: Applications/Networking
Requires: rrdtool = %{version}-%{release}

%description perl
The RRD Tools Perl modules.

%prep
%setup -q -n rrdtool-%{cvsdate}

mkdir config
cd config
ln -s ../mkinstalldirs .
cd ..

%define deffont %{_datadir}/fonts/VeraMono.ttf
perl -pi -e 's!^(#define\s+RRD_DEFAULT_FONT\s+).*!$1"%{deffont}"!' src/rrd_graph.c

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
[ -d examples ] && mv examples examples.src
mv %{buildroot}/usr/examples examples
[ -d html ] && mv html html.src
mv %{buildroot}/usr/html html

# Fix up the perl
%define perlsite %(perl -MConfig -e 'print $Config{"installsitearch"}')
mkdir -p %{buildroot}%{perlsite}
mv %{buildroot}%{_libdir}/perl/* %{buildroot}%{perlsite}
rmdir %{buildroot}%{_libdir}/perl

# Fix up the man pages
if [ "%{_mandir}" != "/usr/man" ]; then
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
%doc 00README CONTRIBUTORS COPYING COPYRIGHT ChangeLog NEWS PROJECTS
%doc README THREADS TODO examples
%doc docs examples html
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
%defattr (-, root, root)
%{perlsite}/RRDp.pm
%{perlsite}/RRDs.pm
%dir %{perlsite}/auto/RRDs
%{perlsite}/auto/RRDs/RRDs.bs
%{perlsite}/auto/RRDs/RRDs.so
%{_mandir}/man1/RRDp.1*
%{_mandir}/man1/RRDs.1*

%changelog
* Thu Apr 29 2004 Chris Adams <cmadams@hiwaay.net> 1.1.0-0.1.20040430
- initial build
