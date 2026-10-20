AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src

script_tests = modify1 modify2 modify3 modify4 modify5 \
	tune1 tune2 graph1 graph2 rpn1 \
	rpn2 rrdcreate dump-restore create-with-source-1 create-with-source-2 \
	create-with-source-3 create-with-source-4 create-with-source-and-mapping-1 create-from-template-1 dcounter1 \
	vformatter1 xport1 xport2 xport3 list1 \
	pdp-calc1 graph3 graph4 graph5 security-header-counts \
	security-fetch-arithmetic security-fetch-daemon-arithmetic

TESTS = $(script_tests) compat-cloexec rrd-open-security

script_tests += security-rrdcached-response-bounds

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
	xport1.json.output xport1.xml.output xport2 xport3 graph3 graph4 graph5 \
	pdp-calc1 pdp-calc1-1-avg-60.output pdp-calc1-1-avg-300.output pdp-calc1-1-max-300.output

# NB: AM_TESTS_ENVIRONMENT not available until automake 1.12
AM_TESTS_ENVIRONMENT = \
	BASEDIR=${abs_srcdir} ; export BASEDIR ; \
	BUILDDIR=${abs_builddir} ; export BUILDDIR ; \
	TOP_BUILDDIR=${abs_top_builddir} ; export TOP_BUILDDIR ;

CLEANFILES = *.rrd \
	ct.out dur.out graph1.output.out graph2.output.out \
	graph4.pdf graph5-full.pdf graph5-clipped.pdf graph5-angle.pdf \
	modify5-testa1-mod.dump modify5-testa2-mod.dump \
	modify5-testa1-mod.dump.tmp modify5-testa2-mod.dump.tmp \
	rpn1.out rpn1.output.out

check_PROGRAMS = \
	compat-cloexec \
	rrd-open-security

compat_cloexec_SOURCES = \
	test_compat-cloexec.c \
	${top_srcdir}/src/compat-cloexec.c \
	${top_srcdir}/src/compat-cloexec.h

rrd_open_security_SOURCES = \
	test_rrd_open_security.c

rrd_open_security_LDADD = \
	${top_builddir}/src/librrd.la

# Compile the real RADOS read path against a local mock; no Ceph service needed.
EXTRA_DIST += mock-rados/rados/librados.h security-rados-bounds
if !MINGW_W64
if HAVE_STATIC_LIBRRD
if BUILD_LIBRADOS
script_tests += security-rados-bounds
check_PROGRAMS += mock-rados-bounds
mock_rados_bounds_SOURCES = test_rrd_rados_bounds.c ${top_srcdir}/src/rrd_open.c
mock_rados_bounds_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/mock-rados
mock_rados_bounds_LDFLAGS = -static
mock_rados_bounds_LDADD = ${top_builddir}/src/librrd.la
endif
endif
endif

if HAVE_STATIC_LIBRRD
check_PROGRAMS += fetch-empty-security
TESTS += fetch-empty-security
fetch_empty_security_SOURCES = test_fetch_empty_security.c
fetch_empty_security_LDFLAGS = -static
fetch_empty_security_LDADD = ${top_builddir}/src/librrd.la
endif

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

script_tests += security-tune-failures-window security-dump-failures-window
