#!/bin/perl -w
$ENV{PATH}="/usr/ucb";
use strict;   
use RRDp;     
my $rrdfile='/tmp/test.rrd';
RRDp::start '/home/oetiker/data/projects/AABN-rrdtool/src/rrdtool';
print grep /rrdtool/,`ps au`;
print grep /rrdtool/,`ps au`;
my $i=0;
while ($i<1000) {
 RRDp::cmd 'info /tmp/test.rrd';
 $_ = RRDp::read;
 $i++;
}
$_ = RRDp::end;
print grep /rrdtool/,`ps au`;
