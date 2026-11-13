#!/bin/bash
# Exercise the build system beyond "configure && make check": the tarball,
# out-of-tree builds, install/uninstall, the optional features and the
# configure options whose effect is only visible in what gets installed.
#
# Usage: conftools/check-buildsystem.sh [step...]
#   steps: distcheck disabled tarball-daemon install options
#          perl-standalone flags lua
#   (default: all, in that order)
#
# Run from the top of a git checkout that has been bootstrapped and
# configured with the bindings to test (CI: --disable-lua --disable-tcl).
# The other builds are made out of tree from the tarball distcheck
# leaves, so they also test what is shipped.
# Needs perl, python3 with setuptools/wheel/pip, ruby with mkmf, groff,
# ghostscript and pkg-config. The Lua check runs when a versioned
# interpreter is installed, with its development files.
# Everything is written below ./_check, which is removed first.

set -eu

TOP=$(pwd)
W=$TOP/_check
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
NOBIND="--disable-perl --disable-python --disable-ruby --disable-lua --disable-tcl"

step () { echo; echo "=== $*"; }
fail () { echo "FAILED: $*" >&2; exit 1; }

# the tarball's source, unpacked once below $W/src
SRC=
source_tree () {
    [ -n "$SRC" ] && return
    ls "$TOP"/rrdtool-*.tar.gz >/dev/null 2>&1 || make -s -C "$TOP" dist >/dev/null
    rm -rf "$W/src"; mkdir -p "$W/src"
    tar -C "$W/src" -xzf "$TOP"/rrdtool-*.tar.gz
    SRC=$(echo "$W"/src/rrdtool-*)
}

# configure the tarball's source into directory $1 with the remaining args
build () {
    local dir=$1; shift
    source_tree
    rm -rf "$dir"; mkdir -p "$dir"
    (cd "$dir" && "$SRC/configure" -q "$@" >configure.out 2>&1 \
        || { cat configure.out; exit 1; }
     make -s -j"$JOBS" >make.out 2>&1 || { tail -50 make.out; exit 1; })
}

check_suite () {
    local dir=$1; shift
    (cd "$dir" && make -s check -j"$JOBS" "$@" >check.out 2>&1) || {
        cat "$dir/tests/test-suite.log"; fail "make check $* in $dir"; }
    grep -E '^# (PASS|SKIP|FAIL)' "$dir/tests/test-suite.log" | tr '\n' ' '; echo
}

do_distcheck () {
    step "make distcheck (with the bindings this tree was configured with)"
    make -s -j"$JOBS" >/dev/null
    make -j"$JOBS" distcheck \
        DISTCHECK_CONFIGURE_FLAGS="--disable-lua --disable-tcl" >"$W/distcheck.out" 2>&1 \
        || { tail -80 "$W/distcheck.out"; fail "make distcheck"; }
    grep 'archives ready' "$W/distcheck.out"
    # the tarball must not depend on how this tree was configured
    tar tzf rrdtool-*.tar.gz >"$W/tarball.list"
    ! grep -qE '/src/rrd_config\.h$|/libtool$' "$W/tarball.list" \
        || fail "the tarball ships generated files"
    for page in rrdlua rrdpython rrdruby rrdrados rrdgraph_libdbi; do
        grep -q "/doc/$page.1\$" "$W/tarball.list" || fail "tarball lacks doc/$page.1"
    done
}

do_disabled () {
    for opt in --disable-rrd_restore --disable-rrdcached --disable-rrd_graph; do
        step "make check with $opt"
        build "$W/b$opt" $NOBIND $opt
        check_suite "$W/b$opt"
    done
    nm -D "$W/b--disable-rrd_restore/src/.libs/librrd.so" | grep -qw 'T rrd_restore' \
        || fail "--disable-rrd_restore does not export rrd_restore"
}

do_tarball_daemon () {
    step "rrdcached test styles, out of tree, from the tarball"
    build "$W/t" $NOBIND
    for style in rrdcached-unix rrdcached-tcp; do
        echo "-- $style"; check_suite "$W/t" TESTS_STYLE=$style
    done
    (cd "$W/t" && make -s distclean >/dev/null)
    [ ! -e "$W/t/tests/cached" ] || fail "distclean left tests/cached"
}

do_install () {
    step "install and uninstall into one DESTDIR"
    python3 "$TOP/bindings/python/tests/test_uninstall.py" \
        || fail "Python uninstall record checks"
    local d=$W/destdir prefix
    prefix=$(sed -n 's/^prefix = //p' Makefile)
    rm -rf "$d"
    # Shared-prefix installs must leave other packages and Perl's shared
    # installation record alone, including when bindings are disabled.
    local perldir pyother
    perldir="$d$prefix/lib/perl/$(perl -MConfig -e 'print "$Config{version}/$Config{archname}"')"
    pyother="$d$prefix/lib/python3/site-packages/rrdtool_unrelated"
    mkdir -p "$perldir" "$pyother"
    echo 'another Perl module' >"$perldir/perllocal.pod"
    echo 'another Python package' >"$pyother/__init__.py"
    make -s install DESTDIR="$d" >"$W/install.out" 2>&1 \
        || { tail -40 "$W/install.out"; fail "make install"; }
    grep -qx 'another Perl module' "$perldir/perllocal.pod" \
        || fail "install changed the shared Perl record"
    find "$d" -type f | sort >"$W/installed-files"
    make -s -C bindings uninstall-local DESTDIR="$d" \
        COMP_PERL= COMP_PYTHON= COMP_RUBY= || fail "uninstall disabled bindings"
    find "$d" -type f | sort >"$W/disabled-uninstall-files"
    cmp "$W/installed-files" "$W/disabled-uninstall-files" \
        || fail "disabled bindings removed installed files"
    local libdir="$d$prefix/lib"
    if grep -q '^COMP_PERL = perl' bindings/Makefile; then
        PERL5LIB=$(dirname "$(find "$d" -name RRDs.pm | head -1)") LD_LIBRARY_PATH=$libdir \
            perl -MRRDs -e 'exit 0' || fail "installed RRDs does not load"
    fi
    if grep -q '^COMP_PYTHON = python' bindings/Makefile; then
        PYTHONPATH=$(dirname "$(find "$d" -name 'rrdtool*.so' | head -1)") LD_LIBRARY_PATH=$libdir \
            python3 -c 'import rrdtool' || fail "installed python module does not load"
    fi
    if grep -q '^COMP_RUBY = ruby' bindings/Makefile; then
        local rb; rb=$(find "$d" -name RRD.so | head -1)
        case "$rb" in "$d$prefix"/*) ;; *) fail "RRD.so is outside the prefix: $rb";; esac
        RUBYLIB=$(dirname "$rb") LD_LIBRARY_PATH=$libdir ruby -e 'require "RRD"' \
            || fail "installed ruby module does not load"
    fi
    make -s uninstall DESTDIR="$d" >"$W/uninstall.out" 2>&1 || fail "make uninstall"
    grep -qx 'another Perl module' "$perldir/perllocal.pod" \
        || fail "uninstall changed the shared Perl record"
    grep -qx 'another Python package' "$pyother/__init__.py" \
        || fail "uninstall removed another Python package"
    rm -f "$perldir/perllocal.pod" "$pyother/__init__.py"
    local left; left=$(find "$d" -type f -o -type l)
    [ -z "$left" ] || fail "make uninstall left: $left"
    echo "install, load and uninstall: ok"
}

do_options () {
    step "--htmldir, RRDTXTDIR, --enable-python-site-install, --localedir"
    build "$W/opt" --prefix=/usr --htmldir=/usr/doc/x/html RRDTXTDIR=/usr/doc/x/txt \
        --localedir=/usr/share/locale-x --enable-python-site-install \
        --disable-perl --disable-ruby --disable-lua --disable-tcl
    rm -rf "$W/opt-d"
    (cd "$W/opt" && make -s install DESTDIR="$W/opt-d" >/dev/null)
    [ -f "$W/opt-d/usr/doc/x/html/rrdtool.html" ] || fail "--htmldir not honoured"
    [ -f "$W/opt-d/usr/doc/x/txt/rrdtool.txt" ] || fail "RRDTXTDIR not honoured"
    local pyexec
    pyexec=$(printf 'print-pyexecdir:\n\t@echo $(pyexecdir)\n' \
        | make -s -C "$W/opt/bindings" -f Makefile -f - print-pyexecdir)
    ls "$W/opt-d$pyexec"/rrdtool*.so >/dev/null 2>&1 \
        || fail "python module not in pyexecdir ($pyexec)"
    grep -q 'LOCALEDIR="\\"$(localedir)\\""' "$W/opt/src/Makefile" \
        && grep -q '^localedir = /usr/share/locale-x' "$W/opt/src/Makefile" \
        || fail "--localedir does not reach the program"
    (cd "$W/opt" && make -s uninstall DESTDIR="$W/opt-d" >uninstall.out 2>&1) \
        || fail "uninstall with --enable-python-site-install"
    [ -z "$(find "$W/opt-d" -type f -o -type l)" ] \
        || fail "site install left files after uninstall"
    echo "options: ok"
}

do_perl_standalone () {
    step "Perl RRDs built standalone against an installed librrd"
    build "$W/inst-b" --prefix="$W/inst" $NOBIND --disable-docs
    (cd "$W/inst-b" && make -s install >/dev/null)
    rm -rf "$W/ps"; cp -r "$TOP/bindings/perl-shared" "$W/ps"
    (cd "$W/ps" && rm -f Makefile && PKG_CONFIG_PATH="$W/inst/lib/pkgconfig" perl Makefile.PL >/dev/null \
        && make >make.out 2>&1) || { cat "$W/ps/make.out"; fail "standalone perl build"; }
    (cd "$W/ps" && LD_LIBRARY_PATH="$W/inst/lib" perl -Mblib -MRRDs -e '
        RRDs::create("x.rrd", "DS:a:GAUGE:600:U:U", "RRA:AVERAGE:0.5:1:5");
        die RRDs::error() if RRDs::error(); -s "x.rrd" or die "no file\n"') \
        || fail "standalone RRDs does not work"
    echo "standalone perl: ok"
}

do_flags () {
    step "PKG_CONFIG, CFLAGS order, Perl option quoting"
    mkdir -p "$W/pc"
    cat >"$W/pc/pkg-config" <<EOF
#!/bin/sh
echo "\$@" >>"$W/pc/log"
exec $(command -v pkg-config) "\$@"
EOF
    chmod +x "$W/pc/pkg-config"; rm -f "$W/pc/log"
    build "$W/flags" $NOBIND --disable-docs PKG_CONFIG="$W/pc/pkg-config" CFLAGS="-O1 -std=c11"
    grep -q -- '--exists glib-2.0' "$W/pc/log" || fail "PKG_CONFIG was not used"
    local line; line=$(cd "$W/flags/src" && touch "$SRC/src/rrd_version.c" && make V=1 2>&1 | grep -m1 -- 'rrd_version.c')
    # the user's -std wins: configure adds no -std of its own then
    [ "$(echo "$line" | grep -o -- '-std=[a-z0-9]*' | sort -u)" = "-std=c11" ] \
        || fail "the user's -std=c11 is not the only -std: $line"
    [ "$(echo "$line" | grep -o -- '-pedantic' | wc -l)" = 1 ] || fail "flags are duplicated: $line"
    build "$W/perlq" --disable-python --disable-ruby --disable-lua --disable-tcl --disable-docs \
        PERLCC=cc PERLCCFLAGS="-O2 -g"
    (cd "$W/perlq" && make -s -C bindings perl-shared >/dev/null 2>&1)
    grep -q '^CCFLAGS = -O2 -g$' "$W/perlq/bindings/perl-shared/Makefile" || fail "PERLCCFLAGS split"
    echo "flags: ok"
}

do_lua () {
    step "a versioned Lua interpreter and its pkg-config flags"
    local lua= binary
    for binary in lua5.4 lua5.3 lua5.2 lua5.1; do
        lua=$(command -v "$binary" || true)
        [ -z "$lua" ] || break
    done
    [ -n "$lua" ] || { echo "no versioned Lua interpreter; skipped"; return; }
    build "$W/lua" --disable-perl --disable-python --disable-ruby \
        --disable-tcl --disable-docs LUA="$lua"
    (cd "$W/lua/bindings/lua" && \
        LUA_CPATH="$W/lua/bindings/lua/.libs/?.so;;" \
        LD_LIBRARY_PATH="$W/lua/src/.libs" make -s test >test.out 2>&1) \
        || { cat "$W/lua/bindings/lua/test.out"; fail "Lua binding test"; }
    echo "versioned Lua: ok"
}

rm -rf "$W"; mkdir -p "$W"
steps=${*:-distcheck disabled tarball-daemon install options perl-standalone flags lua}
for s in $steps; do
    case $s in
        distcheck) do_distcheck ;;
        disabled) do_disabled ;;
        tarball-daemon) do_tarball_daemon ;;
        install) do_install ;;
        options) do_options ;;
        perl-standalone) do_perl_standalone ;;
        flags) do_flags ;;
        lua) do_lua ;;
        *) fail "unknown step $s" ;;
    esac
done
echo; echo "all build-system checks passed"
