## Process this file with automake to produce Makefile.in
# preparation
# cp src/*/*.c .
# cp src/*/*.h include

noinst_LTLIBRARIES  = librrd_freetype.la


librrd_freetype_la_SOURCES = \
	ahangles.c	ahglyph.c	ahglobal.c	ahhint.c	ahmodule.c\
	cff.c ftglyph.c winfnt.c\
	ftcalc.c	fttrigon.c	ftobjs.c	ftstream.c	ftlist.c\
	ftoutln.c	ftnames.c	ftextend.c	ftlru.c		ftcmanag.c\
	ftcglyph.c	ftcchunk.c	ftcimage.c	ftcsbits.c	pcfutil.c\
	pcfread.c	pcfdriver.c	psobjs.c	psauxmod.c	t1decode.c\
	psmodule.c	ftraster.c	ftrend1.c	ttload.c	ttcmap.c\
	sfobjs.c	sfdriver.c	ttsbit.c	ttpost.c	ftgrays.c\
	ftsmooth.c	ttdriver.c	ttpload.c	ttgload.c	ttobjs.c\
	ttinterp.c	t1parse.c	t1load.c	t1objs.c	t1driver.c\
	t1gload.c	t1afm.c		cidparse.c	cidload.c	cidobjs.c\
	cidriver.c	cidgload.c	ftinit.c	ftsystem.c

EXTRA_DIST= README ChangeLog cffdrivr.c      cffparse.c      cffload.c       cffobjs.c       cffgload.c

INCLUDES = -I$(srcdir)/include
