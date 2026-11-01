# RPM spec for the upstream `/opt/rrdtool` build of RRDtool.
#
# This is intentionally NOT a drop-in replacement for the FHS-compliant
# distribution package. It installs everything (binaries, library, headers,
# Perl/Python/Tcl/Lua/Ruby bindings) under /opt/rrdtool and coexists
# with the distro's `rrdtool` package without touching the system.
#
# @VERSION@ is substituted by the release workflow before invoking rpmbuild.

# Our binaries carry a deliberate RPATH of /opt/rrdtool/lib — that is how
# they locate librrd without any system linker config. RHEL's check-rpaths
# QA script rejects every non-standard RPATH and fails the build, so it
# must be disabled for this package.
#
# On RHEL/EL the check is wired through %%__arch_install_post (not the
# Fedora %%__brp_check_rpaths hook), so redefine that hook to keep only the
# buildroot-leak check and drop check-rpaths. Both lines together cover
# RHEL and Fedora.
%global __arch_install_post /usr/lib/rpm/check-buildroot
%global __brp_check_rpaths %{nil}

Name:           rrdtool-1-opt
Version:        @VERSION@
Release:        1%{?dist}
Summary:        Round Robin Database Tool (upstream /opt build)

License:        GPL-2.0-or-later WITH FLOSS-exception-1.0
URL:            https://oss.oetiker.ch/rrdtool/
Source0:        rrdtool-%{version}.tar.gz
Source1:        rrdtool-env.sh

Prefix:         /opt/rrdtool

# Disable RPM's automatic dependency scanner: it would scan our /opt-rooted
# binaries' RPATH and emit Requires like `librrd.so.X()(64bit)` that only
# the /opt-installed librrd can satisfy, breaking install on hosts that
# already have the distro `rrdtool` package present.
AutoReqProv:    no

BuildRequires:  gcc, make, autoconf, automake, libtool, pkgconfig
BuildRequires:  groff, gettext, gettext-devel, intltool
BuildRequires:  cairo-devel >= 1.2, pango-devel >= 1.14
BuildRequires:  freetype-devel, libpng-devel, zlib-devel
BuildRequires:  libxml2-devel, glib2-devel, libdbi-devel
# binding build-deps
BuildRequires:  perl-devel, perl-ExtUtils-MakeMaker
BuildRequires:  python3-devel, python3-setuptools, python3-pip, python3-wheel
BuildRequires:  tcl-devel
BuildRequires:  lua-devel
BuildRequires:  ruby, ruby-devel

# Runtime: explicit because AutoReqProv is off. Listed without versions on
# purpose so the same spec works across el8/el9/fedora.
Requires:       cairo, pango, libxml2, libpng, freetype, libdbi, zlib, glib2

%description
RRDtool is the OpenSource industry standard high performance data
logging and graphing system for time series data.

This package installs RRDtool entirely under /opt/rrdtool so it does not
conflict with the distribution-provided rrdtool package. To make it
discoverable from your shell:

    . /opt/rrdtool/bin/rrdtool-env.sh

That puts /opt/rrdtool/bin on PATH and makes the bundled language
bindings (Perl, Python, Tcl, Lua, Ruby) findable by their interpreters.

For C/C++ consumers, set PKG_CONFIG_PATH=/opt/rrdtool/lib/pkgconfig and
compile with `pkg-config --cflags --libs librrd`; the resulting binary
gets /opt/rrdtool/lib baked in via -Wl,-rpath, so no system linker
config (ld.so.conf) is required.

Language bindings are present on disk but their interpreters (perl,
python3, tcl, lua, ruby) are NOT pulled in as hard dependencies of this
package — install them yourself if you want to use them.

%prep
%setup -q -n rrdtool-%{version}

%build
./configure \
    --prefix=/opt/rrdtool \
    --sysconfdir=/opt/rrdtool/etc \
    --localstatedir=/opt/rrdtool/var \
    --datarootdir=/opt/rrdtool/share \
    --mandir=/opt/rrdtool/share/man \
    --disable-static \
    --with-pic
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

# Ruby's mkmf installs the compiled extension into the interpreter's own
# sitearchdir (/usr/local/lib*/ruby/site_ruby), ignoring our --prefix — so
# RRD.so lands outside /opt and would trip rpmbuild's unpackaged-files check.
# Relocate it under /opt/rrdtool/lib/ruby; rrdtool-env.sh puts that directory
# on RUBYLIB.
rrd_rb=$(find %{buildroot} -name RRD.so -path '*ruby*')
if [ -z "$rrd_rb" ]; then
    echo "ruby binding RRD.so not found in buildroot" >&2
    exit 1
fi
install -d %{buildroot}/opt/rrdtool/lib/ruby
# configure now installs it there itself; older trees did not
[ "$rrd_rb" = %{buildroot}/opt/rrdtool/lib/ruby/RRD.so ] \
    || mv "$rrd_rb" %{buildroot}/opt/rrdtool/lib/ruby/RRD.so
# Drop the now-empty system tree the ruby binding escaped into.
rm -rf %{buildroot}/usr

# Bake the rpath into librrd.pc's Libs: line so consumers get -Wl,-rpath
# embedded in their binaries via `pkg-config --libs librrd`. This is only
# done in the /opt build; src/librrd.pc.in stays untouched upstream so
# FHS-canonical builds aren't affected.
sed -i 's|^Libs: -L\${libdir} -lrrd$|Libs: -L${libdir} -lrrd -Wl,-rpath,${libdir}|' \
    %{buildroot}/opt/rrdtool/lib/pkgconfig/librrd.pc

# Sourceable env-helper. Rendered by the workflow into Source1.
install -m 0755 %{SOURCE1} %{buildroot}/opt/rrdtool/bin/rrdtool-env.sh

%files
/opt/rrdtool

%changelog
# Per-release entries are intentionally omitted: the canonical release
# notes live in CHANGES at the top level of the source tarball.
