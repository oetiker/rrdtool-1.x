TESTS = modify1 modify2 modify3 modify4 modify5 \
	tune1 tune2 graph1 rpn1 rpn2 \
	rrdcreate \
	dump-restore \
	create-with-source-1 create-with-source-2 create-with-source-3 \
	create-with-source-4 create-with-source-and-mapping-1 \
	create-from-template-1 dcounter1 vformatter1 xport1 list1 \
	pdp-calc1

EXTRA_DIST = Makefile.am \
	functions $(TESTS) \
	modify-test1.create.dump modify-test1.mod1.dump \
	modify2-testa-create.dump modify2-testb-mod1.dump modify2-testc-mod1.dump \
	modify-test3.create.dump modify-test3.mod1.dump \
	modify4-testa-create.dump modify4-testa1-create.dump modify4-testa2-create.dump \
	modify5-testa1-create.dump modify5-testa2-create.dump \
	rrdcreate \
	tune1-testa-mod1.dump tune1-testa-mod2.dump tune1-testorg.dump \
	tune2-testa-mod1.dump tune2-testorg.dump \
	valgrind-supressions dcounter1 dcounter1.output graph1.output vformatter1 rpn1.output rpn2.output \
	xport1.json.output xport1.xml.output \
	pdp-calc1 pdp-calc1-1-avg-60.output pdp-calc1-1-avg-300.output pdp-calc1-1-max-300.output

# NB: AM_TESTS_ENVIRONMENT not available until automake 1.12
AM_TESTS_ENVIRONMENT = \
	BASEDIR=${abs_srcdir} ; export BASEDIR ; \
	BUILDDIR=${abs_builddir} ; export BUILDDIR ; \
	TOP_BUILDDIR=${abs_top_builddir} ; export TOP_BUILDDIR ;

CLEANFILES = *.rrd \
	ct.out dur.out graph1.output.out \
	modify5-testa1-mod.dump modify5-testa2-mod.dump \
	modify5-testa1-mod.dump.tmp modify5-testa2-mod.dump.tmp \
	rpn1.out rpn1.output.out
