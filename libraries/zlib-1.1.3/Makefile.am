## Process this file with automake to produce Makefile.in

#AUTOMAKE_OPTIONS        = foreign

# read local config files
#ACLOCAL_M4 = $(top_srcdir)/config/aclocal.m4
#AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/config

EXTRA_DIST= ChangeLog FAQ INDEX README README.rrdtool algorithm.txt zlib.dsp zlib.dsw zlib.3 

noinst_LTLIBRARIES = librrd_z.la

librrd_z_la_SOURCES =	\
	adler32.c	\
	compress.c	\
	crc32.c		\
	deflate.c	\
	gzio.c		\
	infblock.c	\
	infcodes.c	\
	inffast.c	\
	inflate.c	\
	inftrees.c	\
	infutil.c	\
	trees.c		\
	uncompr.c	\
	zutil.c		\
	deflate.h   infcodes.h  inffixed.h  infutil.h   zconf.h     zutil.h	\
	infblock.h  inffast.h   inftrees.h  trees.h     zlib.h
