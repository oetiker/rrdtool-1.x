%define name rrdtool
%define ver 1.0.21
%define extension tar.gz

Summary: Round Robin Database Tools
Name: %name
Version: %{ver}
Release: 2
Copyright: GPL
Group: Applications/Networking
Source: %{name}-%{ver}.%{extension}
Patch0: rrdtool-perldestdir.patch
Patch1: rrdtool-tcldestdir.patch
URL: http://ee-staff.ethz.ch/~oetiker/webtools/rrdtool/
Buildroot: /tmp/%{name}-%{ver}-root

%description
It is pretty easy to gather status information from all sorts of things,
ranging from the temperature in your office to the number of octets which
have passed through the FDDI interface of your router. But it is not so
trivial to store this data in a efficient and systematic manner. This is
where RRDtool kicks in. It lets you log and analyze the data you gather from
all kinds of data-sources (DS). The data analysis part of RRDtool is based
on the ability to quickly generate graphical representations of the data
values collected over a definable time period.

%prep
%setup
%patch0 -p1
%patch1 -p1
%build
./configure --with-tcllib=/usr/lib --prefix=/usr
make

%install
make install DESTDIR=${RPM_BUILD_ROOT}
# install tcl interface...
make site-tcl-install DESTDIR=${RPM_BUILD_ROOT}
# rpm uses /doc for its file restructuring...
mv ${RPM_BUILD_ROOT}/usr/doc ${RPM_BUILD_ROOT}/usr/txt

%clean
rm -rf $RPM_BUILD_ROOT

%files
%doc CHANGES CONTRIBUTORS COPYING COPYRIGHT NT-BUILD-TIPS.txt README TODO 
%doc ${RPM_BUILD_ROOT}/usr/contrib/
%doc ${RPM_BUILD_ROOT}/usr/txt/
%doc ${RPM_BUILD_ROOT}/usr/examples/
%doc ${RPM_BUILD_ROOT}/usr/html/
/usr/man/
/usr/bin/
/usr/lib/

