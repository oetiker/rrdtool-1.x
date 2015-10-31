#! /usr/bin/python

from __future__ import print_function

import locale
import random
import time
from math import sin

import rrdtool

start = int(time.time())
rrd = 'random.rrd'

rrdtool.create(rrd,
               '--start', str(start-1),
               '--step', '300',
               'DS:a:GAUGE:600:U:U',
               'DS:b:GAUGE:600:U:U',
               'RRA:AVERAGE:0.5:1:200')

for t in range(start, start+200*300, 300):
    rrdtool.update(rrd, '%s:%s:%s' % (
        t,
        random.randint(0, 100),
        sin(t/800.)*50+50))

locale.setlocale(locale.LC_ALL, '')  # enable localisation

rrdtool.graph('stripes.png',
              '--title', 'Stripes Demo',
              '--start', str(start),
              '--end', str(start+400*60),
              '--interlaced',
              '--imgformat', 'PNG',
              '--width=450',
              'DEF:a=%s:a:AVERAGE' % rrd,
              'DEF:b=%s:b:AVERAGE' % rrd,
              'CDEF:alpha=TIME,1200,%,600,LT,a,UNKN,IF',
              'CDEF:beta=TIME,1200,%,600,GE,b,UNKN,IF',
              'AREA:alpha#0022e9:alpha',
              'AREA:beta#00b674:beta',
              'LINE1:b#ff4400:beta envelope\\c',
              'COMMENT:\\s',
              'COMMENT:alpha=TIME,1200,%,600,LT,a,UNKN,IF',
              'COMMENT:beta=TIME,1200,%,600,GE,b,UNKN,IF\\j')

print('This script has created stripes.png in the current directory')
print('This demonstrates the use of the TIME and % RPN operators')
