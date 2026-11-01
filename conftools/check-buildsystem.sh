#!/bin/bash
# Exercise the build system beyond "configure && make check": the tarball,
# out-of-tree builds, install/uninstall, the optional features and the
# configure options whose effect is only visible in what gets installed.
#
# Usage: conftools/check-buildsystem.sh [step...]
#   steps: distcheck reproducible sbom disabled tarball-daemon install
#          options perl-standalone flags lua systemd
#   (default: all but systemd, in that order)
#
# Run from the top of a git checkout that has been bootstrapped and
# configured with the bindings to test (CI: --disable-lua --disable-tcl).
# The other builds are made out of tree from the tarball distcheck
# leaves, so they also test what is shipped.
# Needs perl, python3 with setuptools/wheel/pip, ruby with mkmf, groff,
# ghostscript and pkg-config. The Lua check runs when a versioned
# interpreter is installed, with its development files. The
# systemd-analyze tool is used when present. The
# systemd step installs and starts the units, so it needs root and a
# running systemd (CI runners and test VMs, not containers).
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

do_reproducible () {
    step "the release tarball is reproducible"
    # the steps release.yml runs to make the tarball, twice
    local epoch; epoch=$(git -C "$TOP" log -1 --format=%ct 2>/dev/null || date +%s)
    local i
    for i in 1 2; do
        rm -rf "$W/r$i"; git -C "$TOP" worktree add -q --detach "$W/r$i" HEAD 2>/dev/null \
            || { mkdir -p "$W/r$i"; git -C "$TOP" archive HEAD | tar -C "$W/r$i" -xf -; }
        (cd "$W/r$i" && ./bootstrap >/dev/null 2>&1 && ./configure -q $NOBIND >/dev/null \
         && export SOURCE_DATE_EPOCH=$epoch \
                   TAR_OPTIONS="--sort=name --mtime=@$epoch --owner=0 --group=0 --numeric-owner" \
                   GZIP_ENV="--best --no-name" \
         && make -s -C po update-po >/dev/null 2>&1 \
         && pd=$(date -u -d "@$epoch" '+%Y-%m-%d %H:%M+0000' 2>/dev/null || date -u -r "$epoch" '+%Y-%m-%d %H:%M+0000') \
         && sed -i.bak "s/^\"POT-Creation-Date: .*\\\\n\"\$/\"POT-Creation-Date: $pd\\\\n\"/" po/*.po po/*.pot \
         && rm -f po/*.bak \
         && make -s dist DIST_DEPENDS_ON_UPDATE_PO=no >/dev/null 2>&1) || fail "make dist $i"
        sha256sum "$W/r$i"/rrdtool-*.tar.gz | cut -d' ' -f1 >"$W/r$i.sum"
        git -C "$TOP" worktree remove --force "$W/r$i" 2>/dev/null || true
    done
    cmp -s "$W/r1.sum" "$W/r2.sum" || fail "two release tarballs differ"
    echo "reproducible: $(cat "$W/r1.sum")"
}

do_sbom () {
    step "release SBOMs"
    source_tree
    python3 "$TOP/conftools/release_sbom.py" source --tarball "$TOP"/rrdtool-*.tar.gz \
        --version "$(cat "$TOP/VERSION")" --output "$W/source.spdx.json" \
        || fail "release_sbom.py source"
    python3 - "$W/source.spdx.json" <<'PY' || fail "source SBOM content"
import json, sys
d = json.load(open(sys.argv[1]))
assert d["spdxVersion"] == "SPDX-2.3", d["spdxVersion"]
names = {p["name"] for p in d["packages"]}
assert {"rrdtool", "compat-5.1"} <= names, names
print("source SBOM:", len(d["packages"]), "packages")
PY
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
    local unit; unit=$(find "$d" -name rrdcached.service | head -1)
    if [ -n "$unit" ]; then
        case "$unit" in "$d$prefix"/*) ;; *) fail "unit outside the prefix: $unit";; esac
        grep -q "^ExecStart=$prefix/bin/rrdcached " "$unit" || fail "ExecStart: $(grep ExecStart "$unit")"
        if command -v systemd-analyze >/dev/null; then
            mkdir -p "$W/units"
            sed "s|^ExecStart=$prefix|ExecStart=$d$prefix|" "$unit" >"$W/units/rrdcached.service"
            cp "$(dirname "$unit")/rrdcached.socket" "$W/units/"
            (cd "$W/units" && systemd-analyze verify --man=no ./rrdcached.socket ./rrdcached.service) \
                || fail "systemd-analyze verify"
        fi
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

SE_MMAP=
sd_cleanup () {
    systemctl stop rrdcached.service rrdcached.socket 2>/dev/null || true
    if [ -n "$SE_MMAP" ]; then
        setsebool domain_can_mmap_files "$SE_MMAP"
        semanage fcontext -d '/var/lib/rrdcached(/.*)?' 2>/dev/null || true
        rm -f /usr/bin/rrdcached
        SE_MMAP=
    fi
    systemctl reset-failed rrdcached.service rrdcached.socket 2>/dev/null || true
    rm -f /etc/systemd/system/rrdcached.service /etc/systemd/system/rrdcached.socket
    systemctl daemon-reload
    rm -rf /opt/rrdtool-check /var/lib/rrdcached /tmp/outside.rrd
}

# The same through the distribution's SELinux policy: as /usr/bin/rrdcached
# the daemon runs in rrdcached_t, with the label and boolean rrdcached(1)
# asks for. Changes to the policy are undone by sd_cleanup.
sd_confined () {
    semodule -l 2>/dev/null | grep -qw rrdcached || { echo "no rrdcached SELinux module"; return; }
    command -v semanage >/dev/null || fail "semanage is needed for the SELinux check"
    [ ! -e /usr/bin/rrdcached ] || { echo "/usr/bin/rrdcached exists, confined check skipped"; return; }
    systemctl stop rrdcached.service rrdcached.socket
    systemctl reset-failed rrdcached.service rrdcached.socket 2>/dev/null || true
    SE_MMAP=$(getsebool domain_can_mmap_files | awk '{print $3}')
    install -m 755 /opt/rrdtool-check/bin/rrdcached /usr/bin/rrdcached
    restorecon /usr/bin/rrdcached
    sed -i 's|^ExecStart=/opt/rrdtool-check/bin/rrdcached|ExecStart=/usr/bin/rrdcached|' \
        /etc/systemd/system/rrdcached.service
    systemctl daemon-reload
    semanage fcontext -a -t rrdcached_var_run_t '/var/lib/rrdcached(/.*)?'
    setsebool domain_can_mmap_files 1
    rm -rf /var/lib/rrdcached
    local n0; n0=$(grep -c 'avc:  denied' /var/log/audit/audit.log 2>/dev/null || echo 0)
    systemctl start rrdcached.socket
    local r=/opt/rrdtool-check/bin/rrdtool sock=unix:/run/rrdcached/rrdcached.sock
    local f=/var/lib/rrdcached/db/confined.rrd
    $r create --daemon $sock $f --step 60 DS:a:GAUGE:120:U:U RRA:LAST:0.5:1:10 || fail "confined create"
    $r update --daemon $sock $f N:1 || fail "confined update"
    $r flushcached --daemon $sock $f || fail "confined flush"
    $r fetch --daemon $sock $f LAST --start -10min >/dev/null || fail "confined fetch"
    ps -eZ | grep -q 'rrdcached_t.*rrdcached$' || { ps -eZ | grep rrdcached; fail "rrdcached is not in rrdcached_t"; }
    local n1; n1=$(grep -c 'avc:  denied' /var/log/audit/audit.log 2>/dev/null || echo 0)
    [ "$n1" = "$n0" ] || { grep 'avc:  denied' /var/log/audit/audit.log | tail -5; fail "SELinux denied the confined rrdcached"; }
    echo "SELinux enforcing, rrdcached_t: no denials"
}

do_systemd () {
    step "the shipped systemd units serve rrdcached"
    [ "$(id -u)" = 0 ] || fail "the systemd step needs root"
    [ -d /run/systemd/system ] || fail "systemd is not running"
    local prefix=/opt/rrdtool-check sock=/run/rrdcached/rrdcached.sock
    local base=/var/lib/rrdcached/db r
    build "$W/sd" --prefix=$prefix $NOBIND --disable-docs --with-systemdsystemunitdir=/etc/systemd/system
    (cd "$W/sd" && make -s install >/dev/null)
    trap 'journalctl -u rrdcached.service -n 20 --no-pager; sd_cleanup' EXIT
    command -v restorecon >/dev/null && restorecon -R $prefix /etc/systemd/system/rrdcached.* 2>/dev/null || true
    systemctl daemon-reload
    local since; since=$(date '+%m/%d/%Y %H:%M:%S')
    systemctl start rrdcached.socket || fail "rrdcached.socket does not start"
    [ "$(stat -c %a $sock)" = 660 ] || fail "socket mode is $(stat -c %a $sock)"
    r=$prefix/bin/rrdtool
    # the client sends absolute paths, so name files below the base directory
    local f=$base/check.rrd
    $r create --daemon unix:$sock $f --start 1000000000 --step 60 \
        DS:a:GAUGE:120:U:U RRA:LAST:0.5:1:10 || fail "create through the socket"
    $r update --daemon unix:$sock $f 1000000060:1 1000000120:2 || fail "update through the socket"
    $r flushcached --daemon unix:$sock $f || fail "flush through the socket"
    local out; out=$($r fetch --daemon unix:$sock $f LAST --start 1000000000 --end 1000000120) \
        || fail "fetch through the socket"
    echo "$out" | grep -qE '^[0-9]+: [0-9]' || { echo "$out"; fail "fetch through the socket returned no data"; }
    systemctl is-active -q rrdcached.service || fail "socket activation did not start the service"
    [ -s $f ] || fail "$f was not written"
    # -B: no writes outside the base directory; -O: no overwrite on CREATE
    if $r create --daemon unix:$sock /tmp/outside.rrd DS:a:GAUGE:120:U:U RRA:LAST:0.5:1:10 2>/dev/null \
       || [ -e /tmp/outside.rrd ]; then fail "rrdcached wrote outside its base directory"; fi
    local before; before=$(sha256sum $f)
    if $r create --daemon unix:$sock $f DS:a:GAUGE:120:U:U RRA:LAST:0.5:1:10 2>/dev/null \
       || [ "$(sha256sum $f)" != "$before" ]; then fail "CREATE overwrote an existing file"; fi
    if command -v getenforce >/dev/null && [ "$(getenforce)" = Enforcing ]; then
        local avc; avc=$(ausearch -m AVC,USER_AVC -ts "$since" 2>/dev/null | grep -i rrdcached || true)
        [ -z "$avc" ] || { echo "$avc"; fail "SELinux denied rrdcached"; }
        echo "SELinux enforcing, unconfined binary: no denials"
        sd_confined
    fi
    trap - EXIT
    sd_cleanup
    echo "systemd units: ok"
}

rm -rf "$W"; mkdir -p "$W"
steps=${*:-distcheck reproducible sbom disabled tarball-daemon install options perl-standalone flags lua}
for s in $steps; do
    case $s in
        distcheck) do_distcheck ;;
        reproducible) do_reproducible ;;
        sbom) do_sbom ;;
        systemd) do_systemd ;;
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
