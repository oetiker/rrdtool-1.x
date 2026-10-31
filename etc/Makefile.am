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
