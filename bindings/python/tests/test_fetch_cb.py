import base64
import math
import os
import rrdtool
import unittest
import sys

PY3 = sys.version_info >= (3, 0)


class TestFetchCallback(unittest.TestCase):
    if not PY3:
        def assertRaisesRegex(self, *args, **kwargs):
            return self.assertRaisesRegexp(*args, **kwargs)

    def setUp(self):
        self.graphv_args = [
            '-',
            '--title', 'Callback Demo',
            '--start', '1424540800',
            '--end', 'start+24h',
            '--lower-limit=0',
            '--interlaced',
            '--imgformat', 'PNG',
            '--width=450',
            'DEF:a=cb//extrainfo:a:AVERAGE',
            'DEF:b=cb//:b:AVERAGE',
            'DEF:c=cb//:c:AVERAGE',
            'LINE:a#00b6e4:a',
            'LINE:b#10b634:b',
            'LINE:c#503d14:c',
            'VDEF:av=a,AVERAGE',
            'PRINT:av:%8.6lf'
        ]

    def test_callback_return_type(self):
        """
        Test whether callback return type is checked correctly.
        The callback must always return a dict.
        """
        def my_callback(*args, **kwargs):
            return None

        rrdtool.register_fetch_cb(my_callback)

        self.assertRaisesRegex(
            rrdtool.OperationalError,
            'expected callback method to be a dict',
            rrdtool.graphv,
            self.graphv_args
        )

    def test_callback_args(self):
        """
        Test whether all required arguments are passed in kwargs.
        """
        def my_callback(*args, **kwargs):
            required_args = ('filename', 'cf', 'start', 'end', 'step')
            for k in required_args:
                self.assertIn(k, kwargs)

            items = int((kwargs['end'] - kwargs['start']) / kwargs['step'])
            return {
                'start': kwargs['start'],
                'step': 300,
                'data': {
                    'a': [math.sin(x / 200) for x in range(0, items)],
                    'b': [math.cos(x / 200) for x in range(10, items)],
                    'c': [math.sin(x / 100) for x in range(100, items)],
                }
            }

        rrdtool.register_fetch_cb(my_callback)
        rrdtool.graphv(*self.graphv_args)


if __name__ == '__main__':
    unittest.main()