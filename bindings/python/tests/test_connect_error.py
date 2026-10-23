import os
import shutil
import tempfile
import unittest

import rrdtool

# no daemon listens here; connecting fails with "No such file or directory"
NO_DAEMON = 'unix:/nonexistent-rrdtool-test/rrdcached.sock'


class TestConnectError(unittest.TestCase):
    """A command sent to an rrdcached that cannot be reached must raise
    OperationalError. librrd reports some connect failures with a positive
    error number instead of -1, and update() used to return None for them."""

    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='rrdtool-connect-')
        self.rrd = os.path.join(self.dir, 'a.rrd')
        rrdtool.create(self.rrd, '--start', '1200000000', '--step', '300',
                       'DS:a:GAUGE:600:U:U', 'RRA:LAST:0.5:1:10')
        self.env_addr = os.environ.pop('RRDCACHED_ADDRESS', None)

    def tearDown(self):
        if self.env_addr is not None:
            os.environ['RRDCACHED_ADDRESS'] = self.env_addr
        else:
            os.environ.pop('RRDCACHED_ADDRESS', None)
        shutil.rmtree(self.dir)

    def assert_connect_error(self, call, *args):
        with self.assertRaises(rrdtool.OperationalError) as ctx:
            call(*args)
        self.assertIn('Unable to connect to rrdcached', str(ctx.exception))

    def test_update(self):
        self.assert_connect_error(rrdtool.update, '--daemon', NO_DAEMON,
                                  self.rrd, '1200000300:1')
        # nothing was written locally either
        self.assertEqual(rrdtool.last(self.rrd), 1200000000)

    def test_update_from_environment(self):
        os.environ['RRDCACHED_ADDRESS'] = NO_DAEMON
        self.assert_connect_error(rrdtool.update, self.rrd, '1200000300:1')

    def test_xport(self):
        self.assert_connect_error(rrdtool.xport, '--daemon', NO_DAEMON,
                                  '--start', '1200000000',
                                  '--end', '1200000900',
                                  'DEF:a=%s:a:LAST' % self.rrd, 'XPORT:a')

    def test_flushcached(self):
        self.assert_connect_error(rrdtool.flushcached, '--daemon', NO_DAEMON,
                                  self.rrd)

    def test_tune(self):
        self.assert_connect_error(rrdtool.tune, self.rrd, '--daemon',
                                  NO_DAEMON, '--heartbeat', 'a:900')


if __name__ == '__main__':
    unittest.main()
