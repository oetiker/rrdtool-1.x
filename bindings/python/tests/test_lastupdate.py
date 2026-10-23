import os
import shutil
import socket
import subprocess
import tempfile
import time
import unittest

import rrdtool


def _rrdcached_binary():
    path = os.environ.get('RRDCACHED')
    if path:
        return path
    return shutil.which('rrdcached')


class TestLastupdate(unittest.TestCase):
    """lastupdate must honour --daemon/-d and RRDCACHED_ADDRESS like
    `rrdtool lastupdate` does (#1293). Set RRDCACHED to the rrdcached
    binary to run the daemon tests."""

    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='rrdtool-lastupdate-')
        self.base = os.path.join(self.dir, 'base')
        os.makedirs(os.path.join(self.base, 'sub'))
        self.rrd = os.path.join(self.base, 'sub', 'c.rrd')
        rrdtool.create(self.rrd, '--start', '1699999990', '--step', '10',
                       'DS:a:GAUGE:20:U:U', 'DS:b:GAUGE:20:U:U',
                       'RRA:LAST:0.5:1:10')
        self.daemon = None
        self.cwd = os.getcwd()
        self.env_addr = os.environ.pop('RRDCACHED_ADDRESS', None)

    def tearDown(self):
        os.chdir(self.cwd)
        if self.env_addr is not None:
            os.environ['RRDCACHED_ADDRESS'] = self.env_addr
        else:
            os.environ.pop('RRDCACHED_ADDRESS', None)
        if self.daemon is not None:
            self.daemon.terminate()
            self.daemon.wait()
        shutil.rmtree(self.dir)

    def start_daemon(self):
        binary = _rrdcached_binary()
        if not binary:
            self.skipTest('no rrdcached binary (set RRDCACHED)')
        s = socket.socket()
        s.bind(('localhost', 0))
        port = s.getsockname()[1]
        s.close()
        addr = 'localhost:%d' % port
        # -w 3600: nothing is written to disk unless a client flushes
        self.daemon = subprocess.Popen(
            [binary, '-g', '-p', os.path.join(self.dir, 'rrdcached.pid'),
             '-l', addr, '-b', self.base, '-B',
             '-w', '3600', '-z', '1'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(50):
            try:
                socket.create_connection(('localhost', port), 0.1).close()
                break
            except OSError:
                time.sleep(0.1)
        else:
            self.fail('rrdcached did not start')
        rrdtool.update('--daemon', addr, 'sub/c.rrd',
                       '1700000010:1:2', '1700000020:3:4.5')
        # sub/c.rrd does not exist relative to the current directory
        os.chdir(self.dir)
        return addr

    def check(self, result):
        self.assertEqual(int(result['date'].timestamp()), 1700000020)
        self.assertEqual(result['ds'], {'a': 3.0, 'b': 4.5})

    def test_local(self):
        rrdtool.update(self.rrd, '1700000010:1:2', '1700000020:3:4.5')
        self.check(rrdtool.lastupdate(self.rrd))

    def test_daemon_long_option_after_file(self):
        addr = self.start_daemon()
        self.check(rrdtool.lastupdate('sub/c.rrd', '--daemon', addr))

    def test_daemon_short_option_before_file(self):
        addr = self.start_daemon()
        self.check(rrdtool.lastupdate('-d', addr, 'sub/c.rrd'))

    def test_daemon_equals_form(self):
        addr = self.start_daemon()
        self.check(rrdtool.lastupdate('--daemon=' + addr, 'sub/c.rrd'))

    def test_daemon_from_environment(self):
        addr = self.start_daemon()
        os.environ['RRDCACHED_ADDRESS'] = addr
        self.check(rrdtool.lastupdate('sub/c.rrd'))

    def test_daemon_missing_file(self):
        addr = self.start_daemon()
        with self.assertRaises(rrdtool.OperationalError):
            rrdtool.lastupdate('--daemon', addr, 'sub/missing.rrd')

    def test_option_without_argument(self):
        with self.assertRaises(rrdtool.OperationalError):
            rrdtool.lastupdate(self.rrd, '--daemon')

    def test_unknown_option(self):
        with self.assertRaises(rrdtool.OperationalError):
            rrdtool.lastupdate('--bogus', self.rrd)

    def test_two_files(self):
        with self.assertRaises(rrdtool.OperationalError):
            rrdtool.lastupdate(self.rrd, self.rrd)

    def test_missing_filename(self):
        with self.assertRaises(rrdtool.ProgrammingError):
            rrdtool.lastupdate()


if __name__ == '__main__':
    unittest.main()
