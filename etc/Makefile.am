EXTRA_DIST = rrdcached-default-redhat rrdcached-init-redhat rrdcached-default-lsb.in \
	rrdcached-init-lsb rrdcached.socket.in rrdcached.service.in

# the units start rrdcached, so only install them when it is built
if BUILD_RRDCACHED
if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	rrdcached.socket \
	rrdcached.service
endif
endif

# made here rather than by configure so @bindir@ is fully expanded
rrdcached.service: rrdcached.service.in Makefile
	$(AM_V_GEN)sed -e 's|@bindir[@]|$(bindir)|g' $(srcdir)/rrdcached.service.in > $@

rrdcached.socket: rrdcached.socket.in Makefile
	$(AM_V_GEN)cp $(srcdir)/rrdcached.socket.in $@

CLEANFILES = rrdcached.service rrdcached.socket
