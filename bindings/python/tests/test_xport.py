import os
import shutil
import sys
import tempfile
import unittest

import rrdtool


class TestXport(unittest.TestCase):
    """xport must not keep references to the objects it returns, or the
    whole result leaks on every call (#1381)."""

    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='rrdtool-xport-')
        self.rrd = os.path.join(self.dir, 'a.rrd')
        rrdtool.create(self.rrd, '--start', '1200000000', '--step', '300',
                       'DS:a:GAUGE:600:U:U', 'RRA:LAST:0.5:1:10')
        rrdtool.update(self.rrd, '1200000300:1', '1200000600:2')

    def tearDown(self):
        shutil.rmtree(self.dir)

    def test_result_refcounts(self):
        result = rrdtool.xport('--start', '1200000000',
                               '--end', '1200000900',
                               'DEF:a=%s:a:LAST' % self.rrd, 'XPORT:a:a')
        held = {'value': []}
        expected = sys.getrefcount(held['value'])
        self.assertEqual(sys.getrefcount(result['meta']), expected)
        self.assertEqual(sys.getrefcount(result['data']), expected)
        self.assertEqual(sys.getrefcount(result['meta']['legend']), expected)
        self.assertEqual(sys.getrefcount(result['meta']['start']), expected)


if __name__ == '__main__':
    unittest.main()
