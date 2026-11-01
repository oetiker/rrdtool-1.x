AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CFLAGS = $(RRD_CFLAGS)

script_tests = modify1 modify2 modify3 modify4 modify5 \
	tune1 tune2 graph1 graph2 graph-missing-ds rpn1 \
	rpn2 rrdcreate dump-restore create-with-source-1 create-with-source-2 \
	create-with-source-3 create-with-source-4 create-with-source-and-mapping-1 create-from-template-1 dcounter1 \
	vformatter1 xport1 xport2 xport3 list1 \
	pdp-calc1 graph3 graph4 graph5 graph6 graph7 info-cached1 xport4 epoch1 modify6 cached-path1 cached-lastupdate1 security-header-counts \
	security-fetch-arithmetic security-fetch-daemon-arithmetic cached-sockpath1 cached-update1 info-cached2 graph-cairo-error \
	cached-client-api cached-journal-long create-with-source-5 create-no-overwrite functions-report cached-journal-crash cached-first-last first-last-error graph-incomplete-file cached-long-line graph-export-open cached-last-exact cached-update-skip-past graph-lazy functions-fail

TESTS = $(script_tests) compat-cloexec rrd-open-security tune-legacy-version tune-daemon-error \
	client-header-standalone tune-legacy-aberrant

script_tests += security-rrdcached-response-bounds
script_tests += restore-xml-parse-error
script_tests += update-compute-only
script_tests += security-create-source-prefill
script_tests += security-hw-smoothing-window hw-bulk-update-period1
script_tests += security-hw-dependencies security-compute-ds security-header-fields
script_tests += security-daemon-tune-argc

script_tests += security-time-fields
script_tests += security-client-response-validation

EXTRA_DIST = Makefile.am \
	functions $(script_tests) \
	modify-test1.create.dump modify-test1.mod1.dump \
	modify2-testa-create.dump modify2-testb-mod1.dump modify2-testc-mod1.dump \
	modify-test3.create.dump modify-test3.mod1.dump \
	modify4-testa-create.dump modify4-testa1-create.dump modify4-testa2-create.dump \
	modify5-testa1-create.dump modify5-testa2-create.dump \
	rrdcreate \
	tune1-testa-mod1.dump tune1-testa-mod2.dump tune1-testorg.dump \
	tune2-testa-mod1.dump tune2-testorg.dump \
	valgrind-supressions dcounter1 dcounter1.output graph1.output graph2.output vformatter1 rpn1.output rpn2.output \
	xport1.json.output xport1.xml.output xport2 xport3 xport4 graph3 graph4 graph5 graph6 graph7 info-cached1 \
	pdp-calc1 pdp-calc1-1-avg-60.output pdp-calc1-1-avg-300.output pdp-calc1-1-max-300.output

# NB: AM_TESTS_ENVIRONMENT not available until automake 1.12
AM_TESTS_ENVIRONMENT = \
	BASEDIR=${abs_srcdir} ; export BASEDIR ; \
	BUILDDIR=${abs_builddir} ; export BUILDDIR ; \
	TOP_BUILDDIR=${abs_top_builddir} ; export TOP_BUILDDIR ;

CLEANFILES = *.rrd info-cached1.xml \
	ct.out dur.out graph1.out graph1.output.out graph2.output.out \
	graph4.pdf graph5-full.pdf graph5-clipped.pdf graph5-angle.pdf \
	graph6.svg graph6-stdout.svg graph6-zoom.svg graph6-nogridfit.svg \
	graph7-s.pdf graph7-S.pdf graph7-pct.pdf graph7-r.pdf \
	modify5-testa1-mod.dump modify5-testa2-mod.dump \
	modify5-testa1-mod.dump.tmp modify5-testa2-mod.dump.tmp \
	rpn1.out rpn1.output.out create-with-source-5-test.png

check_PROGRAMS = \
	compat-cloexec \
	rrd-open-security \
	lastupdate-v \
	tune-legacy-version \
	tune-daemon-error \
	tune-legacy-aberrant \
	client-header-standalone

# compile-only: includes nothing but the installed rrd_client.h
client_header_standalone_SOURCES = test_client_header_standalone.c

compat_cloexec_SOURCES = \
	test_compat-cloexec.c \
	src-compat-cloexec.c

rrd_open_security_SOURCES = \
	test_rrd_open_security.c

rrd_open_security_LDADD = \
	${top_builddir}/src/librrd.la

# helper for cached-lastupdate1; links the shared librrd to check the export
lastupdate_v_SOURCES = test_lastupdate_v.c
lastupdate_v_LDADD = ${top_builddir}/src/librrd.la

tune_legacy_version_SOURCES = \
	test_tune_legacy_version.c

tune_legacy_version_LDADD = \
	${top_builddir}/src/librrd.la

tune_daemon_error_SOURCES = \
	test_tune_daemon_error.c

tune_daemon_error_LDADD = \
	${top_builddir}/src/librrd.la

tune_legacy_aberrant_SOURCES = \
	test_tune_legacy_aberrant.c

tune_legacy_aberrant_LDADD = \
	${top_builddir}/src/librrd.la

# Compile the real RADOS read path against a local mock; no Ceph service needed.
EXTRA_DIST += mock-rados/rados/librados.h security-rados-bounds
if !MINGW_W64
if HAVE_STATIC_LIBRRD
if BUILD_LIBRADOS
script_tests += security-rados-bounds
check_PROGRAMS += mock-rados-bounds
mock_rados_bounds_SOURCES = test_rrd_rados_bounds.c src-rrd_open.c
mock_rados_bounds_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/mock-rados
mock_rados_bounds_LDFLAGS = -static
mock_rados_bounds_LDADD = ${top_builddir}/src/librrd.la
endif
endif
endif

if HAVE_STATIC_LIBRRD
check_PROGRAMS += hw-failures rpn-failures
TESTS += hw-failures rpn-failures
hw_failures_SOURCES = test_hw_failures.c
hw_failures_LDFLAGS = -static
hw_failures_LDADD = ${top_builddir}/src/librrd.la
if BUILD_RRDRESTORE
check_PROGRAMS += restore-text
TESTS += restore-text
restore_text_SOURCES = test_restore_text.c
restore_text_LDFLAGS = -static
restore_text_LDADD = ${top_builddir}/src/librrd.la
endif
rpn_failures_SOURCES = test_rpn_failures.c
rpn_failures_LDFLAGS = -static
rpn_failures_LDADD = ${top_builddir}/src/librrd.la
check_PROGRAMS += fetch-empty-security
TESTS += fetch-empty-security
fetch_empty_security_SOURCES = test_fetch_empty_security.c
fetch_empty_security_LDFLAGS = -static
fetch_empty_security_LDADD = ${top_builddir}/src/librrd.la
endif

# Linked against the shared librrd on purpose: it fails to link when a
# function of the installed rrd_client.h is not exported.
if BUILD_RRDCACHED
check_PROGRAMS += rrd-client-api
endif
rrd_client_api_SOURCES = test_rrd_client_api.c
rrd_client_api_LDADD = ${top_builddir}/src/librrd.la

if BUILD_RRDCACHED
if HAVE_STATIC_LIBRRD
check_PROGRAMS += daemon-fetch-security
TESTS += daemon-fetch-security
daemon_fetch_security_SOURCES = test_daemon_fetch_security.c
daemon_fetch_security_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src \
    -DVERSION='"$(VERSION)"' -DLOCALSTATEDIR='"$(localstatedir)"'
daemon_fetch_security_LDFLAGS = -static
daemon_fetch_security_LDADD = $(top_builddir)/src/librrd.la \
    $(top_builddir)/src/librrdupd.la $(ALL_LIBS)
endif
endif

script_tests += security-restore-zero-row-cnt security-restore-malformed-row
EXTRA_DIST += security-restore-malformed-row.xml

CLEANFILES += security-header-counts.out
CLEANFILES += cached/*.rrd modify6-testbefore.dump modify6-testafter.dump \
	security-*.out security-fetch-daemon-arithmetic-cached.log
CLEANFILES += modify6-testafter2.dump \
	graph-cairo-error.out graph-cairo-error.err graph-cairo-error.probe \
	graph-cairo-error.png graph-cairo-error.pdf graph-cairo-error.eps
# with rrdcached over a unix socket the tests write into cached/; it also
# holds tracked files, so name the scratch files one by one
CLEANFILES += cached/graph1.output.out cached/rpn1.out cached/rpn1.output.out \
	cached/modify5-testa1-mod.dump.tmp cached/modify5-testa2-mod.dump.tmp \
	cached/modify6-testbefore.dump cached/modify6-testafter.dump \
	cached/modify6-testafter2.dump \
	cached/graph-cairo-error.out cached/graph-cairo-error.err \
	cached/graph-cairo-error.probe cached/graph-cairo-error.png \
	cached/graph-cairo-error.pdf cached/graph-cairo-error.eps

if HAVE_STATIC_LIBRRD
if BUILD_RRDGRAPH
check_PROGRAMS += graph-security
TESTS += graph-security
endif
endif
graph_security_SOURCES = test_graph_security.c
# Exercise internal graph functions that are not exported by the shared library.
graph_security_LDFLAGS = -static
graph_security_LDADD = ${top_builddir}/src/librrd.la

script_tests += security-tune-failures-window security-dump-failures-window \
	tune-reopen
CLEANFILES += first-last-error.out graph-incomplete-file.err

# the rrdcached test styles make cached/ in a separate build directory
distclean-local:
	if test "$(srcdir)" != "."; then rm -rf cached; fi

if HAVE_STATIC_LIBRRD
check_PROGRAMS += numeric-limits
TESTS += numeric-limits
numeric_limits_SOURCES = test_numeric_limits.c
numeric_limits_LDFLAGS = -static
numeric_limits_LDADD = ${top_builddir}/src/librrd.la
endif
