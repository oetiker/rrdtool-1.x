EXTRA_DIST = rrdcached-default-redhat rrdcached-init-redhat rrdcached-default-lsb.in \
	rrdcached-init-lsb rrdcached.socket.in rrdcached.service.in

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	rrdcached.socket \
	rrdcached.service
endif