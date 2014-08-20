TESTS = modify1 modify2 modify3 modify4 modify5 \
	tune1 tune2 rpn1 \
	rrdcreate \
	dump-restore
EXTRA_DIST = Makefile.am \
	alltests functions \
	modify1 modify-test1.create.dump modify-test1.mod1.dump \
	modify2 modify2-testa-create.dump modify2-testb-mod1.dump modify2-testc-mod1.dump \
	modify3 modify-test3.create.dump modify-test3.mod1.dump \
	modify4 modify4-testa-create.dump modify4-testa1-create.dump modify4-testa2-create.dump \
	modify5 modify5-testa1-create.dump modify5-testa2-create.dump \
	rrdcreate \
	tune1 tune1-testa-mod1.dump tune1-testa-mod2.dump tune1-testorg.dump \
	tune2 tune2-testa-mod1.dump tune2-testorg.dump \
	valgrind-supressions
# NB: AM_TESTS_ENVIRONMENT not available until automake 1.12
TESTS_ENVIRONMENT = \
	BASEDIR=${abs_srcdir} ; export BASEDIR ; \
	BUILDDIR=${abs_builddir} ; export BUILDDIR ; \
	TOP_BUILDDIR=${abs_top_builddir} ; export TOP_BUILDDIR ;
CLEANFILES = *.rrd \
	ct.out dur.out \
	modify5-testa1-mod.dump modify5-testa2-mod.dump 
