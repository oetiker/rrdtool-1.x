## Process this file with automake to produce Makefile.in

# Built only with --enable-fuzzing (see configure.ac); never part of the
# default build and never wired into check_PROGRAMS/TESTS, since it needs
# a libFuzzer-capable compiler (clang) and is meant to run under a fuzzing
# engine, not the regular test suite.

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CFLAGS = @CFLAGS@

EXTRA_DIST = corpus

noinst_PROGRAMS = rrd_open_fuzzer

rrd_open_fuzzer_SOURCES = rrd_open_fuzzer.c
rrd_open_fuzzer_DEPENDENCIES = $(top_builddir)/src/librrd.la
rrd_open_fuzzer_LDADD = $(top_builddir)/src/librrd.la

# -fsanitize=fuzzer supplies libFuzzer's main()/driver and is what makes
# this binary a fuzz target in the first place, so it belongs here rather
# than in caller-supplied CFLAGS/LDFLAGS (those are still used, unmodified,
# for whichever memory-safety sanitizers the caller wants -- see the
# fuzz-smoke-test CI job).
rrd_open_fuzzer_LDFLAGS = -fsanitize=fuzzer
